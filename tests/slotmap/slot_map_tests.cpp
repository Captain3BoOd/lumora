// Tests for core/slotmap. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/slot_map_ref.py, an independent Python implementation.
// Regenerate it with: python3 tools/slot_map_ref.py
// The tests also carry a naive oracle that shares no code with core/slotmap/slot_map.cpp: std::vector arrays, the lowest free slot found
// with std::find, generations advanced with a remainder, and the handle split with / and %.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "slot_map.h"

using namespace lumora::slotmap;

namespace {

// BEGIN GENERATED REFERENCE (tools/slot_map_ref.py)
const std::uint32_t kRefCounts[] = {0u, 1u, 5u, 255u, 256u, 257u, 4294967295u};
const std::uint32_t kRefWrapCycles[] = {0u, 1u, 2u, 16777213u, 16777214u, 16777215u, 16777216u, 16777217u};
const std::uint32_t kRefWrapHandles[] = {0x100u, 0x200u, 0x300u, 0xFFFFFE00u, 0xFFFFFF00u, 0x100u, 0x200u, 0x300u};
const std::size_t kRefOps = 12000;
const std::uint64_t kRefDigest_ops = 0xA2463CAB2E7CEA53ull;
const std::uint64_t kRefDigest_table = 0xCFF5643354801FD2ull;
const std::size_t kRefCases_table = 12936;
const std::size_t kRefStat_alloc_ok = 2600;
const std::size_t kRefStat_alloc_fail = 419;
const std::size_t kRefStat_release_ok = 469;
const std::size_t kRefStat_release_fail = 2500;
const std::size_t kRefStat_live_true = 160;
const std::size_t kRefStat_live_false = 1831;
const std::size_t kRefStat_wraps = 2;
const std::size_t kRefStat_forged = 2040;
const std::size_t kRefStat_inits = 250;
const std::size_t kRefStat_corrupt_steps = 1629;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;
constexpr std::uint32_t kNoHandle = 0xDEADBEEFu;

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t u32() {
    x = x * kLcgMul + kLcgAdd;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

std::uint32_t checksum(const SlotMap& m) {
  std::uint32_t s = 0;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    s = s * 31u + m.gen[i] + 1u;
    s = s * 31u + static_cast<std::uint32_t>(m.used[i]) + 1u;
  }
  return s * 31u + m.count + 1u;
}

SlotMap fresh() {
  SlotMap m;
  init(&m);
  return m;
}

// A map with garbage in every field, then init
SlotMap garbage() {
  SlotMap m;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    m.gen[i] = 0xFFFFFFFFu - i;
    m.used[i] = static_cast<std::uint8_t>(i * 7u + 3u);
  }
  m.count = 0xFFFFFFFFu;
  return m;
}

std::uint32_t handle_of(std::uint32_t generation, std::uint32_t index) { return (generation << 8) | index; }

// ---- naive oracle -------------------------------------------------------------------------------------------------------------
struct Oracle {
  std::vector<std::uint32_t> gen = std::vector<std::uint32_t>(256, 1u);
  std::vector<int> used = std::vector<int>(256, 0);
  std::uint64_t count = 0;

  bool allocate(std::uint32_t* handle) {
    if (count > 256) return false;
    const auto it = std::find(used.begin(), used.end(), 0);
    if (it == used.end()) return false;
    const std::size_t i = static_cast<std::size_t>(it - used.begin());
    *handle = static_cast<std::uint32_t>((gen[i] % 16777216u) * 256u + i);
    used[i] = 1;
    ++count;
    return true;
  }
  bool live(std::uint32_t handle) const {
    if (count > 256) return false;
    const std::uint32_t g = handle / 256u;
    const std::size_t i = handle % 256u;
    if (g == 0) return false;
    return used[i] != 0 && gen[i] == g;
  }
  bool release(std::uint32_t handle) {
    if (!live(handle)) return false;
    const std::size_t i = handle % 256u;
    used[i] = 0;
    gen[i] = gen[i] % 16777215u + 1u;  // 16777215 -> 1, everything below goes up by one
    if (count > 0) --count;
    return true;
  }
  std::uint32_t size() const { return count > 256 ? 0u : static_cast<std::uint32_t>(count); }
  bool same(const SlotMap& m) const {
    if (m.count != count) return false;
    for (std::size_t i = 0; i < 256; ++i) {
      if (m.gen[i] != gen[i]) return false;
      if ((m.used[i] != 0) != (used[i] != 0)) return false;
      if (m.used[i] != static_cast<std::uint8_t>(used[i])) return false;
    }
    return true;
  }
};

