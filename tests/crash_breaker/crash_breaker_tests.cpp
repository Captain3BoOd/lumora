// Tests for core/lifecycle/crash_breaker. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/crash_breaker_ref.py, an independent Python
// implementation. Regenerate it with: python3 tools/crash_breaker_ref.py
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "crash_breaker.h"

using namespace lumora::lifecycle;

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

// BEGIN GENERATED REFERENCE (tools/crash_breaker_ref.py)
constexpr std::uint32_t kRefLogLen = 4, kRefLogVal = 6, kRefNowMax = 8, kRefWinMax = 4;
constexpr std::uint32_t kRefDLen = 3, kRefDVal = 2, kRefDNow = 4, kRefDWin = 3, kRefDMaxC = 4;
constexpr std::uint32_t kRefPMax = 18;
const std::uint64_t kRefPWindows[] = {0x0000000000000000ull, 0x0000000000000001ull, 0xFFFFFFFFFFFFFFFFull};
constexpr std::size_t kRefWideCases = 30000;
const std::uint64_t kRefNows[] = {0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000032ull, 0x0000000000000064ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull};
const std::uint64_t kRefWindows[] = {0x0000000000000001ull, 0x0000000000000002ull, 0x000000000000000Aull, 0x0000000000000064ull, 0x0000000100000000ull, 0xFFFFFFFFFFFFFFFFull};
const std::uint32_t kRefCounts[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 100u, 0xFFFFFFFFu};
constexpr std::uint64_t kRefSeed = 0x9E3779B97F4A7C15ull;
const std::uint64_t kRefDigest_record_small = 0xA89819A037577785ull;
const std::uint64_t kRefDigest_recent_small = 0x3BC5CEAB2569D565ull;
const std::uint64_t kRefDigest_decide_small = 0xC06CEF6FF699A1DAull;
const std::uint64_t kRefDigest_valid = 0xE9B2C890A7516867ull;
const std::uint64_t kRefDigest_record_wide = 0xB31551986E4797FEull;
const std::uint64_t kRefDigest_recent_wide = 0x8A9DF951928F94E1ull;
const std::size_t kRefCases_record_small = 100836;
const std::size_t kRefCases_recent_small = 151254;
const std::size_t kRefCases_decide_small = 504300;
const std::size_t kRefCases_valid = 1083;
// END GENERATED REFERENCE

// FNV-1a 64, the same byte streams as tools/crash_breaker_ref.py.
struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void num(std::uint64_t v, int n) {
    for (int i = 0; i < n; ++i) byte(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  }
  void log(const CrashLog& l) {
    num(l.count, 4);
    for (std::uint32_t i = 0; i < kMaxCrashes; ++i) num(l.times_ms[i], 8);
  }
};

// xorshift64*, the same as the reference.
struct Rng {
  std::uint64_t s;
  std::uint64_t next() {
    std::uint64_t x = s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s = x;
    return x * 0x2545F4914F6CDD1Dull;
  }
};

CrashLog make_log(const std::vector<std::uint64_t>& v) {
  CrashLog l;
  l.count = static_cast<std::uint32_t>(v.size());
  for (std::size_t i = 0; i < v.size() && i < kMaxCrashes; ++i) l.times_ms[i] = v[i];
  return l;
}

Policy policy(std::uint32_t max_crashes, std::uint64_t window, std::uint32_t backend_crashes) {
  Policy p;
  p.max_crashes = max_crashes;
  p.window_ms = window;
  p.backend_crashes = backend_crashes;
  return p;
}

// Every sequence of up to `length` values in 0..val, shortest first.
std::vector<std::vector<std::uint64_t>> all_logs(std::uint32_t length, std::uint32_t val) {
  std::vector<std::vector<std::uint64_t>> out;
  out.push_back({});
  std::vector<std::vector<std::uint64_t>> level = {{}};
  for (std::uint32_t n = 1; n <= length; ++n) {
    std::vector<std::vector<std::uint64_t>> next;
    for (const auto& p : level)
      for (std::uint32_t v = 0; v <= val; ++v) {
        auto q = p;
        q.push_back(v);
        next.push_back(q);
      }
    for (const auto& q : next) out.push_back(q);
    level = next;
  }
  return out;
}

