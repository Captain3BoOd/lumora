// Tests for core/diag/diag_log. Evidence label: unit tests only. Every comparison is exact, entries are compared field by
// field (never as raw struct bytes).
// The block between BEGIN and END GENERATED REFERENCE comes from tools/diag_log_ref.py, an independent Python
// implementation. Regenerate it with: python3 tools/diag_log_ref.py
// The tests also carry a naive oracle (a backward walk of the window and a flat word list for the digest) that shares no
// code with core/diag/diag_log.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "diag_log.h"

using namespace lumora::diag;

namespace {

// BEGIN GENERATED REFERENCE (tools/diag_log_ref.py)
const std::uint32_t kRefCounts[] = {0u, 1u, 2u, 3u, 10u, 63u, 64u, 65u, 255u, 4294967295u};
const std::uint32_t kRefNexts[] = {0u, 1u, 5u, 31u, 32u, 63u, 64u, 65u, 4294967295u};
const std::uint64_t kRefDroppeds[] = {0ull, 1ull, 18446744073709551614ull, 18446744073709551615ull, 4294967295ull, 4294967296ull};
const std::uint32_t kRefKinds[] = {0u, 5u};
const std::uint32_t kRefReadIndexes[] = {0u, 1u, 2u, 31u, 62u, 63u, 64u, 65u, 4294967295u};
const std::uint64_t kRefPushTs[] = {0ull, 1ull, 4294967295ull, 4294967296ull, 18446744073709551614ull, 18446744073709551615ull, 81985529216486895ull};
const std::uint16_t kRefPushCodes[] = {0u, 1u, 255u, 256u, 32767u, 32768u, 65535u};
const std::int32_t kRefPushVals[] = {0, 1, -1, INT32_MAX, INT32_MIN, 12345, -12345};
const std::uint64_t kRefSeqSeeds[] = {5u, 6u, 7u, 8u};
const std::size_t kRefSeqN = 400;
const std::uint64_t kRefNullDigest = 0xCBF29CE484222325ull;
const std::uint64_t kRefDigest_hand = 0xEA906278A72E743Dull;
const std::size_t kRefCases_hand = 1080;
const std::uint64_t kRefDigest_seq = 0xBB28A37A14DB4947ull;
const std::size_t kRefCases_seq = 1600;
const std::uint64_t kRefDigest_sat = 0xE24B642FA35E6FC9ull;
const std::size_t kRefCases_sat = 15;
// END GENERATED REFERENCE

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  void u64(std::uint64_t v) {
    u32(static_cast<std::uint32_t>(v));
    u32(static_cast<std::uint32_t>(v >> 32));
  }
};

bool same_entry(const LogEntry& x, const LogEntry& y) {
  return x.timestamp_ns == y.timestamp_ns && x.code == y.code && x.a == y.a && x.b == y.b;
}

bool same_log(const DiagLog& x, const DiagLog& y) {
  if (x.count != y.count || x.next != y.next || x.dropped != y.dropped) return false;
  for (std::uint32_t i = 0; i < kLogCapacity; ++i)
    if (!same_entry(x.entries[i], y.entries[i])) return false;
  return true;
}

LogEntry mk(std::uint64_t ts, std::uint16_t code, std::int32_t a, std::int32_t b) {
  LogEntry e;
  e.timestamp_ns = ts;
  e.code = code;
  e.a = a;
  e.b = b;
  return e;
}

// The deterministic fill that tools/diag_log_ref.py uses for hand-built objects.
void fill(DiagLog* g, std::uint32_t kind) {
  for (std::uint32_t i = 0; i < kLogCapacity; ++i) {
    LogEntry& e = g->entries[i];
    e.timestamp_ns = static_cast<std::uint64_t>(i + 1) * 0x9E3779B97F4A7C15ull + kind;
    e.code = static_cast<std::uint16_t>((i * 257u + kind * 13u + 1u) & 0xFFFFu);
    e.a = static_cast<std::int32_t>((i + 1u) * 0x01000193u + kind);
    e.b = static_cast<std::int32_t>(((i + 7u) * 0x85EBCA6Bu) ^ kind);
  }
}

