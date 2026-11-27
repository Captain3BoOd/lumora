// Tests for core/ring. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/byte_ring_ref.py, an independent Python implementation
// (a data array with every index computed one byte at a time). Regenerate it with: python3 tools/byte_ring_ref.py
// The tests also carry a naive oracle that shares no code with core/ring/byte_ring.cpp: a std::vector FIFO (push at the back, erase
// at the front) with the head tracked separately.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "byte_ring.h"

using namespace lumora::ring;

namespace {

// BEGIN GENERATED REFERENCE (tools/byte_ring_ref.py)
const std::uint32_t kRefHeads[] = {0u, 1u, 2047u, 4094u, 4095u, 4096u, 4097u, 4294967295u};
const std::uint32_t kRefCounts[] = {0u, 1u, 2048u, 4095u, 4096u, 4097u, 4294967295u};
const std::uint32_t kRefNs[] = {0u, 1u, 100u, 4096u, 4294967295u};
const std::uint32_t kRefOffs[] = {0u, 1u, 2048u, 4095u, 4096u, 4294967295u};
const std::size_t kRefOps = 4000;
const std::uint64_t kRefDigest_ops = 0x3AF0239EC56CE73Dull;
const std::size_t kRefCases_ops = 4000;
const std::uint64_t kRefDigest_table = 0xA0D052DAC44DCACFull;
const std::size_t kRefCases_table = 2688;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;
constexpr std::uint32_t kCap = 4096;
constexpr std::uint8_t kSentinel = 0xEE;

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

std::uint32_t checksum(const std::uint8_t* p, std::size_t n) {
  std::uint32_t s = 0;
  for (std::size_t i = 0; i < n; ++i) s = s * 31u + p[i] + 1u;
  return s;
}

void fill_pattern(ByteRing* r) {
  for (std::uint32_t i = 0; i < kCap; ++i) r->data[i] = static_cast<std::uint8_t>((i * 13u + 7u) & 255u);
}

ByteRing make(std::uint32_t head, std::uint32_t count) {
  ByteRing r;
  fill_pattern(&r);
  r.head = head;
  r.count = count;
  return r;
}

bool same_data(const ByteRing& a, const ByteRing& b) {
  for (std::uint32_t i = 0; i < kCap; ++i)
    if (a.data[i] != b.data[i]) return false;
  return true;
}

std::vector<std::uint8_t> bytes_to(std::uint32_t n, std::uint32_t seed) {
  std::vector<std::uint8_t> v(n);
  for (std::uint32_t i = 0; i < n; ++i) v[i] = static_cast<std::uint8_t>(seed + i * 7u);
  return v;
}

// ---- naive oracle -------------------------------------------------------------------------------------------------------------
struct Oracle {
  std::vector<std::uint8_t> live;
  std::uint32_t head = 0;
  std::uint32_t write(const std::uint8_t* src, std::uint32_t n) {
    const std::size_t room = 4096 - live.size();
    const std::size_t k = n < room ? n : room;
    for (std::size_t i = 0; i < k; ++i) live.push_back(src[i]);
    return static_cast<std::uint32_t>(k);
  }
  std::uint32_t read(std::uint8_t* dst, std::uint32_t n) {
    const std::size_t k = n < live.size() ? n : live.size();
    for (std::size_t i = 0; i < k; ++i) dst[i] = live[i];
    drop(k);
    return static_cast<std::uint32_t>(k);
  }
  std::uint32_t peek(std::uint8_t* dst, std::uint32_t n, std::uint32_t offset) const {
    if (offset >= live.size()) return 0;
    const std::size_t avail = live.size() - offset;
    const std::size_t k = n < avail ? n : avail;
    for (std::size_t i = 0; i < k; ++i) dst[i] = live[offset + i];
    return static_cast<std::uint32_t>(k);
  }
  std::uint32_t skip(std::uint32_t n) {
    const std::size_t k = n < live.size() ? n : live.size();
    drop(k);
    return static_cast<std::uint32_t>(k);
  }
  void drop(std::size_t k) {
    live.erase(live.begin(), live.begin() + static_cast<std::ptrdiff_t>(k));
    head = static_cast<std::uint32_t>((head + k) % 4096);
  }
};

// n and offset generators (the reference uses the same draws in the same order)
std::uint32_t gen_n(Lcg& g) {
  const std::uint32_t kind = g.u32() % 7u;
  if (kind == 0) return 0;
  const std::uint32_t v = g.u32();
  switch (kind) {
    case 1: return v % 8u;
    case 2: return v % 64u;
    case 3: return v % 1024u;
    case 4: return v % 5000u;
    case 5: return 4090u + v % 12u;
    default: return 0xFFFFFFFFu - v % 3u;
  }
}

std::uint32_t gen_offset(Lcg& g, std::uint32_t count) {
  const std::uint32_t kind = g.u32() % 5u;
  if (kind == 0) return 0;
  const std::uint32_t v = g.u32();
  switch (kind) {
    case 1: return v % 16u;
    case 2: return v % (count + 1u);
    case 3: return count + v % 3u - 1u;
    default: return 0xFFFFFFFFu - v % 3u;
  }
}

// number of bytes of dst to checksum: the returned bytes and four sentinels after them, at most the buffer
std::size_t tail_span(std::uint32_t res) { return res < kCap ? (res + 4u < kCap ? res + 4u : kCap) : kCap; }

struct OpsStats {
  std::size_t bad = 0, writes = 0, partial_writes = 0, reads = 0, peeks = 0, skips = 0, wraps = 0, clamped = 0, forged = 0, resets = 0;
};

// Runs the scripted operations, compares every step with the oracle, and feeds the digest.
void run_ops(Fnv* f, OpsStats* st) {
  Lcg g(51);
  ByteRing r;
  fill_pattern(&r);
  Oracle o;
  std::vector<std::uint8_t> src(kCap), dst(kCap + 8), odst(kCap + 8);
  for (std::size_t i = 0; i < kRefOps; ++i) {
    const std::uint32_t sel = g.u32() % 8u;
    std::uint32_t res = 0, ck = 0;
    const std::uint32_t before_head = r.head;
    const std::uint32_t before_count = r.count;
    for (auto& b : dst) b = kSentinel;
    for (auto& b : odst) b = kSentinel;
    if (sel <= 2) {
      const std::uint32_t n = gen_n(g);
      const std::uint32_t base = g.u32();
      for (std::uint32_t j = 0; j < kCap; ++j) src[j] = static_cast<std::uint8_t>(base + j * 7u);
      const std::uint32_t want = o.write(src.data(), n);
      res = write(&r, src.data(), n);
      if (res != want) ++st->bad;
      ++st->writes;
      if (n > want && want > 0) ++st->partial_writes;
      if (want > 0 && (before_head + before_count) % kCap + want > kCap) ++st->wraps;
    } else if (sel <= 4) {
      const std::uint32_t n = gen_n(g);
      const std::uint32_t want = o.read(odst.data(), n);
      res = read(&r, dst.data(), n);
      if (res != want || dst != odst) ++st->bad;
      ck = checksum(dst.data(), res);
      ++st->reads;
      if (n > want) ++st->clamped;
    } else if (sel == 5) {
      const std::uint32_t n = gen_n(g);
      const std::uint32_t off = gen_offset(g, r.count);
      const std::uint32_t want = o.peek(odst.data(), n, off);
      res = peek(&r, dst.data(), n, off);
      if (res != want || dst != odst) ++st->bad;
      ck = checksum(dst.data(), res);
      ++st->peeks;
    } else if (sel == 6) {
      const std::uint32_t n = gen_n(g);
      const std::uint32_t want = o.skip(n);
      res = skip(&r, n);
      if (res != want) ++st->bad;
      ++st->skips;
      if (n > want) ++st->clamped;
    } else {
      const std::uint32_t sub = g.u32() % 16u;
      if (sub == 0) {
        reset(&r);
        o.live.clear();
        o.head = 0;
        ++st->resets;
      } else if (sub == 1) {
        r.head = g.u32() % kCap;
        const std::uint32_t v = g.u32();
        r.count = v % 3u == 0 ? v % (kCap + 1u) : 0u;
        o.head = r.head;
        o.live.clear();
        for (std::uint32_t j = 0; j < r.count; ++j) o.live.push_back(r.data[(r.head + j) % kCap]);
        ++st->forged;
      } else {
        res = size(&r);
        ck = free_space(&r);
        if (res != o.live.size() || ck != kCap - o.live.size()) ++st->bad;
      }
    }
    if (r.head >= kCap || r.count > kCap) {  // a corrupted ring: stop before sizing anything from it
      ++st->bad;
      return;
    }
    if (r.head != o.head || r.count != o.live.size()) ++st->bad;
    std::vector<std::uint8_t> live(r.count);
    for (std::uint32_t j = 0; j < r.count; ++j) live[j] = r.data[(r.head + j) % kCap];
    if (live != o.live) ++st->bad;
    f->byte(static_cast<std::uint8_t>(sel));
    f->u32(res);
    f->u32(r.head);
    f->u32(r.count);
    f->u32(ck);
    f->u32(checksum(live.data(), live.size()));
    if (i % 64 == 63) f->u32(checksum(r.data, kCap));
  }
}

}  // namespace

