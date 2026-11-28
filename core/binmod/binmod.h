// Binomial coefficients of u64 arguments modulo a small prime p (2 to 65521, p prime), by Lucas' theorem, and the exponent of p in the coefficient by Kummer's theorem.
// C(n, k) mod p is the product of C(n_i, k_i) mod p over the base-p digits n_i and k_i of n and k (0 as soon as some k_i is above n_i); the exponent of p in C(n, k)
// is the number of carries when k and n - k are added in base p. The coefficient itself is never formed (it is far beyond u64), and C(n, k) = 0 for k above n.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global
// state, no arrays, no tables. Cost: at most 64 base-p digits (p = 2), each digit costing O(p) steps at most; a loop over all n or k is not acceptable.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::binmod {
constexpr std::uint32_t kMaxPrime = 65521u;  // the largest prime below 2^16
enum class BmResult : std::uint8_t { Ok = 0, NullArgument = 1, BadPrime = 2, ZeroBinomial = 3 };
// Check order for both functions: a null output pointer gives NullArgument; then p that is below 2, above 65521 or not a prime gives BadPrime; then
// (binom_pvaluation only) k above n gives ZeroBinomial (the coefficient is 0, so its p-adic exponent is not defined). Nothing is written to *out on a failure.
//
// binom_mod: *out = C(n, k) mod p, with *out = 0 and Ok for k above n. Examples: (10, 3, 7) = 1; (100, 50, 7) = 4; (1000, 500, 13) = 0; (2^20, 12345, 65521) = 0;
// (2^40 + 12345, 2^20 + 7, 3) = 0; (5, 7, 11) = 0 (k above n); (0, 0, 2) = 1; (n, 0, p) = 1 and (n, n, p) = 1 for every n; (2^64 - 1, 2^63, 2) = 1;
// (2^64 - 1, 12345, 5) = 0; (2^64 - 1, 2^32, 65521) = 30337; (6, 2, 4), (6, 2, 1), (6, 2, 0) and (6, 2, 65537) are BadPrime (65537 is prime but above the limit).
BmResult binom_mod(std::uint64_t n, std::uint64_t k, std::uint32_t p, std::uint32_t* out);
// binom_pvaluation: *out = the exponent of p in C(n, k) (the number of carries adding k and n - k in base p). Examples: (10, 3, 7) = 0; (100, 50, 7) = 0;
// (1000, 500, 13) = 1; (2^20, 12345, 65521) = 1; (2^40 + 12345, 2^20 + 7, 3) = 6; (2^64 - 1, 12345, 5) = 4; (2^64 - 1, 2^32, 65521) = 0; (0, 0, 2) = 0; (5, 7, 11)
// is ZeroBinomial.
BmResult binom_pvaluation(std::uint64_t n, std::uint64_t k, std::uint32_t p, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::binmod