// Oracle window: walk backward from next - 1 for count slots, then reverse. Only for valid states.
std::vector<LogEntry> oracle_window(const DiagLog& g) {
  std::vector<LogEntry> rev;
  std::uint32_t s = g.next;
  for (std::uint32_t i = 0; i < g.count; ++i) {
    s = (s == 0) ? kLogCapacity - 1 : s - 1;
    rev.push_back(g.entries[s]);
  }
  std::vector<LogEntry> out(rev.rbegin(), rev.rend());
  return out;
}

bool valid_state(const DiagLog& g) { return g.count <= kLogCapacity && g.next < kLogCapacity; }

std::uint64_t oracle_digest(const DiagLog& g) {
  std::vector<std::uint32_t> words;
  if (valid_state(g)) {
    for (const LogEntry& e : oracle_window(g)) {
      words.push_back(static_cast<std::uint32_t>(e.timestamp_ns & 0xFFFFFFFFull));
      words.push_back(static_cast<std::uint32_t>(e.timestamp_ns >> 32));
      words.push_back(e.code);
      words.push_back(static_cast<std::uint32_t>(e.a));
      words.push_back(static_cast<std::uint32_t>(e.b));
    }
  }
  words.push_back(static_cast<std::uint32_t>(g.dropped & 0xFFFFFFFFull));
  words.push_back(static_cast<std::uint32_t>(g.dropped >> 32));
  Fnv f;
  for (std::uint32_t w : words) f.u32(w);
  return f.h;
}

// Sequence model for logs built only by clear and push: a plain list of the last entries, shifted on overflow.
struct Model {
  std::vector<LogEntry> list;
  std::uint64_t dropped = 0;
  void push(const LogEntry& e) {
    if (list.size() == kLogCapacity) {
      list.erase(list.begin());
      if (dropped != UINT64_MAX) ++dropped;
    }
    list.push_back(e);
  }
  void clear() {
    list.clear();
    dropped = 0;
  }
};

void feed_read(Fnv* f, bool ok, const LogEntry& e) {
  f->byte(ok ? 1 : 0);
  if (ok) {
    f->u64(e.timestamp_ns);
    f->u32(e.code);
    f->u32(static_cast<std::uint32_t>(e.a));
    f->u32(static_cast<std::uint32_t>(e.b));
  }
}

void feed_state(Fnv* f, const DiagLog& g) {
  for (std::uint32_t i = 0; i < kLogCapacity; ++i) {
    f->u64(g.entries[i].timestamp_ns);
    f->u32(g.entries[i].code);
    f->u32(static_cast<std::uint32_t>(g.entries[i].a));
    f->u32(static_cast<std::uint32_t>(g.entries[i].b));
  }
  f->u32(g.count);
  f->u32(g.next);
  f->u64(g.dropped);
}

const LogEntry kSentinel = mk(0x1122334455667788ull, 0xBEEF, -99, 99);

}  // namespace

TEST_CASE("constants and defaults", "[diag_log]") {
  CHECK(kLogCapacity == 64u);
  const DiagLog g;
  CHECK(g.count == 0u);
  CHECK(g.next == 0u);
  CHECK(g.dropped == 0u);
  CHECK(g.entries[0].timestamp_ns == 0u);
  CHECK(g.entries[63].code == 0);
  CHECK(log_size(&g) == 0u);
  CHECK(log_digest(&g) == 0xa8c7f832281a39c5ull);  // eight zero bytes (two dropped words)
}

TEST_CASE("log_clear", "[diag_log][clear]") {
  DiagLog g;
  for (int i = 0; i < 100; ++i) CHECK(log_push(&g, static_cast<std::uint64_t>(1000 + i), 7, i, -i));
  CHECK(g.dropped == 36u);
  CHECK(log_clear(&g));
  CHECK(same_log(g, DiagLog{}));
  fill(&g, 3);
  g.count = 1000;
  g.next = 77;
  g.dropped = UINT64_MAX;
  CHECK(log_clear(&g));  // an invalid object is cleared too
  CHECK(same_log(g, DiagLog{}));
  CHECK_FALSE(log_clear(nullptr));
}