constexpr std::uint32_t kForgedGens[] = {0xFFFFFEu, 0xFFFFFFu, 0u, 0x1000005u};

std::uint32_t pick_handle(Lcg& g, const std::vector<std::uint32_t>& hist) {
  const std::uint32_t r = g.u32();
  if (!hist.empty() && r % 4u != 0) return hist[g.u32() % hist.size()];
  return g.u32();
}

struct OpsStats {
  std::size_t bad = 0, alloc_ok = 0, alloc_fail = 0, release_ok = 0, release_fail = 0, live_true = 0, live_false = 0, wraps = 0, forged = 0,
              inits = 0, corrupt_steps = 0;
};

void run_ops(Fnv* f, OpsStats* st) {
  Lcg g(61);
  SlotMap m = fresh();
  Oracle o;
  std::vector<std::uint32_t> hist;
  for (std::size_t i = 0; i < kRefOps; ++i) {
    const std::uint32_t sel = g.u32() % 12u;
    std::uint32_t res = 0, h = 0;
    if (sel <= 2) {
      h = kNoHandle;
      std::uint32_t oh = kNoHandle;
      const bool want = o.allocate(&oh);
      const bool got = allocate(&m, &h);
      if (got != want || h != oh) ++st->bad;
      res = got ? 1u : 0u;
      if (got) {
        hist.push_back(h);
        ++st->alloc_ok;
      } else {
        ++st->alloc_fail;
      }
    } else if (sel <= 5) {
      h = pick_handle(g, hist);
      const std::uint32_t was = m.gen[h & 255u];
      const bool want = o.release(h);
      const bool got = release(&m, h);
      if (got != want) ++st->bad;
      res = got ? 1u : 0u;
      if (got) {
        ++st->release_ok;
        if (was == kMaxGeneration) ++st->wraps;
      } else {
        ++st->release_fail;
      }
    } else if (sel == 6 || sel == 7) {
      if (sel == 6) {
        h = pick_handle(g, hist);
      } else {
        const std::uint32_t a = g.u32() % 5u;
        const std::uint32_t b = g.u32() & 255u;
        h = (a << 8) | b;
      }
      const bool want = o.live(h);
      const bool got = is_live(&m, h);
      if (got != want) ++st->bad;
      res = got ? 1u : 0u;
      if (got) ++st->live_true;
      else ++st->live_false;
    } else if (sel == 8) {
      res = size(&m);
      if (res != o.size()) ++st->bad;
    } else if (sel <= 10) {
      std::uint32_t s = g.u32() % 256u;
      const std::uint32_t v = g.u32() % 8u;
      if (v <= 3) {
        if (g.u32() % 2u == 0) s %= 4u;
        m.gen[s] = kForgedGens[v];
        o.gen[s] = kForgedGens[v];
      } else if (v == 4) {
        m.used[s] = 2;
        o.used[s] = 2;
      } else if (v == 5) {
        m.used[s] = 0;
        o.used[s] = 0;
      } else if (v == 6) {
        static const std::uint32_t kC[] = {0u, 255u, 256u, 257u};
        const std::uint32_t c = g.u32() % 4u;
        if (g.u32() % 4u == 0) {
          m.count = kC[c];
          o.count = kC[c];
        }
      } else {
        const std::uint32_t val = g.u32();
        m.gen[s] = val;
        o.gen[s] = val;
      }
      ++st->forged;
    } else {
      if (g.u32() % 4u == 0) {
        init(&m);
        o = Oracle();
        ++st->inits;
      }
    }
    if (m.count > kSlots) ++st->corrupt_steps;
    if (!o.same(m)) ++st->bad;
    f->byte(static_cast<std::uint8_t>(sel));
    f->u32(res);
    f->u32(h);
    f->u32(checksum(m));
  }
}