TEST_CASE("constants and defaults", "[ring]") {
  static_assert(kCapacity == 4096, "capacity");
  static_assert(sizeof(ByteRing::data) == 4096, "data size");
  ByteRing r;
  CHECK(r.head == 0);
  CHECK(r.count == 0);
  CHECK(kCapacity == 4096u);
  CHECK(size(&r) == 0u);
  CHECK(free_space(&r) == 4096u);
}

TEST_CASE("null ring, null pointers and n == 0", "[ring][null]") {
  std::uint8_t buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  CHECK(size(nullptr) == 0u);
  CHECK(free_space(nullptr) == 0u);
  CHECK(write(nullptr, buf, 8) == 0u);
  CHECK(read(nullptr, buf, 8) == 0u);
  CHECK(peek(nullptr, buf, 8, 0) == 0u);
  CHECK(skip(nullptr, 8) == 0u);
  reset(nullptr);  // ignored
  CHECK(buf[0] == 1);
  ByteRing r = make(10, 20);
  const ByteRing before = r;
  CHECK(write(&r, nullptr, 5) == 0u);
  CHECK(read(&r, nullptr, 5) == 0u);
  CHECK(peek(&r, nullptr, 5, 0) == 0u);
  CHECK(peek(&r, nullptr, 5, 3) == 0u);
  CHECK(write(&r, nullptr, 0) == 0u);
  CHECK(read(&r, nullptr, 0) == 0u);
  CHECK(peek(&r, nullptr, 0, 0) == 0u);
  CHECK(write(&r, buf, 0) == 0u);
  CHECK(read(&r, buf, 0) == 0u);
  CHECK(peek(&r, buf, 0, 0) == 0u);
  CHECK(skip(&r, 0) == 0u);
  CHECK(r.head == before.head);
  CHECK(r.count == before.count);
  CHECK(same_data(r, before));
  CHECK(buf[0] == 1);
  CHECK(buf[7] == 8);
}

