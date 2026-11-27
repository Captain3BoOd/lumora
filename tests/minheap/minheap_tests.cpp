// Tests for core/minheap. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/minheap_ref.py, an independent Python implementation (a sorted list of
// tuples, no heap). Regenerate it with: python3 tools/minheap_ref.py
// The tests also carry a naive oracle that shares no code with core/minheap/minheap.cpp: an unsorted vector scanned for its minimum.
// Arrays are heap buffers of exactly capacity elements (AddressSanitizer sees any access past the end) or, in the write-rule runs, padded
// with canaries. The layout inside the heap is not specified and is not tested: only popped pairs, peeked pairs, results, *size and the write
// rules (push writes only below the new size, pop only below the old size, peek and every failure write nothing).
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "minheap.h"

using namespace lumora::minheap;

namespace {

// BEGIN GENERATED REFERENCE (tools/minheap_ref.py)
const std::uint32_t kRefP4[][2] = {{0x0u, 0x0u}, {0x0u, 0xFFFFFFFFu}, {0x1u, 0x0u}, {0xFFFFFFFFu, 0x0u}};
const std::uint32_t kRefQ7[][2] = {{0x5u, 0x1u}, {0x5u, 0x0u}, {0x0u, 0x9u}, {0xFFFFFFFFu, 0xFFFFFFFFu}, {0x0u, 0x0u}, {0x80000000u, 0x3u}, {0x5u, 0x2u}};
const std::uint32_t kRefT3[][2] = {{0x2u, 0x2u}, {0x2u, 0x1u}, {0x1u, 0x5u}};
const std::uint32_t kRefSeqMax = 6, kRefSeqCaps = 5, kRefMultiMax = 7;
const std::uint32_t kRefRandSeeds = 200, kRefRandOps = 2000, kRefRandCap = 64;
const std::uint32_t kRefDeepSeeds = 20, kRefDeepOps = 6000, kRefDeepCap = 1000, kRefDeepPhase = 500;
const std::uint64_t kRefDigest_seq = 0x3DE6CA300198A703ull;
const std::size_t kRefCases_seq = 279935, kRefOps_seq = 1623630;
const std::uint64_t kRefDigest_perm = 0xDFF7A696C80BE8FEull;
const std::size_t kRefCases_perm = 5914, kRefOps_perm = 86552;
const std::uint64_t kRefDigest_multi = 0x2981B6A89BFA01CDull;
const std::size_t kRefCases_multi = 3280, kRefOps_multi = 45928;
const std::uint64_t kRefDigest_rand = 0x41883FD27AEB16FFull;
const std::size_t kRefCases_rand = 200, kRefOps_rand = 400000;
const std::uint64_t kRefDigest_deep = 0x392E57A54C3C2B24ull;
const std::size_t kRefCases_deep = 20, kRefOps_deep = 120000;
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kMax32 = 0xFFFFFFFFu;

std::uint32_t code(HeapResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

enum Op : std::uint32_t { kPush = 0, kPop = 1, kPeek = 2 };

// Naive oracle: unsorted pairs, the minimum found by a scan with a hand-written lexicographic comparison.
struct Oracle {
  std::uint32_t cap;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> items;
  explicit Oracle(std::uint32_t c) : cap(c) {}
  static bool smaller(const std::pair<std::uint32_t, std::uint32_t>& a, const std::pair<std::uint32_t, std::uint32_t>& b) {
    if (a.first != b.first) return a.first < b.first;
    return a.second < b.second;
  }
  std::size_t argmin() const {
    std::size_t best = 0;
    for (std::size_t i = 1; i < items.size(); ++i) {
      if (smaller(items[i], items[best])) best = i;
    }
    return best;
  }
};

struct Result3 {
  std::uint32_t code;
  std::uint32_t k;
  std::uint32_t v;
};

// A heap in caller arrays with `pad` canary elements after capacity. Every element starts as a canary.
struct Harness {
  std::uint32_t cap;
  std::uint32_t pad;
  std::unique_ptr<std::uint32_t[]> keys;
  std::unique_ptr<std::uint32_t[]> values;
  std::uint32_t size = 0;
  Oracle oracle;
  std::size_t bad = 0;  // mismatches with the oracle or write-rule violations

  Harness(std::uint32_t c, std::uint32_t p) : cap(c), pad(p), keys(new std::uint32_t[c + p]), values(new std::uint32_t[c + p]), oracle(c) {
    for (std::uint32_t i = 0; i < c + p; ++i) {
      keys[i] = 0xC0DE0000u + i;
      values[i] = 0xBEEF0000u + i;
    }
  }

  // Runs one operation on the heap and the oracle and checks them against each other and against the write rules.
  Result3 step(Op op, std::uint32_t k, std::uint32_t v, bool check_writes) {
    const std::uint32_t old_size = size;
    std::vector<std::uint32_t> sk, sv;
    if (check_writes) {
      sk.assign(keys.get(), keys.get() + cap + pad);
      sv.assign(values.get(), values.get() + cap + pad);
    }
    std::uint32_t ok_ = kSentinel, ov_ = kSentinel;
    HeapResult r{};
    std::uint32_t want_code = 0, want_k = 0, want_v = 0;
    if (op == kPush) {
      r = heap_push(keys.get(), values.get(), &size, cap, k, v);
      if (oracle.items.size() == oracle.cap) {
        want_code = code(HeapResult::Full);
      } else {
        oracle.items.emplace_back(k, v);
      }
    } else {
      r = op == kPop ? heap_pop_min(keys.get(), values.get(), &size, cap, &ok_, &ov_)
                     : heap_peek_min(keys.get(), values.get(), &size, cap, &ok_, &ov_);
      if (oracle.items.empty()) {
        want_code = code(HeapResult::Empty);
      } else {
        const std::size_t m = oracle.argmin();
        want_k = oracle.items[m].first;
        want_v = oracle.items[m].second;
        if (op == kPop) oracle.items.erase(oracle.items.begin() + static_cast<std::ptrdiff_t>(m));
      }
    }
    Result3 out{code(r), 0u, 0u};
    if (op != kPush && r == HeapResult::Ok) {
      out.k = ok_;
      out.v = ov_;
    }
    if (out.code != want_code || out.k != want_k || out.v != want_v) ++bad;
    if (op != kPush && r != HeapResult::Ok && (ok_ != kSentinel || ov_ != kSentinel)) ++bad;
    if (size != oracle.items.size()) ++bad;
    if (check_writes) {
      std::uint32_t lo = 0;  // arrays from this index on must be unchanged
      if (r == HeapResult::Ok && op == kPush) lo = old_size + 1u;
      else if (r == HeapResult::Ok && op == kPop) lo = old_size;
      else lo = 0;
      for (std::uint32_t i = lo; i < cap + pad; ++i) {
        if (keys[i] != sk[i] || values[i] != sv[i]) {
          ++bad;
          break;
        }
      }
    }
    return out;
  }
};

void fold_result(Fold& f, const Result3& r) {
  ++f.ops;
  f.add(r.code);
  f.add((static_cast<std::uint64_t>(r.k) << 32) | r.v);
}

void end_case(Fold& f, const Harness& h) {
  f.add(h.size);
  ++f.cases;
}

// The same 64-bit LCG as the reference; each draw is the high 32 bits.
struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

// Applies the sequence ops (0..3 push P4[o], 4 pop, 5 peek) with capacity cap. Returns the number of mismatches.
void run_seq(Fold* f, std::size_t* bad, std::uint32_t cap, std::uint32_t pad, const std::vector<std::uint32_t>& ops) {
  Harness h(cap, pad);
  for (std::uint32_t o : ops) {
    Result3 r;
    if (o < 4u) {
      r = h.step(kPush, kRefP4[o][0], kRefP4[o][1], true);
    } else {
      r = h.step(o == 4u ? kPop : kPeek, 0, 0, true);
    }
    if (f != nullptr) fold_result(*f, r);
  }
  if (f != nullptr) end_case(*f, h);
  *bad += h.bad;
}

// Calls fn(ops) for every sequence of length 0..max over `base` symbols, in the order of itertools.product by length.
template <typename Fn>
void for_each_sequence(std::uint32_t base, std::uint32_t max_len, Fn fn) {
  for (std::uint32_t n = 0; n <= max_len; ++n) {
    std::vector<std::uint32_t> ops(n, 0u);
    for (;;) {
      fn(ops);
      std::uint32_t i = n;
      while (i > 0u) {
        --i;
        if (++ops[i] < base) goto advanced;
        ops[i] = 0u;
      }
      break;
    advanced:;
    }
  }
}

template <typename Fn>
void for_each_permutation(std::uint32_t n, Fn fn) {
  std::vector<std::uint32_t> p(n);

  for (std::uint32_t i = 0; i < n; ++i) p[i] = i;
  for (;;) {
    fn(p);
    std::uint32_t i = n;
    // find the largest i with p[i-1] < p[i]
    while (i > 1u && p[i - 2u] > p[i - 1u]) --i;
    if (i <= 1u) break;
    std::uint32_t j = n;
    while (p[j - 1u] < p[i - 2u]) --j;
    std::swap(p[i - 2u], p[j - 1u]);
    for (std::uint32_t a = i - 1u, b = n - 1u; a < b; ++a, --b) std::swap(p[a], p[b]);
  }
}

}  // namespace

TEST_CASE("header anchors", "[minheap]") {
  CHECK(kMaxCapacity == (1u << 26));
  CHECK(code(HeapResult::Ok) == 0u);
  CHECK(code(HeapResult::NullArgument) == 1u);
  CHECK(code(HeapResult::TooLarge) == 2u);
  CHECK(code(HeapResult::Full) == 3u);
  CHECK(code(HeapResult::Empty) == 4u);
  CHECK(code(HeapResult::BadSize) == 5u);
}

TEST_CASE("every operation sequence up to length 6 with capacity 0 to 4", "[sweep][minheap]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t cap = 0; cap < kRefSeqCaps; ++cap) {
    for_each_sequence(6u, kRefSeqMax, [&](const std::vector<std::uint32_t>& ops) { run_seq(&f, &bad, cap, 0u, ops); });
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_seq);
  CHECK(f.ops == kRefOps_seq);
  CHECK(f.h == kRefDigest_seq);
}