SlotMap build_map(std::size_t count_i, std::size_t used_i, std::size_t gen_i) {
  SlotMap m = fresh();
  m.count = kRefCounts[count_i];
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    std::uint8_t u = 0;
    switch (used_i) {
      case 0: u = 0; break;
      case 1: u = 1; break;
      case 2: u = i < 3 ? 1 : 0; break;
      case 3: u = i % 3 == 0 ? 1 : 0; break;
      case 4: u = i != 255 ? 1 : 0; break;
      default: u = i == 0 ? 2 : (i == 1 ? 255 : (i == 2 ? 1 : 0)); break;
    }
    m.used[i] = u;
  }
  switch (gen_i) {
    case 1: m.gen[0] = 0u; break;
    case 2: m.gen[0] = 0xFFFFFFu; break;
    case 3: m.gen[0] = 0xFFFFFEu; break;
    case 4: m.gen[0] = 0x1000001u; break;
    case 5: m.gen[0] = 0xFFFFFFFFu; break;
    case 6:
      m.gen[0] = 0xFFFFFFu;
      m.gen[1] = 0xFFFFFFu;
      m.gen[2] = 2u;
      break;
    default: break;
  }
  return m;
}

std::vector<std::uint32_t> table_handles() {
  const std::uint32_t idx[] = {0u, 1u, 2u, 255u};
  const std::uint32_t gens[] = {0u, 1u, 2u, 0xFFFFFEu, 0xFFFFFFu};
  std::vector<std::uint32_t> hs;
  for (const std::uint32_t i : idx)
    for (const std::uint32_t gg : gens) hs.push_back(handle_of(gg, i));
  hs.push_back(0xFFFFFFFFu);
  return hs;
}

}  // namespace

TEST_CASE("constants and layout", "[slotmap]") {
  CHECK(kSlots == 256u);
  CHECK(kMaxGeneration == 0xFFFFFFu);
  SlotMap m;
  CHECK(m.count == 0u);  // the default member initializer
}

TEST_CASE("init works on any previous content and ignores null", "[slotmap][init]") {
  init(nullptr);
  SlotMap m = garbage();
  init(&m);
  CHECK(m.count == 0u);
  std::size_t bad = 0;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    if (m.gen[i] != 1u || m.used[i] != 0) ++bad;
  }
  CHECK(bad == 0);
  CHECK(size(&m) == 0u);
}

TEST_CASE("null map and null handle pointer", "[slotmap][null]") {
  std::uint32_t h = kNoHandle;
  CHECK_FALSE(allocate(nullptr, &h));
  CHECK(h == kNoHandle);
  SlotMap m = fresh();
  CHECK_FALSE(allocate(&m, nullptr));
  CHECK(m.count == 0u);
  CHECK(m.used[0] == 0);
  CHECK_FALSE(release(nullptr, 0x100u));
  CHECK_FALSE(is_live(nullptr, 0x100u));
  CHECK(size(nullptr) == 0u);
}

TEST_CASE("allocate hands out the lowest free slot with generation 1", "[slotmap][allocate]") {
  SlotMap m = fresh();
  std::uint32_t h = kNoHandle;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    REQUIRE(allocate(&m, &h));
    CHECK(h == handle_of(1u, i));
    CHECK(is_live(&m, h));
    CHECK(size(&m) == i + 1u);
  }
  CHECK(m.count == 256u);
  h = kNoHandle;
  const std::uint32_t before = checksum(m);
  CHECK_FALSE(allocate(&m, &h));
  CHECK(h == kNoHandle);
  CHECK(checksum(m) == before);
}

TEST_CASE("release frees a slot, the next allocate reuses the lowest hole with a new generation", "[slotmap][release]") {
  SlotMap m = fresh();
  std::uint32_t hs[8];
  for (auto& x : hs) REQUIRE(allocate(&m, &x));
  CHECK(release(&m, hs[5]));
  CHECK(release(&m, hs[2]));
  CHECK(size(&m) == 6u);
  CHECK_FALSE(is_live(&m, hs[5]));
  CHECK_FALSE(is_live(&m, hs[2]));
  CHECK(is_live(&m, hs[3]));
  std::uint32_t h = 0;
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(2u, 2u));  // lowest hole, generation moved from 1 to 2
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(2u, 5u));
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(1u, 8u));
  CHECK_FALSE(is_live(&m, hs[2]));  // the old handle of slot 2 stays stale
  CHECK(is_live(&m, handle_of(2u, 2u)));
}