TEST_CASE("write: basic, partial, full", "[ring][write]") {
  ByteRing r = make(0, 0);
  const std::vector<std::uint8_t> a = bytes_to(100, 3);
  CHECK(write(&r, a.data(), 100) == 100u);
  CHECK(r.head == 0);
  CHECK(r.count == 100);
  CHECK(size(&r) == 100u);
  CHECK(free_space(&r) == 3996u);
  for (std::uint32_t i = 0; i < 100; ++i) CHECK(r.data[i] == a[i]);
  CHECK(r.data[100] == static_cast<std::uint8_t>((100u * 13u + 7u) & 255u));  // next byte untouched
  // partial write: the first bytes of src are kept
  const std::vector<std::uint8_t> big = bytes_to(5000, 9);
  CHECK(write(&r, big.data(), 5000) == 3996u);
  CHECK(r.count == 4096);
  CHECK(size(&r) == 4096u);
  CHECK(free_space(&r) == 0u);
  for (std::uint32_t i = 0; i < 3996; ++i) CHECK(r.data[100 + i] == big[i]);
  // a full ring takes nothing and changes nothing
  const ByteRing full = r;
  CHECK(write(&r, big.data(), 1) == 0u);
  CHECK(write(&r, big.data(), 0xFFFFFFFFu) == 0u);
  CHECK(r.head == full.head);
  CHECK(r.count == full.count);
  CHECK(same_data(r, full));
  // exact fit and one more
  ByteRing e = make(0, 4000);
  CHECK(write(&e, big.data(), 96) == 96u);
  CHECK(e.count == 4096);
  ByteRing e2 = make(0, 4000);
  CHECK(write(&e2, big.data(), 97) == 96u);
  CHECK(e2.count == 4096);
  // n near UINT32_MAX is clamped, not an error
  ByteRing m = make(5, 10);
  CHECK(write(&m, big.data(), 0xFFFFFFFFu) == 4086u);
  CHECK(m.count == 4096);
  CHECK(m.head == 5);
}

