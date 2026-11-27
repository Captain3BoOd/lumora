// Generational handle allocator. See slot_map.h for the contract and docs/slot_map.md for the check order and the bounds. A handle is
// (generation << 8) | index. Allocation and liveness read the used flags and the stored generation only; count is informational and is
// checked only against kSlots. A generation above kMaxGeneration can exist only in a hand-built map. No floating point, no libm, no
// allocation.
#include "slot_map.h"

#include <cstdint>

namespace lumora::slotmap {

namespace {

constexpr std::uint32_t kIndexMask = 255;

bool count_ok(const SlotMap* map) { return map->count <= kSlots; }

}  // namespace

void init(SlotMap* map) {
  if (map == nullptr) return;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    map->gen[i] = 1;
    map->used[i] = 0;
  }
  map->count = 0;
}

bool allocate(SlotMap* map, std::uint32_t* handle) {
  if (map == nullptr || handle == nullptr || !count_ok(map)) return false;
  for (std::uint32_t i = 0; i < kSlots; ++i) {
    if (map->used[i] == 0) {
      *handle = ((map->gen[i] & kMaxGeneration) << 8) | i;
      map->used[i] = 1;
      ++map->count;
      return true;
    }
  }
  return false;
}

bool is_live(const SlotMap* map, std::uint32_t handle) {
  if (map == nullptr || !count_ok(map)) return false;
  const std::uint32_t generation = handle >> 8;
  if (generation == 0) return false;
  const std::uint32_t index = handle & kIndexMask;
  return map->used[index] != 0 && map->gen[index] == generation;
}

bool release(SlotMap* map, std::uint32_t handle) {
  if (!is_live(map, handle)) return false;
  const std::uint32_t index = handle & kIndexMask;
  map->used[index] = 0;
  map->gen[index] = map->gen[index] == kMaxGeneration ? 1u : map->gen[index] + 1u;
  if (map->count > 0) --map->count;
  return true;
}

std::uint32_t size(const SlotMap* map) { return map != nullptr && count_ok(map) ? map->count : 0; }

}  // namespace lumora::slotmap
