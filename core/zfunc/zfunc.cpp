// Z array and border array of a byte string (contract in zfunc.h). Both arrays are built by the classic linear scans: the Z scan keeps
// the window [l, r) that matches a prefix of the string, the border scan keeps the length k of the current border and falls back through earlier
// borders. Integer only, no allocation, no recursion.
#include "zfunc.h"

namespace lumora::zfunc {

namespace {
using u32 = std::uint32_t;
}  // namespace

ZResult zfunc_z(u32 n, const std::uint8_t* s, u32* out_z) {
  if (n > 0 && (s == nullptr || out_z == nullptr)) return ZResult::NullArgument;
  if (n > kMaxLen) return ZResult::TooLarge;
  if (n == 0) return ZResult::Ok;
  out_z[0] = n;
  u32 l = 0, r = 0;  // s[l .. r) equals s[0 .. r - l), r is the right end of the rightmost match found so far
  for (u32 i = 1; i < n; ++i) {
    u32 z = 0;
    if (i < r) {
      z = out_z[i - l];
      if (z > r - i) z = r - i;
    }
    while (i + z < n && s[z] == s[i + z]) ++z;
    out_z[i] = z;
    if (i + z > r) {
      l = i;
      r = i + z;
    }
  }
  return ZResult::Ok;
}

ZResult zfunc_pi(u32 n, const std::uint8_t* s, u32* out_pi) {
  if (n > 0 && (s == nullptr || out_pi == nullptr)) return ZResult::NullArgument;
  if (n > kMaxLen) return ZResult::TooLarge;
  if (n == 0) return ZResult::Ok;
  out_pi[0] = 0;
  u32 k = 0;
  for (u32 i = 1; i < n; ++i) {
    while (k > 0 && s[k] != s[i]) k = out_pi[k - 1];
    if (s[k] == s[i]) ++k;
    out_pi[i] = k;
  }
  return ZResult::Ok;
}

}  // namespace lumora::zfunc
