// Prefix-function tools for a byte string: the Z array and the border (prefix function) array.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no scratch. The number of steps is linear in n. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::zfunc {
constexpr std::uint32_t kMaxLen = 262144;  // largest string length
enum class ZResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };
// The string is s[0 .. n) of raw bytes, compared exactly as values 0 to 255.
//
// zfunc_z: out_z[i] for i in 0 .. n-1 = the length of the longest common prefix of s and the suffix s[i .. n). So out_z[0] = n (the whole string) and
// out_z[i] is at most n - i. For n = 0 nothing is written.
// zfunc_pi: out_pi[i] for i in 0 .. n-1 = the length of the longest proper border of the prefix s[0 .. i], that is the largest k below i + 1 such that
// s[0 .. k) equals s[i + 1 - k .. i + 1) (0 when there is none). So out_pi[0] = 0. For n = 0 nothing is written.
// Check order for both functions: a null pointer argument (s and the output array, only while n is above 0) gives
// NullArgument; then n above kMaxLen gives TooLarge (the condition uses the given n even when it is above the limit). On failure nothing is written.
// On success only out_z[0 .. n) or out_pi[0 .. n) is written; the arrays must not overlap s.
ZResult zfunc_z(std::uint32_t n, const std::uint8_t* s, std::uint32_t* out_z);
ZResult zfunc_pi(std::uint32_t n, const std::uint8_t* s, std::uint32_t* out_pi);
}  // namespace lumora::zfunc