TEST_CASE("release rejects a second release, a stale handle, a wrong generation and a free slot", "[slotmap][release]") {
  SlotMap m = fresh();
  std::uint32_t a = 0, b = 0;
  REQUIRE(allocate(&m, &a));
  REQUIRE(allocate(&m, &b));
  CHECK(release(&m, a));
  const std::uint32_t before = checksum(m);
  CHECK_FALSE(release(&m, a));  // second release of the same handle
  CHECK(checksum(m) == before);
  CHECK_FALSE(release(&m, handle_of(2u, 1u)));  // right slot in use, generation one too high
  CHECK_FALSE(release(&m, handle_of(0u, 1u)));  // generation 0
  CHECK_FALSE(release(&m, handle_of(1u, 7u)));  // free slot
  CHECK_FALSE(release(&m, 0u));
  CHECK(checksum(m) == before);
  CHECK(size(&m) == 1u);
  CHECK(is_live(&m, b));
  CHECK(m.gen[0] == 2u);
}

TEST_CASE("is_live: generation 0 is never live, index comes from the low 8 bits", "[slotmap][live]") {
  SlotMap m = fresh();
  std::uint32_t h = 0;
  REQUIRE(allocate(&m, &h));
  CHECK(is_live(&m, 0x100u));
  CHECK_FALSE(is_live(&m, 0x000u));  // generation 0, slot 0 in use
  CHECK_FALSE(is_live(&m, 0x200u));
  CHECK_FALSE(is_live(&m, 0x101u));  // slot 1 is free
  CHECK_FALSE(is_live(&m, 0xFFFFFFFFu));
  m.gen[0] = 0;  // forged: generation 0 in use
  CHECK_FALSE(is_live(&m, 0x000u));
  CHECK_FALSE(is_live(&m, 0x100u));
}

TEST_CASE("generation wraps from 0xFFFFFF to 1 (forged near the wrap)", "[slotmap][wrap]") {
  SlotMap m = fresh();
  m.gen[3] = 0xFFFFFEu;
  m.used[3] = 1;
  m.count = 1;
  CHECK(is_live(&m, handle_of(0xFFFFFEu, 3u)));
  CHECK(release(&m, handle_of(0xFFFFFEu, 3u)));
  CHECK(m.gen[3] == 0xFFFFFFu);
  CHECK(m.used[3] == 0);
  CHECK(m.count == 0u);
  std::uint32_t h = 0;
  m.used[0] = 1;
  m.used[1] = 1;
  m.used[2] = 1;
  m.count = 3;
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(0xFFFFFFu, 3u));
  CHECK(is_live(&m, h));
  CHECK_FALSE(is_live(&m, handle_of(0xFFFFFEu, 3u)));
  CHECK(release(&m, h));
  CHECK(m.gen[3] == 1u);  // wrapped, never 0
  CHECK_FALSE(is_live(&m, h));
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(1u, 3u));
}

TEST_CASE("generation wrap through the full 16,777,215-cycle loop on one slot", "[slotmap][wrap]") {
  SlotMap m = fresh();
  const std::uint32_t total = kMaxGeneration + 3u;  // the loop goes three cycles past the wrap
  std::size_t bad = 0;
  std::size_t probe = 0;
  std::uint32_t first = 0;
  std::uint32_t old_handle = 0;
  for (std::uint32_t k = 0; k < total; ++k) {
    std::uint32_t h = kNoHandle;
    if (!allocate(&m, &h)) {
      ++bad;
      break;
    }
    const std::uint32_t want_gen = k % kMaxGeneration + 1u;  // 1 .. 0xFFFFFF, then 1 again
    if (h != handle_of(want_gen, 0u)) ++bad;
    if (k == 0) first = h;
    if (probe < sizeof(kRefWrapCycles) / sizeof(kRefWrapCycles[0]) && kRefWrapCycles[probe] == k) {
      if (h != kRefWrapHandles[probe]) ++bad;
      ++probe;
    }
    if (k > 0 && is_live(&m, old_handle)) ++bad;  // the previous handle is stale (except after a full period, see below)
    if (!is_live(&m, h)) ++bad;
    if (m.count != 1u) ++bad;
    if (!release(&m, h)) ++bad;
    if (is_live(&m, h)) ++bad;
    if (m.gen[0] == 0u || m.gen[0] > kMaxGeneration) ++bad;
    old_handle = h;
  }
  CHECK(bad == 0);
  CHECK(probe == sizeof(kRefWrapCycles) / sizeof(kRefWrapCycles[0]));
  // after exactly 0xFFFFFF cycles the generation is back at 1, so the handle from cycle 0 is the next handle of slot 0 (a known
  // property of 24-bit generations: a handle kept for 16,777,215 reuses of its slot becomes live again)
  CHECK(m.gen[0] == 4u);
  CHECK(first == handle_of(1u, 0u));
}