TEST_CASE("write wraps around the end of data", "[ring][write][wrap]") {
  ByteRing r = make(4090, 0);
  const std::vector<std::uint8_t> a = bytes_to(20, 100);
  CHECK(write(&r, a.data(), 20) == 20u);
  CHECK(r.head == 4090);
  CHECK(r.count == 20);
  for (std::uint32_t i = 0; i < 20; ++i) CHECK(r.data[(4090 + i) % 4096] == a[i]);
  CHECK(r.data[14] == static_cast<std::uint8_t>((14u * 13u + 7u) & 255u));  // first untouched byte after the wrap
  CHECK(r.data[4089] == static_cast<std::uint8_t>((4089u * 13u + 7u) & 255u));
  // tail index computed from head + count, with the sum above 4096
  ByteRing t = make(4000, 200);  // next byte goes to (4000 + 200) % 4096 = 104
  const std::vector<std::uint8_t> b = bytes_to(5, 1);
  CHECK(write(&t, b.data(), 5) == 5u);
  for (std::uint32_t i = 0; i < 5; ++i) CHECK(t.data[104 + i] == b[i]);
  CHECK(t.data[103] == static_cast<std::uint8_t>((103u * 13u + 7u) & 255u));
  CHECK(t.data[109] == static_cast<std::uint8_t>((109u * 13u + 7u) & 255u));
  // tail exactly at the end of data (index 4095), and exactly at 0
  ByteRing e = make(4000, 95);
  const std::vector<std::uint8_t> c = bytes_to(3, 50);
  CHECK(write(&e, c.data(), 3) == 3u);
  CHECK(e.data[4095] == c[0]);
  CHECK(e.data[0] == c[1]);
  CHECK(e.data[1] == c[2]);
  ByteRing z = make(4000, 96);  // tail (4000 + 96) % 4096 = 0
  CHECK(write(&z, c.data(), 3) == 3u);
  CHECK(z.data[0] == c[0]);
  CHECK(z.data[2] == c[2]);
  // a partial write on a wrapped ring
  ByteRing w = make(100, 4090);  // tail = (100 + 4090) % 4096 = 94, 6 free
  const std::vector<std::uint8_t> d = bytes_to(50, 77);
  CHECK(write(&w, d.data(), 50) == 6u);
  for (std::uint32_t i = 0; i < 6; ++i) CHECK(w.data[94 + i] == d[i]);
  CHECK(w.count == 4096);
}

TEST_CASE("read: order, partial, empty keeps head", "[ring][read]") {
  ByteRing r = make(0, 0);
  const std::vector<std::uint8_t> a = bytes_to(50, 5);
  REQUIRE(write(&r, a.data(), 50) == 50u);
  std::uint8_t out[60];
  for (auto& b : out) b = kSentinel;
  CHECK(read(&r, out, 20) == 20u);
  for (std::uint32_t i = 0; i < 20; ++i) CHECK(out[i] == a[i]);
  CHECK(out[20] == kSentinel);
  CHECK(r.head == 20);
  CHECK(r.count == 30);
  // n larger than stored: clamped
  for (auto& b : out) b = kSentinel;
  CHECK(read(&r, out, 0xFFFFFFFFu) == 30u);
  for (std::uint32_t i = 0; i < 30; ++i) CHECK(out[i] == a[20 + i]);
  CHECK(out[30] == kSentinel);
  CHECK(r.head == 50);  // not reset when the ring becomes empty
  CHECK(r.count == 0);
  // an empty ring reads nothing and leaves head alone
  CHECK(read(&r, out, 10) == 0u);
  CHECK(r.head == 50);
  CHECK(out[0] == a[20]);
  // head wraps modulo 4096
  ByteRing w = make(4090, 20);
  std::uint8_t big[20];
  CHECK(read(&w, big, 20) == 20u);
  for (std::uint32_t i = 0; i < 20; ++i) CHECK(big[i] == w.data[(4090 + i) % 4096]);
  CHECK(w.head == 14);
  CHECK(w.count == 0);
  ByteRing x = make(4095, 4096);
  std::vector<std::uint8_t> all(4096);
  CHECK(read(&x, all.data(), 4096) == 4096u);
  CHECK(x.head == 4095);  // 4095 + 4096 wraps back to 4095
  CHECK(x.count == 0);
  for (std::uint32_t i = 0; i < 4096; ++i) CHECK(all[i] == x.data[(4095 + i) % 4096]);
  // reads do not change the data bytes
  CHECK(x.data[0] == static_cast<std::uint8_t>(7));
}