TEST_CASE("log_push first entries, wrap and drop count", "[diag_log][push]") {
  DiagLog g;
  CHECK(log_push(&g, 5, 6, 7, 8));
  CHECK(g.count == 1u);
  CHECK(g.next == 1u);
  CHECK(g.dropped == 0u);
  CHECK(same_entry(g.entries[0], mk(5, 6, 7, 8)));
  for (std::uint32_t i = 1; i < 64; ++i) {
    CHECK(log_push(&g, 100 + i, static_cast<std::uint16_t>(i), static_cast<std::int32_t>(i), -static_cast<std::int32_t>(i)));
    CHECK(g.count == i + 1);
    CHECK(g.next == (i + 1) % 64);
    CHECK(g.dropped == 0u);
  }
  CHECK(g.count == 64u);
  CHECK(g.next == 0u);
  CHECK(log_push(&g, 9000, 1, 2, 3));  // the 65th overwrites slot 0, the oldest
  CHECK(same_entry(g.entries[0], mk(9000, 1, 2, 3)));
  CHECK(g.count == 64u);
  CHECK(g.next == 1u);
  CHECK(g.dropped == 1u);
  for (int i = 0; i < 70; ++i) CHECK(log_push(&g, 1, 1, 1, 1));
  CHECK(g.dropped == 71u);
  CHECK(g.next == (1u + 70u) % 64u);
  CHECK_FALSE(log_push(nullptr, 1, 2, 3, 4));
}

TEST_CASE("log_push stores every field at full range", "[diag_log][push]") {
  DiagLog g;
  CHECK(log_push(&g, UINT64_MAX, 65535, INT32_MIN, INT32_MAX));
  CHECK(same_entry(g.entries[0], mk(UINT64_MAX, 65535, INT32_MIN, INT32_MAX)));
  CHECK(log_push(&g, 0x0123456789ABCDEFull, 0, INT32_MAX, INT32_MIN));
  CHECK(same_entry(g.entries[1], mk(0x0123456789ABCDEFull, 0, INT32_MAX, INT32_MIN)));
  CHECK(g.entries[2].timestamp_ns == 0u);  // untouched slots stay zero
}

TEST_CASE("log_push saturates dropped at UINT64_MAX", "[diag_log][push]") {
  DiagLog g;
  for (int i = 0; i < 64; ++i) CHECK(log_push(&g, 1, 1, 1, 1));
  g.dropped = UINT64_MAX - 2;
  CHECK(log_push(&g, 2, 2, 2, 2));
  CHECK(g.dropped == UINT64_MAX - 1);
  CHECK(log_push(&g, 3, 3, 3, 3));
  CHECK(g.dropped == UINT64_MAX);
  CHECK(log_push(&g, 4, 4, 4, 4));
  CHECK(g.dropped == UINT64_MAX);
  CHECK(g.count == 64u);
  g.dropped = 0xFFFFFFFFull;  // the 32-bit carry
  CHECK(log_push(&g, 5, 5, 5, 5));
  CHECK(g.dropped == 0x100000000ull);
}

TEST_CASE("log_push repairs invalid state without counting a drop", "[diag_log][push][repair]") {
  struct Case {
    std::uint32_t count, next;
  };
  const Case cases[] = {{65, 3}, {64, 64}, {10, 64}, {UINT32_MAX, 5}, {5, UINT32_MAX}, {65, 0}, {0, 64}, {UINT32_MAX, UINT32_MAX}};
  for (const Case& c : cases) {
    DiagLog g;
    fill(&g, 1);
    g.count = c.count;
    g.next = c.next;
    g.dropped = 12;
    DiagLog before = g;
    CHECK(log_push(&g, 9, 8, 7, 6));
    INFO("count " << c.count << " next " << c.next);
    CHECK(g.count == 1u);
    CHECK(g.next == 1u);
    CHECK(g.dropped == 12u);
    CHECK(same_entry(g.entries[0], mk(9, 8, 7, 6)));
    for (std::uint32_t i = 1; i < kLogCapacity; ++i) CHECK(same_entry(g.entries[i], before.entries[i]));  // stale slots stay
    LogEntry e = kSentinel;
    CHECK(log_read(&g, 0, &e));
    CHECK(same_entry(e, mk(9, 8, 7, 6)));
  }
}