TEST_CASE("forged maps: generation 0 and generation above the maximum on allocate", "[slotmap][forged]") {
  SlotMap m = fresh();
  m.gen[0] = 0;
  m.gen[1] = 0x1000001u;
  m.gen[2] = 0xFFFFFFFFu;
  std::uint32_t h = kNoHandle;
  REQUIRE(allocate(&m, &h));
  CHECK(h == 0x000u);  // generation 0 handed out as stored
  CHECK(m.gen[0] == 0u);
  CHECK_FALSE(is_live(&m, h));  // generation field 0 is never live
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(1u, 1u));  // 0x1000001 masked to 24 bits
  CHECK(m.gen[1] == 0x1000001u);  // and not repaired
  CHECK_FALSE(is_live(&m, h));    // the stored generation differs from the masked one
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(0xFFFFFFu, 2u));
  CHECK(m.gen[2] == 0xFFFFFFFFu);
  CHECK_FALSE(is_live(&m, h));
  CHECK(m.count == 3u);
  CHECK_FALSE(release(&m, h));
  CHECK(m.count == 3u);
}

TEST_CASE("forged maps: used values other than 0 and 1 count as in use", "[slotmap][forged]") {
  SlotMap m = fresh();
  m.used[0] = 2;
  m.used[1] = 255;
  m.used[2] = 1;
  m.count = 3;
  std::uint32_t h = 0;
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(1u, 3u));
  CHECK(is_live(&m, handle_of(1u, 0u)));
  CHECK(is_live(&m, handle_of(1u, 1u)));
  CHECK(release(&m, handle_of(1u, 0u)));
  CHECK(m.used[0] == 0);
  CHECK(release(&m, handle_of(1u, 1u)));
  CHECK(m.used[1] == 0);
  CHECK(m.count == 2u);
}

TEST_CASE("forged maps: count disagreeing with the flags", "[slotmap][forged]") {
  SlotMap m = fresh();
  m.used[0] = 1;
  m.used[1] = 1;
  m.count = 0;  // flags say 2 in use, count says 0
  std::uint32_t h = 0;
  CHECK(size(&m) == 0u);
  REQUIRE(allocate(&m, &h));
  CHECK(h == handle_of(1u, 2u));  // the flags decide
  CHECK(m.count == 1u);
  m.count = 0;
  CHECK(release(&m, handle_of(1u, 0u)));
  CHECK(m.count == 0u);  // never underflows
  CHECK(m.used[0] == 0);
  CHECK(m.gen[0] == 2u);
  CHECK(release(&m, handle_of(1u, 1u)));
  CHECK(m.count == 0u);
  // count 256 with a free flag: allocate still works and count becomes 257
  SlotMap n = fresh();
  n.count = 256;
  REQUIRE(allocate(&n, &h));
  CHECK(h == handle_of(1u, 0u));
  CHECK(n.count == 257u);
  CHECK(size(&n) == 0u);  // now corrupt
  // count below 256 with every flag set: no slot, false
  SlotMap full = fresh();
  for (auto& u : full.used) u = 1;
  full.count = 5;
  h = kNoHandle;
  CHECK_FALSE(allocate(&full, &h));
  CHECK(h == kNoHandle);
  CHECK(full.count == 5u);
}