TEST_CASE("peek: offsets, clamping, no change", "[ring][peek]") {
  ByteRing r = make(4090, 30);
  const ByteRing before = r;
  std::uint8_t out[40];
  for (auto& b : out) b = kSentinel;
  CHECK(peek(&r, out, 10, 0) == 10u);
  for (std::uint32_t i = 0; i < 10; ++i) CHECK(out[i] == r.data[(4090 + i) % 4096]);
  CHECK(out[10] == kSentinel);
  for (auto& b : out) b = kSentinel;
  CHECK(peek(&r, out, 40, 25) == 5u);  // min(n, size - offset)
  for (std::uint32_t i = 0; i < 5; ++i) CHECK(out[i] == r.data[(4090 + 25 + i) % 4096]);
  CHECK(out[5] == kSentinel);
  for (auto& b : out) b = kSentinel;
  CHECK(peek(&r, out, 40, 29) == 1u);
  CHECK(out[0] == r.data[(4090 + 29) % 4096]);
  CHECK(out[1] == kSentinel);
  CHECK(peek(&r, out, 40, 30) == 0u);  // offset == size
  CHECK(peek(&r, out, 40, 31) == 0u);
  CHECK(peek(&r, out, 40, 0xFFFFFFFFu) == 0u);
  CHECK(peek(&r, out, 0, 0) == 0u);
  CHECK(peek(&r, out, 0xFFFFFFFFu, 0) == 30u);  // clamped to the stored 30 bytes (out holds 40)
  CHECK(r.head == before.head);
  CHECK(r.count == before.count);
  CHECK(same_data(r, before));
}

TEST_CASE("skip: clamping and head", "[ring][skip]") {
  ByteRing r = make(4000, 300);
  CHECK(skip(&r, 100) == 100u);
  CHECK(r.head == 4100 % 4096);
  CHECK(r.count == 200);
  CHECK(skip(&r, 0xFFFFFFFFu) == 200u);
  CHECK(r.head == (4100 + 200) % 4096);
  CHECK(r.count == 0);
  CHECK(skip(&r, 5) == 0u);
  CHECK(r.head == (4100 + 200) % 4096);
  ByteRing f = make(4095, 4096);
  CHECK(skip(&f, 4096) == 4096u);
  CHECK(f.head == 4095);
  CHECK(f.count == 0);
  ByteRing g = make(10, 4096);
  CHECK(skip(&g, 1) == 1u);
  CHECK(g.head == 11);
  CHECK(g.count == 4095);
  CHECK(free_space(&g) == 1u);
}

TEST_CASE("invalid rings return 0 and change nothing", "[ring][invalid]") {
  const std::uint32_t heads[] = {4096u, 4097u, 0xFFFFFFFFu, 0u, 100u};
  const std::uint32_t counts[] = {0u, 5u, 4097u, 0xFFFFFFFFu};
  std::vector<std::uint8_t> src = bytes_to(4096, 1);
  for (std::uint32_t h : heads)
    for (std::uint32_t c : counts) {
      const bool valid = h < 4096u && c <= 4096u;
      ByteRing r = make(h, c);
      const ByteRing before = r;
      std::uint8_t out[16];
      for (auto& b : out) b = kSentinel;
      if (!valid) {
        CHECK(size(&r) == 0u);
        CHECK(free_space(&r) == 0u);
        CHECK(write(&r, src.data(), 10) == 0u);
        CHECK(read(&r, out, 10) == 0u);
        CHECK(peek(&r, out, 10, 0) == 0u);
        CHECK(skip(&r, 10) == 0u);
        CHECK(r.head == before.head);
        CHECK(r.count == before.count);
        CHECK(same_data(r, before));
        CHECK(out[0] == kSentinel);
        reset(&r);  // an invalid ring is reset too
        CHECK(r.head == 0);
        CHECK(r.count == 0);
        CHECK(same_data(r, before));
      } else {
        CHECK(size(&r) == c);
        CHECK(free_space(&r) == 4096u - c);
      }
    }
}