TEST_CASE("log_push honors next of a non-full hand-built object", "[diag_log][push]") {
  DiagLog g;
  fill(&g, 0);
  g.count = 3;
  g.next = 10;
  CHECK(log_push(&g, 1, 2, 3, 4));
  CHECK(same_entry(g.entries[10], mk(1, 2, 3, 4)));
  CHECK(g.count == 4u);
  CHECK(g.next == 11u);
  CHECK(g.dropped == 0u);
  LogEntry e = kSentinel;
  CHECK(log_read(&g, 3, &e));
  CHECK(same_entry(e, mk(1, 2, 3, 4)));
  g.next = 63;
  g.count = 63;
  CHECK(log_push(&g, 8, 8, 8, 8));
  CHECK(g.next == 0u);
  CHECK(g.count == 64u);
  CHECK(g.dropped == 0u);
  CHECK(log_push(&g, 9, 9, 9, 9));
  CHECK(g.dropped == 1u);
}

TEST_CASE("log_read window, index and failures", "[diag_log][read]") {
  DiagLog g;
  for (std::uint32_t i = 0; i < 70; ++i) CHECK(log_push(&g, i, static_cast<std::uint16_t>(i), static_cast<std::int32_t>(i), 0));
  LogEntry e = kSentinel;
  CHECK(log_read(&g, 0, &e));
  CHECK(e.timestamp_ns == 6u);  // 70 pushes into 64 slots: the oldest left is number 6
  CHECK(log_read(&g, 63, &e));
  CHECK(e.timestamp_ns == 69u);
  CHECK(log_read(&g, 10, &e));
  CHECK(e.timestamp_ns == 16u);
  e = kSentinel;
  CHECK_FALSE(log_read(&g, 64, &e));
  CHECK_FALSE(log_read(&g, 65, &e));
  CHECK_FALSE(log_read(&g, UINT32_MAX, &e));
  CHECK_FALSE(log_read(nullptr, 0, &e));
  CHECK_FALSE(log_read(&g, 0, nullptr));
  CHECK_FALSE(log_read(nullptr, 0, nullptr));
  CHECK(same_entry(e, kSentinel));
  DiagLog empty;
  CHECK_FALSE(log_read(&empty, 0, &e));
  CHECK(same_entry(e, kSentinel));
  // hand-built window: count 10, next 5 reads slots 59..63, 0..4
  DiagLog h;
  fill(&h, 0);
  h.count = 10;
  h.next = 5;
  const std::uint32_t slots[10] = {59, 60, 61, 62, 63, 0, 1, 2, 3, 4};
  for (std::uint32_t i = 0; i < 10; ++i) {
    CHECK(log_read(&h, i, &e));
    CHECK(same_entry(e, h.entries[slots[i]]));
  }
  CHECK_FALSE(log_read(&h, 10, &e));
  // invalid states read nothing
  e = kSentinel;
  for (std::uint32_t c : {65u, UINT32_MAX}) {
    h.count = c;
    h.next = 5;
    CHECK_FALSE(log_read(&h, 0, &e));
  }
  h.count = 5;
  h.next = 64;
  CHECK_FALSE(log_read(&h, 0, &e));
  h.next = UINT32_MAX;
  CHECK_FALSE(log_read(&h, 0, &e));
  CHECK(same_entry(e, kSentinel));
}

TEST_CASE("log_size", "[diag_log][size]") {
  DiagLog g;
  CHECK(log_size(&g) == 0u);
  for (std::uint32_t i = 1; i <= 70; ++i) {
    CHECK(log_push(&g, i, 0, 0, 0));
    CHECK(log_size(&g) == (i < 64 ? i : 64u));
  }
  CHECK(log_size(nullptr) == 0u);
  g.count = 65;
  CHECK(log_size(&g) == 0u);
  g.count = UINT32_MAX;
  CHECK(log_size(&g) == 0u);
  g.count = 5;
  g.next = 64;
  CHECK(log_size(&g) == 0u);
  g.next = 63;
  CHECK(log_size(&g) == 5u);
  g.count = 64;
  CHECK(log_size(&g) == 64u);
}

