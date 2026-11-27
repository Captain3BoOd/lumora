// Tests for core/sortedops. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/sortedops_ref.py, an independent Python implementation (linear
// definitions, sorted(a + b), itertools.groupby). Regenerate it with: python3 tools/sortedops_ref.py
// The tests also carry a naive oracle that shares no code with core/sortedops/sortedops.cpp: bounds by a linear scan, merge by the rank of
// every element (its index plus the number of elements of the other array that go before it), unique by comparing with the input neighbour.
// Inputs are heap buffers of exactly the stated length (AddressSanitizer sees any access past the end); the write-rule runs pad the output
// with canaries. Inputs are always ascending (the header's precondition); unsorted input is never tested.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

#include "sortedops.h"

using namespace lumora::sortedops;

namespace {

// BEGIN GENERATED REFERENCE (tools/sortedops_ref.py)
const std::uint32_t kRefV4[] = {0x0u, 0x1u, 0x2u, 0xFFFFFFFFu};
const std::uint32_t kRefKeys7[] = {0x0u, 0x1u, 0x2u, 0x3u, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu};
const std::uint32_t kRefBoundMax = 6, kRefMergeMax = 5, kRefUniqMax = 7;
const std::uint32_t kRefRandSeeds = 300, kRefRandLen = 301, kRefRandProbes = 40;
const std::uint32_t kRefGapMods[] = {1u, 3u, 1048576u};
const std::uint64_t kRefDigest_bound = 0x50A73549F53DCD45ull;
const std::size_t kRefCases_bound = 1470, kRefOps_bound = 2940;
const std::uint64_t kRefDigest_merge = 0x73EE96C55D921D3Dull;
const std::size_t kRefCases_merge = 158760, kRefOps_merge = 158760;
const std::uint64_t kRefDigest_uniq = 0xDB13845BC5D6CED5ull;
const std::size_t kRefCases_uniq = 330, kRefOps_uniq = 330;
const std::uint64_t kRefDigest_rand = 0x6E0143D1046608F1ull;
const std::size_t kRefCases_rand = 300, kRefOps_rand = 25200;
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kMax32 = 0xFFFFFFFFu;

using Vec = std::vector<std::uint32_t>;

std::uint32_t code(OpResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

// A heap buffer holding a copy of v, exactly v.size() elements (a valid non-null pointer even when empty).
struct Buf {
  std::unique_ptr<std::uint32_t[]> p;
  std::uint32_t n;
  explicit Buf(const Vec& v, std::uint32_t pad = 0, std::uint32_t fill = 0) : p(new std::uint32_t[v.size() + pad]), n(static_cast<std::uint32_t>(v.size())) {
    for (std::size_t i = 0; i < v.size(); ++i) p[i] = v[i];
    for (std::uint32_t i = 0; i < pad; ++i) p[v.size() + i] = fill + i;
  }
  const std::uint32_t* get() const { return p.get(); }
  std::uint32_t* get() { return p.get(); }
};

std::uint32_t naive_lower(const Vec& a, std::uint32_t key) {
  std::uint32_t i = 0;
  while (i < a.size() && a[i] < key) ++i;
  return i;
}

std::uint32_t naive_upper(const Vec& a, std::uint32_t key) {
  std::uint32_t i = 0;
  while (i < a.size() && !(a[i] > key)) ++i;
  return i;
}

Vec naive_merge(const Vec& a, const Vec& b) {
  Vec out(a.size() + b.size(), 0u);
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::size_t before = 0;
    for (std::size_t j = 0; j < b.size(); ++j) before += b[j] < a[i] ? 1u : 0u;
    out[i + before] = a[i];
  }
  for (std::size_t j = 0; j < b.size(); ++j) {
    std::size_t before = 0;
    for (std::size_t i = 0; i < a.size(); ++i) before += a[i] <= b[j] ? 1u : 0u;
    out[j + before] = b[j];
  }
  return out;
}

Vec naive_unique(const Vec& a) {
  Vec out;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (i == 0 || a[i] != a[i - 1]) out.push_back(a[i]);
  }
  return out;
}

void do_bound(Fold* f, std::size_t* bad, const Vec& a, std::uint32_t key) {
  const Buf buf(a);
  std::uint32_t lo = kSentinel, up = kSentinel;
  const OpResult r1 = lower_bound_u32(buf.get(), buf.n, key, &lo);
  const OpResult r2 = upper_bound_u32(buf.get(), buf.n, key, &up);
  if (r1 != OpResult::Ok || r2 != OpResult::Ok || lo != naive_lower(a, key) || up != naive_upper(a, key)) ++*bad;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (buf.get()[i] != a[i]) ++*bad;  // reads only
  }
  if (f != nullptr) {
    f->ops += 2;
    f->add((static_cast<std::uint64_t>(lo) << 32) | up);
  }
}