TEST_CASE("the same sequences with canary padding: write rules", "[sweep][minheap]") {
  std::size_t bad = 0;
  std::size_t cases = 0;
  for (std::uint32_t cap = 0; cap < kRefSeqCaps; ++cap) {
    for_each_sequence(6u, kRefSeqMax, [&](const std::vector<std::uint32_t>& ops) {
      run_seq(nullptr, &bad, cap, 3u, ops);
      ++cases;
    });
  }
  CHECK(bad == 0u);
  CHECK(cases == kRefCases_seq);
}

TEST_CASE("every permutation of up to 7 distinct pairs: push all, pop all", "[sweep][minheap]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n = 0; n <= 7u; ++n) {
    for_each_permutation(n, [&](const std::vector<std::uint32_t>& p) {
      Harness h(n, 0u);
      for (std::uint32_t i : p) fold_result(f, h.step(kPush, kRefQ7[i][0], kRefQ7[i][1], true));
      for (std::uint32_t i = 0; i <= n; ++i) fold_result(f, h.step(kPop, 0, 0, true));
      end_case(f, h);
      bad += h.bad;
    });
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_perm);
  CHECK(f.ops == kRefOps_perm);
  CHECK(f.h == kRefDigest_perm);
}

TEST_CASE("every push sequence of length 0 to 7 over 3 pairs with duplicates", "[sweep][minheap]") {
  Fold f;
  std::size_t bad = 0;
  for_each_sequence(3u, kRefMultiMax, [&](const std::vector<std::uint32_t>& p) {
    const std::uint32_t n = static_cast<std::uint32_t>(p.size());
    Harness h(n, 0u);
    for (std::uint32_t i : p) fold_result(f, h.step(kPush, kRefT3[i][0], kRefT3[i][1], true));
    for (std::uint32_t i = 0; i <= n; ++i) fold_result(f, h.step(kPop, 0, 0, true));
    end_case(f, h);
    bad += h.bad;
  });
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_multi);
  CHECK(f.ops == kRefOps_multi);
  CHECK(f.h == kRefDigest_multi);
}