TEST_CASE("log_digest known values", "[diag_log][digest]") {
  CHECK(log_digest(nullptr) == 0xcbf29ce484222325ull);
  CHECK(kRefNullDigest == 0xcbf29ce484222325ull);
  DiagLog g;
  CHECK(log_digest(&g) == 0xa8c7f832281a39c5ull);
  CHECK(log_push(&g, 1, 2, 3, -4));
  CHECK(log_digest(&g) == 0x24b1188408d391d2ull);
  CHECK(log_push(&g, 0x100000005ull, 65535, INT32_MIN, INT32_MAX));
  CHECK(log_digest(&g) == 0x4324bf7ddff36208ull);
  DiagLog d;
  d.dropped = 0x100000007ull;  // the dropped counter is hashed as two words
  CHECK(log_digest(&d) == 0xebd24f1f5d8217f3ull);
  DiagLog f;
  for (std::uint32_t i = 0; i < 66; ++i) CHECK(log_push(&f, i, static_cast<std::uint16_t>(i), static_cast<std::int32_t>(i), static_cast<std::int32_t>(i)));
  CHECK(f.dropped == 2u);
  CHECK(log_digest(&f) == 0xa6c5c43cda80d187ull);
}

TEST_CASE("log_digest: invalid log hashes as empty with dropped as given", "[diag_log][digest]") {
  DiagLog e;
  e.dropped = 0x100000007ull;
  const std::uint64_t empty = log_digest(&e);
  for (std::uint32_t c : {65u, UINT32_MAX}) {
    DiagLog g;
    fill(&g, 0);
    g.count = c;
    g.next = 3;
    g.dropped = 0x100000007ull;
    CHECK(log_digest(&g) == empty);
  }
  DiagLog g;
  fill(&g, 0);
  g.count = 3;
  g.next = 64;
  g.dropped = 0x100000007ull;
  CHECK(log_digest(&g) == empty);
  g.dropped = 1;
  CHECK(log_digest(&g) != empty);
}