void do_merge(Fold* f, std::size_t* bad, const Vec& a, const Vec& b, std::uint32_t cap, std::uint32_t pad) {
  const Buf ba(a), bb(b);
  const std::uint32_t total = static_cast<std::uint32_t>(a.size() + b.size());
  Buf dst(Vec(total, 0u), pad, 0xC0DE0000u);
  for (std::uint32_t i = 0; i < total; ++i) dst.get()[i] = 0xC0DE1000u + i;  // canaries inside the output range too
  std::uint32_t len = kSentinel;
  const OpResult r = merge_sorted_u32(ba.get(), ba.n, bb.get(), bb.n, dst.get(), cap, &len);
  std::uint32_t out_len = 0;
  if (cap < total) {
    if (r != OpResult::BufferTooSmall || len != kSentinel) ++*bad;
    for (std::uint32_t i = 0; i < total; ++i) {
      if (dst.get()[i] != 0xC0DE1000u + i) ++*bad;  // a failure writes nothing
    }
  } else {
    const Vec want = naive_merge(a, b);
    if (r != OpResult::Ok || len != total) ++*bad;
    for (std::uint32_t i = 0; i < total; ++i) {
      if (dst.get()[i] != want[i]) ++*bad;
    }
    out_len = len;
  }
  for (std::uint32_t i = 0; i < pad; ++i) {
    if (dst.get()[total + i] != 0xC0DE0000u + i) ++*bad;  // nothing past na + nb
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ba.get()[i] != a[i]) ++*bad;
  }
  for (std::size_t i = 0; i < b.size(); ++i) {
    if (bb.get()[i] != b[i]) ++*bad;
  }
  if (f != nullptr) {
    ++f->ops;
    f->add((static_cast<std::uint64_t>(code(r)) << 32) | (r == OpResult::Ok ? out_len : 0u));
    if (r == OpResult::Ok) {
      for (std::uint32_t i = 0; i < total; ++i) f->add(dst.get()[i]);
    }
  }
}

void do_unique(Fold* f, std::size_t* bad, const Vec& a, std::uint32_t pad) {
  Buf buf(a, pad, 0xC0DE0000u);
  std::uint32_t len = kSentinel;
  const OpResult r = unique_sorted_u32(buf.get(), buf.n, &len);
  const Vec want = naive_unique(a);
  if (r != OpResult::Ok || len != want.size()) {
    ++*bad;
  } else {
    for (std::size_t i = 0; i < want.size(); ++i) {
      if (buf.get()[i] != want[i]) ++*bad;  // only the prefix of length out_len is specified
    }
  }
  for (std::uint32_t i = 0; i < pad; ++i) {
    if (buf.get()[a.size() + i] != 0xC0DE0000u + i) ++*bad;  // nothing at or above n
  }
  if (f != nullptr) {
    ++f->ops;
    f->add((static_cast<std::uint64_t>(code(r)) << 32) | len);
    for (std::uint32_t i = 0; i < len && i < a.size(); ++i) f->add(buf.get()[i]);
  }
}

// Calls fn(array) for every non-decreasing array of length 0..max over the 4-value alphabet, in the order of
// itertools.combinations_with_replacement by length.
template <typename Fn>
void for_each_array(std::uint32_t max_len, Fn fn) {
  for (std::uint32_t n = 0; n <= max_len; ++n) {
    std::vector<std::uint32_t> idx(n, 0u);
    for (;;) {
      Vec a(n);
      for (std::uint32_t i = 0; i < n; ++i) a[i] = kRefV4[idx[i]];
      fn(a);
      std::uint32_t i = n;
      while (i > 0u && idx[i - 1u] == 3u) --i;
      if (i == 0u) break;
      const std::uint32_t v = idx[i - 1u] + 1u;
      for (std::uint32_t j = i - 1u; j < n; ++j) idx[j] = v;
    }
  }
}

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

}  // namespace

TEST_CASE("header anchors", "[sortedops]") {
  CHECK(kMaxCount == (1u << 26));
  CHECK(code(OpResult::Ok) == 0u);
  CHECK(code(OpResult::NullArgument) == 1u);
  CHECK(code(OpResult::TooLarge) == 2u);
  CHECK(code(OpResult::BufferTooSmall) == 3u);
}