TEST_CASE("pseudo-random sequences, capacity 64 (not exhaustive)", "[sweep][minheap]") {
  Fold f;
  std::size_t bad = 0;
  const std::uint64_t kmods[3] = {4u, 16u, 0u};  // 0 means the full 32-bit range
  const std::uint64_t vmods[3] = {3u, 5u, 0u};
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(seed);
    const std::uint32_t pb = 2u + seed % 4u;
    const std::uint64_t km = kmods[seed % 3u];
    const std::uint64_t vm = vmods[(seed / 3u) % 3u];
    Harness h(kRefRandCap, seed % 2u == 0u ? 2u : 0u);
    for (std::uint32_t j = 0; j < kRefRandOps; ++j) {
      const std::uint32_t sel = g.next() % 8u;
      const std::uint32_t d2 = g.next();
      const std::uint32_t d3 = g.next();
      const std::uint32_t k = km == 0u ? d2 : static_cast<std::uint32_t>(d2 % km);
      const std::uint32_t v = vm == 0u ? d3 : static_cast<std::uint32_t>(d3 % vm);
      if (sel < pb) {
        fold_result(f, h.step(kPush, k, v, true));
      } else {
        fold_result(f, h.step((sel - pb) % 2u == 0u ? kPop : kPeek, 0, 0, true));
      }
    }
    end_case(f, h);
    bad += h.bad;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

TEST_CASE("deep heaps: capacity 1000, push-heavy and pop-heavy phases (not exhaustive)", "[sweep][minheap]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefDeepSeeds; ++seed) {
    Lcg g(seed);
    const std::uint64_t km = seed % 2u == 0u ? 50u : 0u;
    const std::uint64_t vm = (seed / 2u) % 2u == 0u ? 7u : 0u;
    Harness h(kRefDeepCap, 0u);
    for (std::uint32_t j = 0; j < kRefDeepOps; ++j) {
      const std::uint32_t sel = g.next() % 8u;
      const std::uint32_t d2 = g.next();
      const std::uint32_t d3 = g.next();
      const std::uint32_t k = km == 0u ? d2 : static_cast<std::uint32_t>(d2 % km);
      const std::uint32_t v = vm == 0u ? d3 : static_cast<std::uint32_t>(d3 % vm);
      const bool push_phase = (j / kRefDeepPhase) % 2u == 0u;
      if ((push_phase && sel < 7u) || (!push_phase && sel < 1u)) {
        fold_result(f, h.step(kPush, k, v, false));
      } else {
        fold_result(f, h.step(kPop, 0, 0, false));
      }
    }
    end_case(f, h);
    bad += h.bad;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_deep);
  CHECK(f.ops == kRefOps_deep);
  CHECK(f.h == kRefDigest_deep);
}

