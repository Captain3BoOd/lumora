// Binomial modulo a prime stub (contract in binmod.h). Owned by the binmod contributor; the implementation is not written yet.
#include "binmod.h"
namespace lumora::binmod {
BmResult binom_mod(std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t*) { return BmResult::BadPrime; }
BmResult binom_pvaluation(std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t*) { return BmResult::BadPrime; }
}  // namespace lumora::binmod