// ---- Naive oracles, sharing no code with the implementation ----------------------------------------------------
bool naive_inside(std::uint64_t t, std::uint64_t now, std::uint64_t window) {
  if (window == 0) return false;
  if (t > now) return true;
  return now - t < window;
}

std::uint32_t naive_recent(const CrashLog& l, std::uint64_t now, std::uint64_t window) {
  std::uint32_t c = 0;
  std::uint32_t n = l.count;
  if (n > 16) n = 16;
  for (std::uint32_t i = 0; i < n; ++i) {
    if (naive_inside(l.times_ms[i], now, window)) c = c + 1;
  }
  return c;
}

// A vector-based record: erase what is outside, drop the front when full, push the new time, pad with zeros.
CrashLog naive_record(const CrashLog& in, std::uint64_t now, std::uint64_t window) {
  std::vector<std::uint64_t> v;
  std::uint32_t n = in.count > 16 ? 16 : in.count;
  for (std::uint32_t i = 0; i < n; ++i) v.push_back(in.times_ms[i]);
  for (std::size_t i = 0; i < v.size();) {
    if (!naive_inside(v[i], now, window)) {
      v.erase(v.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
  if (v.size() == 16) v.erase(v.begin());
  v.push_back(now);
  CrashLog out;
  out.count = static_cast<std::uint32_t>(v.size());
  for (std::size_t i = 0; i < 16; ++i) out.times_ms[i] = i < v.size() ? v[i] : 0;
  return out;
}

bool same(const CrashLog& a, const CrashLog& b) {
  if (a.count != b.count) return false;
  for (std::uint32_t i = 0; i < kMaxCrashes; ++i)
    if (a.times_ms[i] != b.times_ms[i]) return false;
  return true;
}

}  // namespace

// The provisional defaults of docs/android-lifecycle.md section 10.7 live here only. Update this one case when
// they are pinned at M2; every other test passes explicit policies.
TEST_CASE("provisional default policy", "[breaker][defaults]") {
  const Policy p;
  CHECK(p.max_crashes == 3u);
  CHECK(p.window_ms == 86400000ull);
  CHECK(p.backend_crashes == 2u);
  CHECK(valid(p));
  CHECK(kMaxCrashes == 16u);
}

TEST_CASE("valid() bounds", "[breaker]") {
  CHECK(valid(policy(1, 1, 1)));
  CHECK(valid(policy(16, 1, 16)));
  CHECK(valid(policy(16, kMax, 1)));
  CHECK_FALSE(valid(policy(0, 1, 1)));
  CHECK_FALSE(valid(policy(0, 1, 0)));
  CHECK_FALSE(valid(policy(17, 1, 1)));
  CHECK_FALSE(valid(policy(0xFFFFFFFFu, 1, 1)));
  CHECK_FALSE(valid(policy(3, 1, 0)));
  CHECK_FALSE(valid(policy(3, 1, 4)));
  CHECK(valid(policy(3, 1, 3)));
  CHECK_FALSE(valid(policy(3, 0, 2)));
  CHECK_FALSE(valid(policy(16, 1, 17)));
}

TEST_CASE("window edges", "[breaker]") {
  // At now 1000 the ages are 101, 100 and 99. A crash is inside when age < window.
  const CrashLog l = make_log({899, 900, 901});
  CHECK(recent_crashes(l, 1000, 102) == 3u);
  CHECK(recent_crashes(l, 1000, 101) == 2u);
  CHECK(recent_crashes(l, 1000, 100) == 1u);
  CHECK(recent_crashes(l, 1000, 99) == 0u);
  CHECK(recent_crashes(l, 1000, 1) == 0u);
  CHECK(recent_crashes(make_log({1000}), 1000, 1) == 1u);  // age 0
  CHECK(recent_crashes(l, 1000, 0) == 0u);
  CHECK(recent_crashes(make_log({1000}), 1000, 0) == 0u);
}

TEST_CASE("a backwards clock does not hide crashes", "[breaker]") {
  const CrashLog l = make_log({500, 2000, 3000});
  CHECK(recent_crashes(l, 1000, 10) == 2u);  // 2000 and 3000 are in the future: age 0
  CHECK(recent_crashes(l, 0, 1) == 3u);
  CHECK(recent_crashes(l, 1000, 0) == 0u);  // a zero window counts nothing, future entries included
  CrashLog m = l;
  REQUIRE(record_crash(&m, 1000, policy(3, 10, 2)));
  REQUIRE(m.count == 3u);
  CHECK(m.times_ms[0] == 2000u);
  CHECK(m.times_ms[1] == 3000u);
  CHECK(m.times_ms[2] == 1000u);  // appended even though older than the last entry; never sorted
}

TEST_CASE("UINT64_MAX times", "[breaker]") {
  const CrashLog l = make_log({0, kMax - 1, kMax});
  CHECK(recent_crashes(l, kMax, 1) == 1u);
  CHECK(recent_crashes(l, kMax, 2) == 2u);
  CHECK(recent_crashes(l, kMax, 3) == 2u);  // ages kMax, 1 and 0
  CHECK(recent_crashes(l, kMax, kMax) == 2u);   // age of 0 is kMax, not below kMax
  CHECK(recent_crashes(l, 0, kMax) == 3u);      // clock at 0: everything is in the future or now
  CHECK(recent_crashes(l, kMax - 1, 1) == 2u);  // kMax is in the future
  CrashLog m = l;
  REQUIRE(record_crash(&m, kMax, policy(3, 2, 2)));
  REQUIRE(m.count == 3u);
  CHECK(m.times_ms[0] == kMax - 1);
  CHECK(m.times_ms[1] == kMax);
  CHECK(m.times_ms[2] == kMax);
  CrashLog z;
  REQUIRE(record_crash(&z, 0, policy(3, kMax, 2)));
  CHECK(z.count == 1u);
  CHECK(z.times_ms[0] == 0u);
}

TEST_CASE("record_crash pruning, capacity and canonical slots", "[breaker]") {
  const Policy p = policy(3, 100, 2);
  SECTION("empty log") {
    CrashLog l;
    REQUIRE(record_crash(&l, 5, p));
    CHECK(l.count == 1u);
    CHECK(l.times_ms[0] == 5u);
    for (std::uint32_t i = 1; i < kMaxCrashes; ++i) CHECK(l.times_ms[i] == 0u);
  }
  SECTION("pruning keeps order, edge at age window") {
    CrashLog l = make_log({899, 900, 950, 1000});
    REQUIRE(record_crash(&l, 1000, p));
    REQUIRE(l.count == 3u);
    CHECK(l.times_ms[0] == 950u);
    CHECK(l.times_ms[1] == 1000u);
    CHECK(l.times_ms[2] == 1000u);
  }
  SECTION("stale slots above count are zeroed") {
    CrashLog l = make_log({1, 2, 3});
    for (std::uint32_t i = 3; i < kMaxCrashes; ++i) l.times_ms[i] = 777;
    REQUIRE(record_crash(&l, 1000, p));
    CHECK(l.count == 1u);
    CHECK(l.times_ms[0] == 1000u);
    for (std::uint32_t i = 1; i < kMaxCrashes; ++i) CHECK(l.times_ms[i] == 0u);
  }
  SECTION("a full log of recent entries drops entry 0") {
    std::vector<std::uint64_t> v;
    for (std::uint64_t i = 0; i < 16; ++i) v.push_back(900 + i);
    CrashLog l = make_log(v);
    REQUIRE(record_crash(&l, 1000, p));
    REQUIRE(l.count == 16u);
    CHECK(l.times_ms[0] == 901u);
    CHECK(l.times_ms[14] == 915u);
    CHECK(l.times_ms[15] == 1000u);
  }
  SECTION("15 entries grow to 16 without dropping") {
    std::vector<std::uint64_t> v;
    for (std::uint64_t i = 0; i < 15; ++i) v.push_back(901 + i);
    CrashLog l = make_log(v);
    REQUIRE(record_crash(&l, 1000, p));
    REQUIRE(l.count == 16u);
    CHECK(l.times_ms[0] == 901u);
    CHECK(l.times_ms[15] == 1000u);
  }
  SECTION("a full log with old entries prunes first, nothing extra dropped") {
    std::vector<std::uint64_t> v;
    for (std::uint64_t i = 0; i < 16; ++i) v.push_back(i < 4 ? i : 950 + i);
    CrashLog l = make_log(v);
    REQUIRE(record_crash(&l, 1000, p));
    REQUIRE(l.count == 13u);
    CHECK(l.times_ms[0] == 954u);
    CHECK(l.times_ms[12] == 1000u);
    CHECK(l.times_ms[13] == 0u);
  }
  SECTION("corrupted count above 16 is clamped before pruning") {
    CrashLog l;
    for (std::uint32_t i = 0; i < kMaxCrashes; ++i) l.times_ms[i] = 950 + i;
    l.count = 17;
    REQUIRE(record_crash(&l, 1000, p));
    REQUIRE(l.count == 16u);
    CHECK(l.times_ms[0] == 951u);
    CHECK(l.times_ms[15] == 1000u);
    CrashLog m;
    for (std::uint32_t i = 0; i < kMaxCrashes; ++i) m.times_ms[i] = 1;
    m.count = 0xFFFFFFFFu;
    REQUIRE(record_crash(&m, 1000, p));
    CHECK(m.count == 1u);
    CHECK(m.times_ms[0] == 1000u);
  }
  SECTION("recent_crashes reads a corrupted count as 16") {
    CrashLog l;
    for (std::uint32_t i = 0; i < kMaxCrashes; ++i) l.times_ms[i] = 1000;
    l.count = 0xFFFFFFFFu;
    CHECK(recent_crashes(l, 1000, 10) == 16u);
    l.count = 17;
    CHECK(recent_crashes(l, 1000, 10) == 16u);
    l.count = 16;
    CHECK(recent_crashes(l, 1000, 10) == 16u);
    l.count = 15;
    CHECK(recent_crashes(l, 1000, 10) == 15u);
    l.count = 0;
    CHECK(recent_crashes(l, 1000, 10) == 0u);
  }
}

TEST_CASE("record_crash failure touches nothing", "[breaker]") {
  CrashLog l = make_log({1, 2, 3});
  l.times_ms[7] = 99;
  const CrashLog before = l;
  CHECK_FALSE(record_crash(&l, 10, policy(0, 100, 1)));
  CHECK(same(l, before));
  CHECK_FALSE(record_crash(&l, 10, policy(3, 0, 2)));
  CHECK(same(l, before));
  CHECK_FALSE(record_crash(&l, 10, policy(3, 100, 4)));
  CHECK(same(l, before));
  CHECK_FALSE(record_crash(&l, 10, policy(17, 100, 1)));
  CHECK(same(l, before));
  CHECK_FALSE(record_crash(nullptr, 10, policy(3, 100, 2)));
  CrashLog c = before;
  c.count = 40;
  const CrashLog cb = c;
  CHECK_FALSE(record_crash(&c, 10, policy(3, 0, 2)));
  CHECK(same(c, cb));  // a corrupted count is not repaired on failure
}

TEST_CASE("decide rules", "[breaker]") {
  const Policy p = policy(3, 100, 2);
  const std::uint64_t now = 1000;
  Decision d;
  d.action = Action::Disable;
  d.backend = Backend::Gles;
  SECTION("empty logs run on Vulkan") {
    REQUIRE(decide(nullptr, nullptr, now, p, true, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Vulkan);
    const CrashLog e;
    REQUIRE(decide(&e, &e, now, p, true, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Vulkan);
  }
  SECTION("one fewer than backend_crashes on Vulkan stays on Vulkan") {
    const CrashLog v = make_log({990});
    REQUIRE(decide(&v, nullptr, now, p, true, &d));
    CHECK(d.backend == Backend::Vulkan);
  }
  SECTION("backend_crashes on Vulkan moves to GLES") {
    const CrashLog v = make_log({990, 995});
    REQUIRE(decide(&v, nullptr, now, p, true, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Gles);
  }
  SECTION("GLES unavailable keeps Vulkan") {
    const CrashLog v = make_log({990, 995});
    REQUIRE(decide(&v, nullptr, now, p, false, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Vulkan);
  }
  SECTION("GLES crashes alone never move the backend") {
    const CrashLog g = make_log({990, 995});
    REQUIRE(decide(nullptr, &g, now, p, true, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Vulkan);
  }
  SECTION("total at max_crashes disables, both backends together") {
    const CrashLog v = make_log({990, 995});
    const CrashLog g = make_log({998});
    REQUIRE(decide(&v, &g, now, p, true, &d));
    CHECK(d.action == Action::Disable);
    CHECK(d.backend == Backend::Vulkan);
    d.action = Action::Run;
    d.backend = Backend::Gles;
    REQUIRE(decide(&v, &g, now, p, false, &d));
    CHECK(d.action == Action::Disable);
    CHECK(d.backend == Backend::Vulkan);
  }
  SECTION("disable on one backend alone") {
    const CrashLog v = make_log({990, 995, 996});
    REQUIRE(decide(&v, nullptr, now, p, true, &d));
    CHECK(d.action == Action::Disable);
    const CrashLog g = make_log({990, 995, 996});
    d.action = Action::Run;
    REQUIRE(decide(nullptr, &g, now, p, true, &d));
    CHECK(d.action == Action::Disable);
  }
  SECTION("crashes outside the window do not count") {
    const CrashLog v = make_log({1, 2, 3, 4, 5});
    REQUIRE(decide(&v, &v, now, p, true, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Vulkan);
  }
  SECTION("the window edge: age window_ms is outside, window_ms - 1 is inside") {
    const CrashLog out = make_log({900, 900, 900});
    REQUIRE(decide(&out, nullptr, now, p, true, &d));
    CHECK(d.action == Action::Run);
    const CrashLog in = make_log({901, 901, 901});
    REQUIRE(decide(&in, nullptr, now, p, true, &d));
    CHECK(d.action == Action::Disable);
  }
  SECTION("a policy with max_crashes 1 disables on the first crash, window 1") {
    const Policy q = policy(1, 1, 1);
    const CrashLog v = make_log({1000});
    REQUIRE(decide(&v, nullptr, now, q, true, &d));
    CHECK(d.action == Action::Disable);
    const CrashLog old = make_log({999});
    REQUIRE(decide(&old, nullptr, now, q, true, &d));
    CHECK(d.action == Action::Run);
  }
  SECTION("a 16-crash policy with a full log") {
    const Policy q = policy(16, 100, 16);
    std::vector<std::uint64_t> v;
    for (std::uint64_t i = 0; i < 15; ++i) v.push_back(950);
    const CrashLog l15 = make_log(v);
    REQUIRE(decide(&l15, nullptr, now, q, true, &d));
    CHECK(d.action == Action::Run);
    CHECK(d.backend == Backend::Vulkan);
    v.push_back(950);
    const CrashLog l16 = make_log(v);
    REQUIRE(decide(&l16, nullptr, now, q, true, &d));
    CHECK(d.action == Action::Disable);
    REQUIRE(decide(&l15, &l15, now, q, true, &d));
    CHECK(d.action == Action::Disable);  // 30 total with no overflow
  }
  SECTION("a corrupted count is read as 16") {
    CrashLog v;
    for (std::uint32_t i = 0; i < kMaxCrashes; ++i) v.times_ms[i] = 999;
    v.count = 0xFFFFFFFFu;
    REQUIRE(decide(&v, nullptr, now, policy(16, 100, 16), true, &d));
    CHECK(d.action == Action::Disable);
  }
  SECTION("UINT64_MAX now and window") {
    const CrashLog v = make_log({kMax, kMax, kMax});
    REQUIRE(decide(&v, nullptr, kMax, policy(3, 1, 2), true, &d));
    CHECK(d.action == Action::Disable);
    REQUIRE(decide(&v, nullptr, 0, policy(3, 1, 2), true, &d));
    CHECK(d.action == Action::Disable);  // in the future counts as age 0
  }
}

TEST_CASE("decide failure writes nothing", "[breaker]") {
  Decision d;
  d.action = Action::Disable;
  d.backend = Backend::Gles;
  CHECK_FALSE(decide(nullptr, nullptr, 10, policy(0, 100, 1), true, &d));
  CHECK(d.action == Action::Disable);
  CHECK(d.backend == Backend::Gles);
  CHECK_FALSE(decide(nullptr, nullptr, 10, policy(3, 0, 2), true, &d));
  CHECK_FALSE(decide(nullptr, nullptr, 10, policy(3, 100, 4), true, &d));
  CHECK(d.action == Action::Disable);
  CHECK(d.backend == Backend::Gles);
  CHECK_FALSE(decide(nullptr, nullptr, 10, policy(3, 100, 2), true, nullptr));
}

TEST_CASE("to_string", "[breaker]") {
  CHECK(std::strcmp(to_string(Action::Run), "Run") == 0);
  CHECK(std::strcmp(to_string(Action::Disable), "Disable") == 0);
  CHECK(std::strcmp(to_string(Backend::Vulkan), "Vulkan") == 0);
  CHECK(std::strcmp(to_string(Backend::Gles), "Gles") == 0);
  CHECK(std::strcmp(to_string(static_cast<Action>(2)), "unknown") == 0);
  CHECK(std::strcmp(to_string(static_cast<Action>(255)), "unknown") == 0);
  CHECK(std::strcmp(to_string(static_cast<Backend>(2)), "unknown") == 0);
  CHECK(std::strcmp(to_string(static_cast<Backend>(255)), "unknown") == 0);
}

TEST_CASE("valid() sweep matches the reference digest", "[breaker][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (std::uint32_t mc = 0; mc <= kRefPMax; ++mc)
    for (std::uint32_t bc = 0; bc <= kRefPMax; ++bc)
      for (std::uint64_t w : kRefPWindows) {
        const bool v = valid(policy(mc, w, bc));
        const bool expect = mc >= 1 && mc <= 16 && bc >= 1 && bc <= mc && w >= 1;
        if (v != expect) FAIL("valid mismatch: max " << mc << " backend " << bc << " window " << w);
        f.num(v ? 1 : 0, 1);
        ++n;
      }
  CHECK(n == kRefCases_valid);
  CHECK(f.h == kRefDigest_valid);
}

TEST_CASE("small logs: record_crash and recent_crashes match the oracle and the digest", "[breaker][sweep]") {
  const auto logs = all_logs(kRefLogLen, kRefLogVal);
  Fnv fr, fc;
  std::size_t nr = 0, nc = 0;
  for (const auto& v : logs) {
    const CrashLog l = make_log(v);
    for (std::uint32_t now = 0; now <= kRefNowMax; ++now) {
      for (std::uint32_t window = 1; window <= kRefWinMax; ++window) {
        CrashLog m = l;
        const bool ok = record_crash(&m, now, policy(3, window, 2));
        const CrashLog e = naive_record(l, now, window);
        if (!ok || !same(m, e)) FAIL("record mismatch: count " << l.count << " now " << now << " window " << window);
        fr.log(m);
        ++nr;
      }
      for (std::uint32_t window = 0; window <= kRefWinMax + 1; ++window) {
        const std::uint32_t r = recent_crashes(l, now, window);
        if (r != naive_recent(l, now, window)) FAIL("recent mismatch: count " << l.count << " now " << now << " window " << window);
        fc.num(r, 4);
        ++nc;
      }
    }
  }
  CHECK(nr == kRefCases_record_small);
  CHECK(nc == kRefCases_recent_small);
  CHECK(fr.h == kRefDigest_record_small);
  CHECK(fc.h == kRefDigest_recent_small);
}

TEST_CASE("decide sweep matches the digest and the rules", "[breaker][sweep]") {
  const auto logs = all_logs(kRefDLen, kRefDVal);
  Fnv f;
  std::size_t n = 0;
  for (std::size_t vi = 0; vi <= logs.size(); ++vi) {
    for (std::size_t gi = 0; gi <= logs.size(); ++gi) {
      const CrashLog vl = vi > 0 ? make_log(logs[vi - 1]) : CrashLog();
      const CrashLog gl = gi > 0 ? make_log(logs[gi - 1]) : CrashLog();
      for (std::uint32_t now = 0; now <= kRefDNow; ++now) {
        for (std::uint32_t window = 1; window <= kRefDWin; ++window) {
          const std::uint32_t rv = naive_recent(vl, now, window), rg = naive_recent(gl, now, window);
          for (std::uint32_t mc = 1; mc <= kRefDMaxC; ++mc) {
            for (std::uint32_t bc = 1; bc <= mc; ++bc) {
              for (int ga = 0; ga < 2; ++ga) {
                Decision d;
                const bool ok = decide(vi > 0 ? &vl : nullptr, gi > 0 ? &gl : nullptr, now, policy(mc, window, bc), ga != 0, &d);
                Action ea = Action::Run;
                Backend eb = Backend::Vulkan;
                if (rv + rg >= mc) {
                  ea = Action::Disable;
                } else if (ga != 0 && rv >= bc) {
                  eb = Backend::Gles;
                }
                if (!ok || d.action != ea || d.backend != eb) FAIL("decide mismatch at case " << n);
                f.num(static_cast<std::uint64_t>(d.action), 1);
                f.num(static_cast<std::uint64_t>(d.backend), 1);
                ++n;
              }
            }
          }
        }
      }
    }
  }
  CHECK(n == kRefCases_decide_small);
  CHECK(f.h == kRefDigest_decide_small);
}

TEST_CASE("wide random logs with extreme times match the oracle and the digest", "[breaker][sweep]") {
  Rng r{kRefSeed};
  Fnv fr, fc;
  for (std::size_t c = 0; c < kRefWideCases; ++c) {
    const std::uint64_t now = kRefNows[r.next() % (sizeof(kRefNows) / sizeof(kRefNows[0]))];
    const std::uint64_t window = kRefWindows[r.next() % (sizeof(kRefWindows) / sizeof(kRefWindows[0]))];
    CrashLog l;
    l.count = kRefCounts[r.next() % (sizeof(kRefCounts) / sizeof(kRefCounts[0]))];
    for (std::uint32_t i = 0; i < kMaxCrashes; ++i) {
      const std::uint64_t k = r.next() % 8;
      std::uint64_t v;
      switch (k) {
        case 0: v = 0; break;
        case 1: v = now; break;
        case 2: v = now - 1; break;
        case 3: v = now + 1; break;
        case 4: v = kMax; break;
        case 5: v = now - window; break;
        case 6: v = now - window + 1; break;
        default: v = r.next() % 200; break;
      }
      l.times_ms[i] = v;
    }
    const std::uint32_t rc = recent_crashes(l, now, window);
    if (rc != naive_recent(l, now, window)) FAIL("recent mismatch at wide case " << c);
    fc.num(rc, 4);
    CrashLog m = l;
    if (!record_crash(&m, now, policy(3, window, 2)) || !same(m, naive_record(l, now, window))) {
      FAIL("record mismatch at wide case " << c);
    }
    fr.log(m);
  }
  CHECK(fr.h == kRefDigest_record_wide);
  CHECK(fc.h == kRefDigest_recent_wide);
}