TEST_CASE("log_digest sees every word of every entry", "[diag_log][digest]") {
  DiagLog base;
  for (std::uint32_t i = 0; i < 64; ++i) CHECK(log_push(&base, 0x1000000010ull + i, static_cast<std::uint16_t>(i), 5, 6));
  const std::uint64_t d0 = log_digest(&base);
  std::size_t bad = 0;
  for (std::uint32_t slot = 0; slot < 64; ++slot) {
    for (int w = 0; w < 6; ++w) {
      DiagLog g = base;
      LogEntry& e = g.entries[slot];
      if (w == 0) e.timestamp_ns ^= 1ull;
      if (w == 1) e.timestamp_ns ^= 1ull << 32;
      if (w == 2) e.code = static_cast<std::uint16_t>(e.code ^ 1);
      if (w == 3) e.a ^= 1;
      if (w == 4) e.b ^= 1;
      if (w == 5) g.dropped += 1;
      if (log_digest(&g) == d0 || log_digest(&g) != oracle_digest(g)) ++bad;
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("sequences against the model, the oracle and the digest", "[diag_log][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  constexpr std::size_t kT = sizeof(kRefPushTs) / sizeof(kRefPushTs[0]);
  constexpr std::size_t kC = sizeof(kRefPushCodes) / sizeof(kRefPushCodes[0]);
  constexpr std::size_t kV = sizeof(kRefPushVals) / sizeof(kRefPushVals[0]);
  for (std::uint64_t seed : kRefSeqSeeds) {
    DiagLog g;
    Model m;
    std::uint64_t x = seed;
    for (std::size_t k = 0; k < kRefSeqN; ++k) {
      x = x * 6364136223846793005ull + 1442695040888963407ull;
      const std::uint64_t r = x >> 33;
      if (r % 61 == 0) {
        if (!log_clear(&g)) ++bad;
        m.clear();
        f.byte(2);
      } else {
        const std::uint64_t ts = (r % 5 == 0) ? kRefPushTs[r % kT] : r * 1000003ull;
        const std::uint16_t code = (r % 3 == 0) ? kRefPushCodes[(r >> 8) % kC] : static_cast<std::uint16_t>((r >> 4) & 0xFFFFu);
        const std::int32_t a = (r % 4 == 0) ? kRefPushVals[(r >> 12) % kV] : static_cast<std::int32_t>(static_cast<std::uint32_t>(r >> 3));
        const std::int32_t b = (r % 2 == 0) ? kRefPushVals[(r >> 16) % kV] : static_cast<std::int32_t>(static_cast<std::uint32_t>(r * 7));
        if (!log_push(&g, ts, code, a, b)) ++bad;
        m.push(mk(ts, code, a, b));
        f.byte(1);
      }
      // model check: the ring reads back exactly the model list
      if (log_size(&g) != m.list.size() || g.dropped != m.dropped) ++bad;
      for (std::size_t i = 0; i < m.list.size(); ++i) {
        LogEntry e = kSentinel;
        if (!log_read(&g, static_cast<std::uint32_t>(i), &e) || !same_entry(e, m.list[i])) ++bad;
      }
      if (log_digest(&g) != oracle_digest(g)) ++bad;
      f.byte(static_cast<std::uint8_t>(log_size(&g)));
      f.u64(log_digest(&g));
      f.u32(g.next);
      f.u64(g.dropped);
      LogEntry e = kSentinel;
      const bool ok = log_read(&g, static_cast<std::uint32_t>((r >> 20) % 70), &e);
      feed_read(&f, ok, e);
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_seq);
  CHECK(f.h == kRefDigest_seq);
}

TEST_CASE("hand-built objects against the oracle and the digest", "[diag_log][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t kind : kRefKinds) {
    for (std::uint32_t c : kRefCounts) {
      for (std::uint32_t nx : kRefNexts) {
        for (std::uint64_t d : kRefDroppeds) {
          DiagLog g;
          fill(&g, kind);
          g.count = c;
          g.next = nx;
          g.dropped = d;
          const bool ok_state = valid_state(g);
          const std::vector<LogEntry> win = ok_state ? oracle_window(g) : std::vector<LogEntry>();
          if (log_size(&g) != (ok_state ? c : 0u)) ++bad;
          if (log_digest(&g) != oracle_digest(g)) ++bad;
          f.byte(static_cast<std::uint8_t>(log_size(&g)));
          f.u64(log_digest(&g));
          for (std::uint32_t i : kRefReadIndexes) {
            LogEntry e = kSentinel;
            const bool ok = log_read(&g, i, &e);
            if (ok != (ok_state && i < win.size())) ++bad;
            if (ok && !same_entry(e, win[i])) ++bad;
            if (!ok && !same_entry(e, kSentinel)) ++bad;
            feed_read(&f, ok, e);
          }
          // push: model the repair, the write at next and the drop rule by hand
          DiagLog want = g;
          if (!ok_state) {
            want.count = 0;
            want.next = 0;
          }
          want.entries[want.next] = mk(0xAAAA5555AAAA5555ull, 777, -5, 6);
          want.next = (want.next + 1) % kLogCapacity;
          if (want.count < kLogCapacity) {
            want.count += 1;
          } else if (want.dropped != UINT64_MAX) {
            want.dropped += 1;
          }
          if (!log_push(&g, 0xAAAA5555AAAA5555ull, 777, -5, 6) || !same_log(g, want)) ++bad;
          feed_state(&f, g);
          f.u64(log_digest(&g));
          ++n;
        }
      }
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_hand);
  CHECK(f.h == kRefDigest_hand);
}

TEST_CASE("dropped saturation digest", "[diag_log][sweep]") {
  Fnv f;
  std::size_t n = 0;
  DiagLog g;
  for (std::uint32_t i = 0; i < kLogCapacity; ++i) CHECK(log_push(&g, i, static_cast<std::uint16_t>(i), static_cast<std::int32_t>(i), static_cast<std::int32_t>(i)));
  for (std::uint64_t start : {UINT64_MAX - 3, UINT64_MAX - 1, UINT64_MAX}) {
    g.dropped = start;
    for (std::uint32_t i = 0; i < 5; ++i) {
      CHECK(log_push(&g, 100 + i, static_cast<std::uint16_t>(i), static_cast<std::int32_t>(i), static_cast<std::int32_t>(i)));
      const std::uint64_t exp = (UINT64_MAX - start >= i + 1) ? start + i + 1 : UINT64_MAX;
      CHECK(g.dropped == exp);
      f.u64(g.dropped);
      f.u64(log_digest(&g));
      ++n;
    }
  }
  CHECK(n == kRefCases_sat);
  CHECK(f.h == kRefDigest_sat);
}
