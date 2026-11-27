// Linear recurrences modulo the prime 998244353: the n-th term by polynomial exponentiation, and the shortest recurrence of a sequence by the
// Berlekamp-Massey method.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::linrec {
constexpr std::uint32_t kMod = 998244353u;   // prime
constexpr std::uint32_t kMaxOrder = 256u;    // largest recurrence order for linrec_nth
constexpr std::uint32_t kMaxLen = 1024u;     // largest sequence length for linrec_bm
enum class LinResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutTooSmall = 4, ValueOutOfRange = 5 };
// All numbers are residues in 0 .. kMod - 1; an entry that is not below kMod gives ValueOutOfRange. Arithmetic is exact in the field. A recurrence of
// order d with coefficients c[0 .. d) says: a[k] = c[0] * a[k - 1] + c[1] * a[k - 2] + ... + c[d - 1] * a[k - d] (modulo kMod) for every k >= d.
// Scratch sizes are in u32 words; scratch is working memory with unspecified contents afterwards (not tested) and needs only the alignment of u32.
// Output arrays and scratch must not overlap the input arrays or each other (outside the contract, not tested). Nothing is written on any failure.
//
// linrec_nth_scratch_words: *out_words = 4 * d (the algorithm may use less). Check order: a null out_words gives NullArgument; then d above
// kMaxOrder gives TooLarge. linrec_bm_scratch_words: *out_words = 3 * len + 3 for len above 0 and 0 for len 0; check order: a null out_words gives
// NullArgument; then len above kMaxLen gives TooLarge.
LinResult linrec_nth_scratch_words(std::uint32_t d, std::uint32_t* out_words);
LinResult linrec_bm_scratch_words(std::uint32_t len, std::uint32_t* out_words);
// linrec_nth: the sequence a has the recurrence c[0 .. d) and the first terms init[0 .. d) (a[i] = init[i] for i below d). *out = a[n] for any n from 0
// to 2^64 - 1. For d = 0 the sequence is empty of initial terms and every term is 0, so *out = 0. The number of steps is O(d^2 log n) (polynomial
// exponentiation modulo the characteristic polynomial); computing every term up to n is not acceptable for a large n.
// Check order: a null out (always), or coef, init and scratch (each only while d is above 0), gives NullArgument; then d above kMaxOrder gives
// TooLarge; then, only while d is above 0, scratch_words below 4 * d gives ScratchTooSmall; then an entry of coef or init not below kMod gives
// ValueOutOfRange.
LinResult linrec_nth(const std::uint32_t* coef, const std::uint32_t* init, std::uint32_t d, std::uint64_t n, std::uint32_t* scratch,
                     std::uint32_t scratch_words, std::uint32_t* out);
// linrec_bm: the shortest linear recurrence that generates seq[0 .. len): *out_d = L, the least order such that coefficients c[0 .. L) exist with
// seq[k] = c[0] * seq[k - 1] + ... + c[L - 1] * seq[k - L] for every k from L to len - 1, and out_coef[0 .. L) = such coefficients. L is always unique;
// L = 0 exactly when every entry of seq is 0 (and always for len = 0), and L = len when no shorter recurrence works (every k condition is then empty).
// The coefficients are unique when 2 * L <= len. When 2 * L > len more than one shortest recurrence can exist and WHICH ONE is returned is NOT
// specified: a test then checks only that the returned coefficients generate seq with order L. The number of steps is O(len^2).
// Check order: a null out_d (always), or seq, scratch and out_coef (each only while len is above 0), gives NullArgument; then len above kMaxLen gives
// TooLarge; then, only while len is above 0, scratch_words below 3 * len + 3 gives ScratchTooSmall; then an entry of seq not below kMod gives
// ValueOutOfRange; then, when out_cap is below L, OutTooSmall (the order is only known after the computation; nothing is written, not even *out_d).
// On Ok *out_d = L and only out_coef[0 .. L) is written. len = 0 is Ok with *out_d = 0 and nothing else written.
LinResult linrec_bm(const std::uint32_t* seq, std::uint32_t len, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_coef,
                    std::uint32_t out_cap, std::uint32_t* out_d);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::linrec