TEST_CASE("bounds: every ascending array of length 0 to 6 over 4 values, 7 keys", "[sweep][sortedops]") {
  Fold f;
  std::size_t bad = 0;
  for_each_array(kRefBoundMax, [&](const Vec& a) {
    for (std::uint32_t key : kRefKeys7) {
      do_bound(&f, &bad, a, key);
      ++f.cases;
    }
  });
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_bound);
  CHECK(f.ops == kRefOps_bound);
  CHECK(f.h == kRefDigest_bound);
}

TEST_CASE("merge: every pair of ascending arrays of length 0 to 5, every dst_cap 0 to na+nb+1", "[sweep][sortedops]") {
  Fold f;
  std::size_t bad = 0;
  for_each_array(kRefMergeMax, [&](const Vec& a) {
    for_each_array(kRefMergeMax, [&](const Vec& b) {
      const std::uint32_t total = static_cast<std::uint32_t>(a.size() + b.size());
      for (std::uint32_t cap = 0; cap <= total + 1u; ++cap) {
        do_merge(&f, &bad, a, b, cap, 0u);
        do_merge(nullptr, &bad, a, b, cap, 3u);  // the same with canary padding after the output
        ++f.cases;
      }
    });
  });
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_merge);
  CHECK(f.ops == kRefOps_merge);
  CHECK(f.h == kRefDigest_merge);
}

TEST_CASE("unique: every ascending array of length 0 to 7 over 4 values", "[sweep][sortedops]") {
  Fold f;
  std::size_t bad = 0;
  for_each_array(kRefUniqMax, [&](const Vec& a) {
    do_unique(&f, &bad, a, 0u);
    do_unique(nullptr, &bad, a, 3u);  // the same with canary padding after the array
    ++f.cases;
  });
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_uniq);
  CHECK(f.ops == kRefOps_uniq);
  CHECK(f.h == kRefDigest_uniq);
}