TEST_CASE("corrupt count (above 256): every function refuses and changes nothing", "[slotmap][corrupt]") {
  const std::uint32_t counts[] = {257u, 1000u, 0xFFFFFFFFu};
  for (const std::uint32_t c : counts) {
    SlotMap m = fresh();
    m.used[0] = 1;
    m.count = c;
    const std::uint32_t before = checksum(m);
    std::uint32_t h = kNoHandle;
    CHECK(size(&m) == 0u);
    CHECK_FALSE(allocate(&m, &h));
    CHECK(h == kNoHandle);
    CHECK_FALSE(is_live(&m, handle_of(1u, 0u)));
    CHECK_FALSE(release(&m, handle_of(1u, 0u)));
    CHECK(checksum(m) == before);
    init(&m);  // init repairs it
    CHECK(size(&m) == 0u);
    CHECK(allocate(&m, &h));
  }
  SlotMap edge = fresh();
  edge.count = 256;  // exactly kSlots is not corrupt
  CHECK(size(&edge) == 256u);
  edge.count = 257;
  CHECK(size(&edge) == 0u);
}

TEST_CASE("every handle value: low 8 bits pick the slot, generation 0 is never live", "[slotmap][live]") {
  SlotMap m = fresh();
  std::uint32_t h = 0;
  for (std::uint32_t i = 0; i < kSlots; ++i) REQUIRE(allocate(&m, &h));
  std::size_t bad = 0;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    if (!is_live(&m, handle_of(1u, i))) ++bad;
    if (is_live(&m, handle_of(0u, i))) ++bad;
    if (is_live(&m, handle_of(2u, i))) ++bad;
    if (is_live(&m, handle_of(0xFFFFFFu, i))) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("scripted operations match the oracle and the reference digest", "[slotmap][sweep]") {
  Fnv f;
  OpsStats st;
  run_ops(&f, &st);
  CHECK(st.bad == 0);
  CHECK(st.alloc_ok == kRefStat_alloc_ok);
  CHECK(st.alloc_fail == kRefStat_alloc_fail);
  CHECK(st.release_ok == kRefStat_release_ok);
  CHECK(st.release_fail == kRefStat_release_fail);
  CHECK(st.live_true == kRefStat_live_true);
  CHECK(st.live_false == kRefStat_live_false);
  CHECK(st.wraps == kRefStat_wraps);
  CHECK(st.forged == kRefStat_forged);
  CHECK(st.inits == kRefStat_inits);
  CHECK(st.corrupt_steps == kRefStat_corrupt_steps);
  CHECK(st.alloc_ok > 1000);
  CHECK(st.alloc_fail > 100);
  CHECK(st.release_ok > 300);
  CHECK(st.live_true > 100);
  CHECK(st.wraps >= 1);
  CHECK(st.forged > 1000);
  CHECK(st.inits > 100);
  CHECK(st.corrupt_steps > 500);
  CHECK(f.h == kRefDigest_ops);
}

TEST_CASE("forged map table digest", "[slotmap][sweep]") {
  Fnv f;
  std::size_t cases = 0;
  const std::vector<std::uint32_t> handles = table_handles();
  for (std::size_t c = 0; c < sizeof(kRefCounts) / sizeof(kRefCounts[0]); ++c)
    for (std::size_t u = 0; u < 6; ++u)
      for (std::size_t gi = 0; gi < 7; ++gi) {
        const SlotMap base = build_map(c, u, gi);
        f.u32(size(&base));
        SlotMap a = base;
        std::uint32_t h = kNoHandle;
        const bool ok = allocate(&a, &h);
        f.u32(ok ? 1u : 0u);
        f.u32(h);
        f.u32(checksum(a));
        f.u32(is_live(&a, h) ? 1u : 0u);
        cases += 2;
        for (const std::uint32_t hh : handles) {
          f.u32(is_live(&base, hh) ? 1u : 0u);
          SlotMap r = base;
          f.u32(release(&r, hh) ? 1u : 0u);
          f.u32(checksum(r));
          cases += 2;
        }
      }
  CHECK(cases == kRefCases_table);
  CHECK(f.h == kRefDigest_table);
}