TEST_CASE("reset", "[ring][reset]") {
  ByteRing r = make(123, 456);
  const ByteRing before = r;
  reset(&r);
  CHECK(r.head == 0);
  CHECK(r.count == 0);
  CHECK(same_data(r, before));  // data not touched
  CHECK(size(&r) == 0u);
  CHECK(free_space(&r) == 4096u);
}

TEST_CASE("write then read round trip at every head", "[ring][roundtrip]") {
  std::size_t bad = 0;
  const std::vector<std::uint8_t> src = bytes_to(4096, 17);
  std::vector<std::uint8_t> out(4096 + 4);
  for (std::uint32_t head = 0; head < kCap; head += 37) {
    for (std::uint32_t n : {1u, 2u, 255u, 2048u, 4095u, 4096u}) {
      ByteRing r = make(head, 0);
      for (auto& b : out) b = kSentinel;
      if (write(&r, src.data(), n) != n) ++bad;
      if (peek(&r, out.data(), n, 0) != n) ++bad;
      for (std::uint32_t i = 0; i < n; ++i)
        if (out[i] != src[i]) ++bad;
      for (auto& b : out) b = kSentinel;
      if (read(&r, out.data(), n) != n) ++bad;
      for (std::uint32_t i = 0; i < n; ++i)
        if (out[i] != src[i]) ++bad;
      if (out[n] != kSentinel) ++bad;
      if (r.head != (head + n) % kCap || r.count != 0) ++bad;
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("scripted operations match the oracle and the reference digest", "[ring][sweep]") {
  Fnv f;
  OpsStats st;
  run_ops(&f, &st);
  CHECK(st.bad == 0);
  CHECK(st.writes > 1000);
  CHECK(st.reads > 500);
  CHECK(st.peeks > 200);
  CHECK(st.skips > 100);
  CHECK(st.partial_writes > 20);
  CHECK(st.wraps > 100);
  CHECK(st.clamped > 100);
  CHECK(st.forged > 10);
  CHECK(st.resets > 10);
  CHECK(f.h == kRefDigest_ops);
}

TEST_CASE("valid and invalid ring table digest", "[ring][sweep]") {
  Fnv f;
  std::size_t cases = 0;
  std::vector<std::uint8_t> src(kCap);
  for (std::uint32_t j = 0; j < kCap; ++j) src[j] = static_cast<std::uint8_t>(j * 5u + 3u);
  std::vector<std::uint8_t> dst(kCap + 8);
  const auto clear_dst = [&]() {
    for (auto& b : dst) b = kSentinel;
  };
  for (const std::uint32_t head : kRefHeads)
    for (const std::uint32_t count : kRefCounts) {
      ByteRing r = make(head, count);
      f.u32(size(&r));
      f.u32(free_space(&r));
      cases += 2;
      for (const std::uint32_t n : kRefNs) {
        r = make(head, count);
        const std::uint32_t res = write(&r, src.data(), n);
        f.u32(res);
        f.u32(r.head);
        f.u32(r.count);
        f.u32(checksum(r.data, kCap));
        ++cases;
      }
      for (const std::uint32_t n : kRefNs) {
        r = make(head, count);
        clear_dst();
        const std::uint32_t res = read(&r, dst.data(), n);
        f.u32(res);
        f.u32(r.head);
        f.u32(r.count);
        f.u32(checksum(dst.data(), tail_span(res)));
        ++cases;
      }
      for (const std::uint32_t n : kRefNs)
        for (const std::uint32_t off : kRefOffs) {
          r = make(head, count);
          clear_dst();
          const std::uint32_t res = peek(&r, dst.data(), n, off);
          f.u32(res);
          f.u32(r.head);
          f.u32(r.count);
          f.u32(checksum(dst.data(), tail_span(res)));
          ++cases;
        }
      for (const std::uint32_t n : kRefNs) {
        r = make(head, count);
        const std::uint32_t res = skip(&r, n);
        f.u32(res);
        f.u32(r.head);
        f.u32(r.count);
        ++cases;
      }
      r = make(head, count);
      reset(&r);
      f.u32(r.head);
      f.u32(r.count);
      f.u32(checksum(r.data, kCap));
      ++cases;
    }
  CHECK(cases == kRefCases_table);
  CHECK(f.h == kRefDigest_table);
}
