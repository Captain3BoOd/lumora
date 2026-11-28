// Collatz trajectories over u64 (contract in collatz.h). One shared walk serves both functions: it follows x -> x / 2 (even x) and x -> 3 x + 1 (odd x) from n
// until x is 1, counting the steps and keeping the largest value met. Integer only: no 128-bit types, no builtins or intrinsics, no allocation, no recursion,
// no mutable statics, no arrays, no iteration cap.
//
// Overflow rule: for an odd x the result 3 x + 1 fits a u64 exactly when x <= kMaxOdd = 0x5555555555555553 = 6148914691236517203. The next odd value,
// 0x5555555555555555 = 6148914691236517205, gives 3 x + 1 = 2^64. The test is the exact comparison x > kMaxX with kMaxX = 0x5555555555555554 (the even number
// between them, so "x > kMaxX" for an odd x means x >= 0x5555555555555555). Nothing is multiplied before the test, so no wrapped value is ever formed.
//
// Termination: the loop is not bounded by a counter. It ends for every u64 start because the call either reaches 1 or returns Overflow when an odd x is above
// kMaxX; that every start reaches 1 is the open Collatz conjecture and is not proved here (see docs/collatz.md for the external verification claims).
#include "collatz.h"

namespace lumora::collatz {
namespace {
using u64 = std::uint64_t;
constexpr u64 kMaxX = 0x5555555555555554ULL;

// Runs the trajectory; *steps and *peak are only written on Ok.
CzResult walk(u64 n, u64* steps, u64* peak) {
  u64 x = n;
  u64 s = 0u;
  u64 p = n;
  while (x != 1u) {
    if ((x & 1u) == 0u) {
      x >>= 1u;
    } else {
      if (x > kMaxX) return CzResult::Overflow;
      x = 3u * x + 1u;
      if (x > p) p = x;
    }
    ++s;
  }
  *steps = s;
  *peak = p;
  return CzResult::Ok;
}
}  // namespace

CzResult collatz_steps(std::uint64_t n, std::uint64_t* out) {
  if (out == nullptr) return CzResult::NullArgument;
  if (n == 0u) return CzResult::BadArgument;
  u64 s = 0u;
  u64 p = 0u;
  const CzResult r = walk(n, &s, &p);
  if (r != CzResult::Ok) return r;
  *out = s;
  return CzResult::Ok;
}

CzResult collatz_peak(std::uint64_t n, std::uint64_t* out) {
  if (out == nullptr) return CzResult::NullArgument;
  if (n == 0u) return CzResult::BadArgument;
  u64 s = 0u;
  u64 p = 0u;
  const CzResult r = walk(n, &s, &p);
  if (r != CzResult::Ok) return r;
  *out = p;
  return CzResult::Ok;
}
}  // namespace lumora::collatz