TEST_CASE("lexicographic order: key first, then value", "[minheap]") {
  const std::uint32_t pk[] = {1u, 0u, 0u, kMax32, kMax32, 0u, 1u};
  const std::uint32_t pv[] = {0u, kMax32, 0u, 0u, kMax32, 1u, kMax32};
  // sorted: (0,0) (0,1) (0,max) (1,0) (1,max) (max,0) (max,max)
  const std::uint32_t wk[] = {0u, 0u, 0u, 1u, 1u, kMax32, kMax32};
  const std::uint32_t wv[] = {0u, 1u, kMax32, 0u, kMax32, 0u, kMax32};
  std::uint32_t keys[7], values[7], size = 0;
  for (int i = 0; i < 7; ++i) CHECK(heap_push(keys, values, &size, 7u, pk[i], pv[i]) == HeapResult::Ok);
  CHECK(size == 7u);
  for (int i = 0; i < 7; ++i) {
    std::uint32_t k = kSentinel, v = kSentinel;
    CHECK(heap_peek_min(keys, values, &size, 7u, &k, &v) == HeapResult::Ok);
    CHECK((k == wk[i] && v == wv[i]));
    k = kSentinel;
    v = kSentinel;
    CHECK(heap_pop_min(keys, values, &size, 7u, &k, &v) == HeapResult::Ok);
    CHECK((k == wk[i] && v == wv[i]));
  }
  CHECK(size == 0u);
}