TEST_CASE("pseudo-random ascending arrays up to 300 elements (not exhaustive)", "[sweep][sortedops]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(seed);
    const std::uint32_t n1 = g.next() % kRefRandLen;
    const std::uint32_t n2 = g.next() % kRefRandLen;
    const std::uint32_t s1 = g.next() % (1u << 31);
    const std::uint32_t s2 = g.next() % (1u << 31);
    const std::uint32_t gm1 = kRefGapMods[seed % 3u], gm2 = kRefGapMods[(seed / 3u) % 3u];
    Vec a, b;
    std::uint32_t x = s1;
    for (std::uint32_t i = 0; i < n1; ++i) {
      x += g.next() % gm1;
      a.push_back(x);
    }
    x = s2;
    for (std::uint32_t i = 0; i < n2; ++i) {
      x += g.next() % gm2;
      b.push_back(x);
    }
    for (std::uint32_t p = 0; p < kRefRandProbes; ++p) {
      const std::uint32_t r = g.next();
      const std::uint32_t base = n1 > 0u ? a[r % n1] : 0u;
      const std::uint32_t keys[4] = {base, base + 1u, base - 1u, r};
      do_bound(&f, &bad, a, keys[p % 4u]);
    }
    do_merge(&f, &bad, a, b, n1 + n2, 0u);
    if (n1 + n2 > 0u) do_merge(&f, &bad, a, b, n1 + n2 - 1u, 0u);
    do_merge(nullptr, &bad, a, b, n1 + n2 + 5u, 3u);
    do_unique(&f, &bad, a, 0u);
    do_unique(&f, &bad, b, 3u);
    ++f.cases;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

TEST_CASE("lower_bound and upper_bound: arguments and check order", "[sortedops]") {
  const std::uint32_t one[1] = {5u};
  std::uint32_t out = kSentinel;
  for (int which = 0; which < 2; ++which) {
    auto call = [&](const std::uint32_t* k, std::uint32_t n, std::uint32_t key, std::uint32_t* o) {
      return which == 0 ? lower_bound_u32(k, n, key, o) : upper_bound_u32(k, n, key, o);
    };
    out = kSentinel;
    CHECK(call(nullptr, 1u, 5u, &out) == OpResult::NullArgument);
    CHECK(call(one, 1u, 5u, nullptr) == OpResult::NullArgument);
    CHECK(call(nullptr, 0u, 5u, nullptr) == OpResult::NullArgument);  // out pointer always counts
    CHECK(call(one, 0u, 5u, nullptr) == OpResult::NullArgument);
    // null before TooLarge
    CHECK(call(nullptr, kMaxCount + 1u, 5u, &out) == OpResult::NullArgument);
    CHECK(call(nullptr, kMax32, 5u, &out) == OpResult::NullArgument);
    CHECK(call(one, kMaxCount + 1u, 5u, nullptr) == OpResult::NullArgument);
    // TooLarge: nothing is read (one-element array) and out is untouched
    CHECK(call(one, kMaxCount + 1u, 5u, &out) == OpResult::TooLarge);
    CHECK(call(one, kMax32, 5u, &out) == OpResult::TooLarge);
    CHECK(out == kSentinel);
    // n == 0: Ok, index 0, the array pointer may be null and is not read
    CHECK(call(nullptr, 0u, 5u, &out) == OpResult::Ok);
    CHECK(out == 0u);
    out = kSentinel;
    CHECK(call(one, 0u, 5u, &out) == OpResult::Ok);
    CHECK(out == 0u);
  }
  // equal keys: lower gives the first, upper the one after the last
  const std::uint32_t eq[5] = {1u, 3u, 3u, 3u, 7u};
  CHECK(lower_bound_u32(eq, 5u, 3u, &out) == OpResult::Ok);
  CHECK(out == 1u);
  CHECK(upper_bound_u32(eq, 5u, 3u, &out) == OpResult::Ok);
  CHECK(out == 4u);
  CHECK(lower_bound_u32(eq, 5u, 8u, &out) == OpResult::Ok);
  CHECK(out == 5u);
  CHECK(upper_bound_u32(eq, 5u, 0u, &out) == OpResult::Ok);
  CHECK(out == 0u);
  CHECK(lower_bound_u32(eq, 5u, kMax32, &out) == OpResult::Ok);
  CHECK(out == 5u);
}

TEST_CASE("merge: arguments, check order, sizes", "[sortedops]") {
  const std::uint32_t a1[1] = {5u}, b1[1] = {6u};
  std::uint32_t dst[4] = {kSentinel, kSentinel, kSentinel, kSentinel};
  std::uint32_t len = kSentinel;
  // null pointers (arrays count only while their length is above 0, dst while na + nb is above 0, out_len always)
  CHECK(merge_sorted_u32(nullptr, 1u, b1, 1u, dst, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, 1u, nullptr, 1u, dst, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, 1u, b1, 1u, nullptr, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, 1u, b1, 1u, dst, 4u, nullptr) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(nullptr, 0u, nullptr, 0u, dst, 4u, nullptr) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, 1u, nullptr, 0u, nullptr, 4u, &len) == OpResult::NullArgument);  // dst null with na + nb > 0
  CHECK(merge_sorted_u32(nullptr, 0u, b1, 1u, nullptr, 4u, &len) == OpResult::NullArgument);
  // null before TooLarge
  CHECK(merge_sorted_u32(nullptr, 1u, b1, kMaxCount + 1u, dst, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, kMaxCount + 1u, nullptr, 1u, dst, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, kMaxCount + 1u, b1, 1u, nullptr, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, kMaxCount + 1u, b1, 1u, dst, 4u, nullptr) == OpResult::NullArgument);
  // na + nb above 2^32 still counts as above zero for the dst null rule
  CHECK(merge_sorted_u32(a1, kMax32, b1, 1u, nullptr, 4u, &len) == OpResult::NullArgument);
  CHECK(merge_sorted_u32(a1, kMax32, b1, kMax32, nullptr, 0u, &len) == OpResult::NullArgument);
  // TooLarge (one-element arrays: nothing is read), also before BufferTooSmall
  CHECK(merge_sorted_u32(a1, kMaxCount + 1u, b1, 1u, dst, 0u, &len) == OpResult::TooLarge);
  CHECK(merge_sorted_u32(a1, 1u, b1, kMaxCount + 1u, dst, 0u, &len) == OpResult::TooLarge);
  CHECK(merge_sorted_u32(a1, kMax32, b1, kMax32, dst, 0u, &len) == OpResult::TooLarge);
  CHECK(merge_sorted_u32(a1, kMaxCount + 1u, nullptr, 0u, dst, 4u, &len) == OpResult::TooLarge);
  CHECK(merge_sorted_u32(nullptr, 0u, b1, kMaxCount + 1u, dst, 4u, &len) == OpResult::TooLarge);
  // BufferTooSmall at the largest sizes: na + nb = 2^27 does not fit in dst_cap = 2^27 - 1 (no array is read or written)
  CHECK(merge_sorted_u32(a1, kMaxCount, b1, kMaxCount, dst, 2u * kMaxCount - 1u, &len) == OpResult::BufferTooSmall);
  CHECK(merge_sorted_u32(a1, kMaxCount, b1, kMaxCount, dst, kMaxCount, &len) == OpResult::BufferTooSmall);
  CHECK(merge_sorted_u32(a1, kMaxCount, b1, kMaxCount, dst, 0u, &len) == OpResult::BufferTooSmall);
  CHECK(merge_sorted_u32(a1, kMaxCount, b1, 1u, dst, kMaxCount, &len) == OpResult::BufferTooSmall);
  CHECK(merge_sorted_u32(a1, kMaxCount, nullptr, 0u, dst, kMaxCount - 1u, &len) == OpResult::BufferTooSmall);
  CHECK(len == kSentinel);
  CHECK((dst[0] == kSentinel && dst[1] == kSentinel && dst[2] == kSentinel && dst[3] == kSentinel));
  // every length 0: Ok, no array touched, null arrays and a null dst are fine, any dst_cap
  CHECK(merge_sorted_u32(nullptr, 0u, nullptr, 0u, nullptr, 0u, &len) == OpResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(merge_sorted_u32(nullptr, 0u, nullptr, 0u, nullptr, 9u, &len) == OpResult::Ok);
  CHECK(len == 0u);
  // one empty side
  len = kSentinel;
  CHECK(merge_sorted_u32(a1, 1u, nullptr, 0u, dst, 1u, &len) == OpResult::Ok);
  CHECK((len == 1u && dst[0] == 5u));
  len = kSentinel;
  CHECK(merge_sorted_u32(nullptr, 0u, b1, 1u, dst, 1u, &len) == OpResult::Ok);
  CHECK((len == 1u && dst[0] == 6u));
  // duplicates are all kept
  const std::uint32_t da[3] = {2u, 2u, 9u}, db[2] = {2u, 9u};
  std::uint32_t out5[5] = {};
  CHECK(merge_sorted_u32(da, 3u, db, 2u, out5, 5u, &len) == OpResult::Ok);
  CHECK(len == 5u);
  const std::uint32_t want[5] = {2u, 2u, 2u, 9u, 9u};
  for (int i = 0; i < 5; ++i) CHECK(out5[i] == want[i]);
}

TEST_CASE("unique: arguments, check order, sizes", "[sortedops]") {
  std::uint32_t one[1] = {5u};
  std::uint32_t len = kSentinel;
  CHECK(unique_sorted_u32(nullptr, 1u, &len) == OpResult::NullArgument);
  CHECK(unique_sorted_u32(one, 1u, nullptr) == OpResult::NullArgument);
  CHECK(unique_sorted_u32(nullptr, 0u, nullptr) == OpResult::NullArgument);
  CHECK(unique_sorted_u32(one, 0u, nullptr) == OpResult::NullArgument);
  CHECK(unique_sorted_u32(nullptr, kMaxCount + 1u, &len) == OpResult::NullArgument);
  CHECK(unique_sorted_u32(one, kMaxCount + 1u, nullptr) == OpResult::NullArgument);
  CHECK(unique_sorted_u32(one, kMaxCount + 1u, &len) == OpResult::TooLarge);
  CHECK(unique_sorted_u32(one, kMax32, &len) == OpResult::TooLarge);
  CHECK(len == kSentinel);
  CHECK(one[0] == 5u);
  CHECK(unique_sorted_u32(nullptr, 0u, &len) == OpResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(unique_sorted_u32(one, 0u, &len) == OpResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(unique_sorted_u32(one, 1u, &len) == OpResult::Ok);
  CHECK((len == 1u && one[0] == 5u));
  std::uint32_t runs[8] = {0u, 0u, 0u, 4u, 4u, 9u, kMax32, kMax32};
  CHECK(unique_sorted_u32(runs, 8u, &len) == OpResult::Ok);
  CHECK(len == 4u);
  CHECK((runs[0] == 0u && runs[1] == 4u && runs[2] == 9u && runs[3] == kMax32));
  std::uint32_t distinct[4] = {1u, 2u, 3u, 4u};
  CHECK(unique_sorted_u32(distinct, 4u, &len) == OpResult::Ok);
  CHECK(len == 4u);
  for (std::uint32_t i = 0; i < 4u; ++i) CHECK(distinct[i] == i + 1u);
}
