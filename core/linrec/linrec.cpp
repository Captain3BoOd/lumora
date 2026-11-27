// Linear recurrences modulo 998244353 (contract in linrec.h). linrec_nth: x^n modulo the characteristic polynomial by binary exponentiation, then the
// dot product with the initial terms. linrec_bm: the Berlekamp-Massey method with the three polynomials C, B and T in scratch. Integer only, u64
// products, no allocation, no recursion. The proofs of the bounds are in docs/linrec.md.
#include "linrec.h"

namespace lumora::linrec {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

u32 mul_mod(u32 x, u32 y) { return static_cast<u32>((static_cast<u64>(x) * static_cast<u64>(y)) % kMod); }
u32 add_mod(u32 x, u32 y) {
  const u32 s = x + y;  // both below kMod < 2^30, so no wrap
  return s >= kMod ? s - kMod : s;
}
u32 sub_mod(u32 x, u32 y) { return x >= y ? x - y : x + kMod - y; }
u32 pow_mod(u32 base, u32 e) {
  u32 r = 1u;
  u32 b = base;
  while (e != 0u) {
    if ((e & 1u) != 0u) r = mul_mod(r, b);
    b = mul_mod(b, b);
    e >>= 1u;
  }
  return r;
}

// out = x * y modulo the characteristic polynomial x^d - c[0] x^(d-1) - ... - c[d-1]. x and y have d coefficients, tmp has 2 * d words, out may be x.
void poly_mul_mod(const u32* c, u32 d, const u32* x, const u32* y, u32* tmp, u32* out) {
  for (u32 i = 0u; i < 2u * d; ++i) tmp[i] = 0u;
  for (u32 i = 0u; i < d; ++i) {
    if (x[i] == 0u) continue;
    for (u32 j = 0u; j < d; ++j) tmp[i + j] = add_mod(tmp[i + j], mul_mod(x[i], y[j]));
  }
  for (u32 i = 2u * d - 2u; i >= d; --i) {
    const u32 t = tmp[i];
    if (t != 0u) {
      for (u32 j = 1u; j <= d; ++j) tmp[i - j] = add_mod(tmp[i - j], mul_mod(t, c[j - 1u]));
    }
    tmp[i] = 0u;
    if (i == d) break;
  }
  for (u32 i = 0u; i < d; ++i) out[i] = tmp[i];
}
}  // namespace

LinResult linrec_nth_scratch_words(u32 d, u32* out_words) {
  if (out_words == nullptr) return LinResult::NullArgument;
  if (d > kMaxOrder) return LinResult::TooLarge;
  *out_words = 4u * d;
  return LinResult::Ok;
}

LinResult linrec_bm_scratch_words(u32 len, u32* out_words) {
  if (out_words == nullptr) return LinResult::NullArgument;
  if (len > kMaxLen) return LinResult::TooLarge;
  *out_words = (len == 0u) ? 0u : 3u * len + 3u;
  return LinResult::Ok;
}

LinResult linrec_nth(const u32* coef, const u32* init, u32 d, u64 n, u32* scratch, u32 scratch_words, u32* out) {
  if (out == nullptr) return LinResult::NullArgument;
  if (d > 0u && (coef == nullptr || init == nullptr || scratch == nullptr)) return LinResult::NullArgument;
  if (d > kMaxOrder) return LinResult::TooLarge;
  if (d > 0u && scratch_words < 4u * d) return LinResult::ScratchTooSmall;
  for (u32 i = 0u; i < d; ++i) {
    if (coef[i] >= kMod || init[i] >= kMod) return LinResult::ValueOutOfRange;
  }
  if (d == 0u) {
    *out = 0u;
    return LinResult::Ok;
  }
  if (n < static_cast<u64>(d)) {
    *out = init[static_cast<u32>(n)];
    return LinResult::Ok;
  }
  u32* res = scratch;
  u32* base = scratch + d;
  u32* tmp = scratch + 2u * d;
  for (u32 i = 0u; i < d; ++i) {
    res[i] = 0u;
    base[i] = 0u;
  }
  res[0] = 1u;
  if (d == 1u) {
    base[0] = coef[0];  // x modulo (x - c[0])
  } else {
    base[1] = 1u;
  }
  u64 e = n;
  while (e != 0u) {
    if ((e & 1u) != 0u) poly_mul_mod(coef, d, res, base, tmp, res);
    e >>= 1u;
    if (e != 0u) poly_mul_mod(coef, d, base, base, tmp, base);
  }
  u32 acc = 0u;
  for (u32 i = 0u; i < d; ++i) acc = add_mod(acc, mul_mod(res[i], init[i]));
  *out = acc;
  return LinResult::Ok;
}

LinResult linrec_bm(const u32* seq, u32 len, u32* scratch, u32 scratch_words, u32* out_coef, u32 out_cap, u32* out_d) {
  if (out_d == nullptr) return LinResult::NullArgument;
  if (len > 0u && (seq == nullptr || scratch == nullptr || out_coef == nullptr)) return LinResult::NullArgument;
  if (len > kMaxLen) return LinResult::TooLarge;
  if (len > 0u && scratch_words < 3u * len + 3u) return LinResult::ScratchTooSmall;
  for (u32 i = 0u; i < len; ++i) {
    if (seq[i] >= kMod) return LinResult::ValueOutOfRange;
  }
  if (len == 0u) {
    *out_d = 0u;
    return LinResult::Ok;
  }
  const u32 w = len + 1u;
  u32* C = scratch;
  u32* B = scratch + w;
  u32* T = scratch + 2u * w;
  for (u32 i = 0u; i < w; ++i) {
    C[i] = 0u;
    B[i] = 0u;
    T[i] = 0u;
  }
  C[0] = 1u;
  B[0] = 1u;
  u32 L = 0u;
  u32 m = 1u;
  u32 b = 1u;
  for (u32 i = 0u; i < len; ++i) {
    u32 disc = seq[i];
    for (u32 j = 1u; j <= L; ++j) disc = add_mod(disc, mul_mod(C[j], seq[i - j]));
    if (disc == 0u) {
      ++m;
      continue;
    }
    const u32 coefk = mul_mod(disc, pow_mod(b, kMod - 2u));
    if (2u * L <= i) {
      for (u32 j = 0u; j < w; ++j) T[j] = C[j];
      for (u32 j = 0u; j + m < w; ++j) C[j + m] = sub_mod(C[j + m], mul_mod(coefk, B[j]));
      L = i + 1u - L;
      for (u32 j = 0u; j < w; ++j) B[j] = T[j];
      b = disc;
      m = 1u;
    } else {
      for (u32 j = 0u; j + m < w; ++j) C[j + m] = sub_mod(C[j + m], mul_mod(coefk, B[j]));
      ++m;
    }
  }
  if (out_cap < L) return LinResult::OutTooSmall;
  for (u32 j = 1u; j <= L; ++j) out_coef[j - 1u] = (C[j] == 0u) ? 0u : kMod - C[j];
  *out_d = L;
  return LinResult::Ok;
}

}  // namespace lumora::linrec