TEST_CASE("push: argument checks, order and untouched outputs", "[minheap]") {
  std::uint32_t keys[2] = {11u, 12u}, values[2] = {21u, 22u}, size = 1;
  // null pointers
  CHECK(heap_push(nullptr, values, &size, 2u, 1u, 1u) == HeapResult::NullArgument);
  CHECK(heap_push(keys, nullptr, &size, 2u, 1u, 1u) == HeapResult::NullArgument);
  CHECK(heap_push(keys, values, nullptr, 2u, 1u, 1u) == HeapResult::NullArgument);
  // null comes before TooLarge and BadSize
  CHECK(heap_push(nullptr, values, &size, kMaxCapacity + 1u, 1u, 1u) == HeapResult::NullArgument);
  CHECK(heap_push(keys, values, nullptr, kMax32, 1u, 1u) == HeapResult::NullArgument);
  CHECK(heap_push(nullptr, values, &size, 0u, 1u, 1u) == HeapResult::NullArgument);  // size 1 > capacity 0 too
  // TooLarge comes before BadSize and Full
  std::uint32_t big = kMax32;
  CHECK(heap_push(keys, values, &big, kMaxCapacity + 1u, 1u, 1u) == HeapResult::TooLarge);
  CHECK(heap_push(keys, values, &size, kMaxCapacity + 1u, 1u, 1u) == HeapResult::TooLarge);
  CHECK(heap_push(keys, values, &size, kMax32, 1u, 1u) == HeapResult::TooLarge);
  // BadSize comes before Full
  size = 3u;
  CHECK(heap_push(keys, values, &size, 2u, 1u, 1u) == HeapResult::BadSize);
  size = kMax32;
  CHECK(heap_push(keys, values, &size, 2u, 1u, 1u) == HeapResult::BadSize);
  size = 1u;
  CHECK(heap_push(keys, values, &size, 0u, 1u, 1u) == HeapResult::BadSize);
  // Full
  size = 2u;
  CHECK(heap_push(keys, values, &size, 2u, 1u, 1u) == HeapResult::Full);
  size = 0u;
  CHECK(heap_push(keys, values, &size, 0u, 1u, 1u) == HeapResult::Full);  // capacity 0
  // nothing was written by any failure
  CHECK((keys[0] == 11u && keys[1] == 12u && values[0] == 21u && values[1] == 22u));
  CHECK(size == 0u);
  // Ok
  CHECK(heap_push(keys, values, &size, 2u, 7u, 9u) == HeapResult::Ok);
  CHECK(size == 1u);
  CHECK((keys[0] == 7u && values[0] == 9u));
  CHECK((keys[1] == 12u && values[1] == 22u));  // the slot at the new size is untouched
}

TEST_CASE("pop and peek: argument checks, order and untouched outputs", "[minheap]") {
  std::uint32_t keys[2] = {11u, 12u}, values[2] = {21u, 22u}, size = 2u;
  std::uint32_t ok = kSentinel, ov = kSentinel;
  for (int which = 0; which < 2; ++which) {
    const bool pop = which == 0;
    auto call = [&](std::uint32_t* k, std::uint32_t* v, std::uint32_t* s, std::uint32_t cap, std::uint32_t* a, std::uint32_t* b) {
      return pop ? heap_pop_min(k, v, s, cap, a, b) : heap_peek_min(k, v, s, cap, a, b);
    };
    ok = kSentinel;
    ov = kSentinel;
    size = 2u;
    CHECK(call(nullptr, values, &size, 2u, &ok, &ov) == HeapResult::NullArgument);
    CHECK(call(keys, nullptr, &size, 2u, &ok, &ov) == HeapResult::NullArgument);
    CHECK(call(keys, values, nullptr, 2u, &ok, &ov) == HeapResult::NullArgument);
    CHECK(call(keys, values, &size, 2u, nullptr, &ov) == HeapResult::NullArgument);
    CHECK(call(keys, values, &size, 2u, &ok, nullptr) == HeapResult::NullArgument);
    // null before TooLarge and BadSize, and before Empty
    CHECK(call(keys, values, &size, kMaxCapacity + 1u, nullptr, &ov) == HeapResult::NullArgument);
    CHECK(call(keys, values, &size, 1u, &ok, nullptr) == HeapResult::NullArgument);  // size 2 > capacity 1 too
    std::uint32_t zero = 0u;
    CHECK(call(keys, values, &zero, 2u, nullptr, nullptr) == HeapResult::NullArgument);  // empty heap, still Null
    CHECK(call(keys, values, nullptr, kMax32, &ok, &ov) == HeapResult::NullArgument);
    // TooLarge before BadSize
    std::uint32_t big = kMax32;
    CHECK(call(keys, values, &big, kMaxCapacity + 1u, &ok, &ov) == HeapResult::TooLarge);
    CHECK(call(keys, values, &size, kMax32, &ok, &ov) == HeapResult::TooLarge);
    // BadSize before Empty is not distinguishable (size 0 is never above capacity); BadSize alone:
    CHECK(call(keys, values, &size, 1u, &ok, &ov) == HeapResult::BadSize);
    CHECK(call(keys, values, &big, 2u, &ok, &ov) == HeapResult::BadSize);
    size = 1u;
    CHECK(call(keys, values, &size, 0u, &ok, &ov) == HeapResult::BadSize);  // capacity 0, size 1
    // Empty
    size = 0u;
    CHECK(call(keys, values, &size, 2u, &ok, &ov) == HeapResult::Empty);
    CHECK(call(keys, values, &size, 0u, &ok, &ov) == HeapResult::Empty);  // capacity 0
    // nothing written
    CHECK((ok == kSentinel && ov == kSentinel));
    CHECK(size == 0u);
    CHECK((keys[0] == 11u && keys[1] == 12u && values[0] == 21u && values[1] == 22u));
  }
}

