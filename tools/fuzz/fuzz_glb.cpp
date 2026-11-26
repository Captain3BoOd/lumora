// Fuzz target: glb. Feeds raw bytes to lumora::glb::load with small limits. A success must give vertex arrays that are finite
// and consistent, indices below the vertex count in whole triangles, and a stable digest. A failure must leave the model empty.
// Anything else aborts, so the fuzzer reports it as a crash.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "glb.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::glb;
  Limits lim;
  lim.max_file_bytes = 1u << 20;
  lim.max_json_bytes = 1u << 16;
  lim.max_vertices = 1u << 14;
  lim.max_indices = 3u << 14;
  lim.max_total_vertices = 1u << 15;
  const LoadResult r = load(data, size, lim);
  if (!r.ok()) {
    if (!r.model.meshes.empty()) std::abort();
    return 0;
  }
  for (const Mesh& m : r.model.meshes) {
    for (const Primitive& p : m.primitives) {
      if (p.positions.size() % 3 != 0) std::abort();
      const std::uint32_t n = p.vertex_count();
      if (!p.normals.empty() && p.normals.size() != p.positions.size()) std::abort();
      if (!p.uvs.empty() && p.uvs.size() != static_cast<std::size_t>(n) * 2) std::abort();
      if (p.indices.size() % 3 != 0) std::abort();
      for (float f : p.positions) if (!std::isfinite(f)) std::abort();
      for (float f : p.normals) if (!std::isfinite(f)) std::abort();
      for (float f : p.uvs) if (!std::isfinite(f)) std::abort();
      for (std::uint32_t i : p.indices) if (i >= n) std::abort();
    }
  }
  if (digest_model(r.model) != digest_model(r.model)) std::abort();
  return 0;
}