TEST_CASE("peek leaves keys, values and size unchanged; pop writes only below the old size", "[minheap]") {
  std::uint32_t keys[6] = {}, values[6] = {}, size = 0;
  for (std::uint32_t i = 0; i < 6u; ++i) {
    keys[i] = 0xAAAA0000u + i;
    values[i] = 0xBBBB0000u + i;
  }
  for (std::uint32_t i = 0; i < 4u; ++i) CHECK(heap_push(keys, values, &size, 6u, 9u - i, i) == HeapResult::Ok);
  CHECK(size == 4u);
  CHECK((keys[4] == 0xAAAA0004u && values[4] == 0xBBBB0004u && keys[5] == 0xAAAA0005u && values[5] == 0xBBBB0005u));
  std::uint32_t sk[6], sv[6];
  for (int i = 0; i < 6; ++i) {
    sk[i] = keys[i];
    sv[i] = values[i];
  }
  std::uint32_t k = 0, v = 0;
  CHECK(heap_peek_min(keys, values, &size, 6u, &k, &v) == HeapResult::Ok);
  CHECK((k == 6u && v == 3u));
  CHECK(size == 4u);
  for (int i = 0; i < 6; ++i) CHECK((keys[i] == sk[i] && values[i] == sv[i]));
  CHECK(heap_pop_min(keys, values, &size, 6u, &k, &v) == HeapResult::Ok);
  CHECK((k == 6u && v == 3u));
  CHECK(size == 3u);
  CHECK((keys[4] == 0xAAAA0004u && values[4] == 0xBBBB0004u && keys[5] == 0xAAAA0005u && values[5] == 0xBBBB0005u));
}

TEST_CASE("capacity bounds: kMaxCapacity is accepted, one more is TooLarge (1-element buffers)", "[minheap]") {
  std::uint32_t keys[1] = {kSentinel}, values[1] = {kSentinel}, size = 0, k = 0, v = 0;
  // Only elements below *size (and the slot written by a push) are touched, so tiny buffers are enough.
  CHECK(heap_push(keys, values, &size, kMaxCapacity, 5u, 6u) == HeapResult::Ok);
  CHECK(size == 1u);
  CHECK(heap_peek_min(keys, values, &size, kMaxCapacity, &k, &v) == HeapResult::Ok);
  CHECK((k == 5u && v == 6u));
  CHECK(heap_pop_min(keys, values, &size, kMaxCapacity, &k, &v) == HeapResult::Ok);
  CHECK((k == 5u && v == 6u));
  CHECK(size == 0u);
  CHECK(heap_pop_min(keys, values, &size, kMaxCapacity, &k, &v) == HeapResult::Empty);
  CHECK(heap_push(keys, values, &size, kMaxCapacity + 1u, 5u, 6u) == HeapResult::TooLarge);
  CHECK(heap_pop_min(keys, values, &size, kMaxCapacity + 1u, &k, &v) == HeapResult::TooLarge);
  CHECK(heap_peek_min(keys, values, &size, kMaxCapacity + 1u, &k, &v) == HeapResult::TooLarge);
  CHECK(size == 0u);
  // Full at *size == kMaxCapacity: nothing is read or written (the arrays have one element).
  size = kMaxCapacity;
  CHECK(heap_push(keys, values, &size, kMaxCapacity, 5u, 6u) == HeapResult::Full);
  CHECK(size == kMaxCapacity);
  size = kMaxCapacity + 1u;
  CHECK(heap_push(keys, values, &size, kMaxCapacity, 5u, 6u) == HeapResult::BadSize);
  CHECK(heap_pop_min(keys, values, &size, kMaxCapacity, &k, &v) == HeapResult::BadSize);
  CHECK(heap_peek_min(keys, values, &size, kMaxCapacity, &k, &v) == HeapResult::BadSize);
}
