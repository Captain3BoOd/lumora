// glTF 2.0 binary (.glb) loader for untrusted bytes. Pure CPU, no GPU, no file system, no floating point in decisions except
// finite checks. Every size is checked in 64-bit arithmetic against the limits before any allocation; failures return a status
// and an empty model. Supported subset (anything else returns Unsupported, never a guess): triangle primitives
// (mode 4), POSITION and optional NORMAL as FLOAT VEC3, optional TEXCOORD_0 as FLOAT VEC2, indices as UNSIGNED_BYTE, UNSIGNED_SHORT
// or UNSIGNED_INT scalars (or none: sequential), one embedded BIN chunk as buffer 0, no sparse accessors, no extensions that are
// listed in extensionsRequired. Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lumora::glb {

struct Limits {
  std::uint64_t max_file_bytes = 64ull << 20;
  std::uint64_t max_json_bytes = 4ull << 20;
  std::uint32_t max_meshes = 256;
  std::uint32_t max_primitives = 1024;           // in total
  std::uint32_t max_vertices = 1u << 22;         // per primitive
  std::uint32_t max_indices = 3u << 22;          // per primitive
  std::uint64_t max_total_vertices = 1ull << 23; // over the whole model
};

enum class Status {
  Ok,
  TooSmall,      // fewer bytes than header plus a JSON chunk header
  BadMagic,      // not "glTF"
  BadVersion,    // version is not 2
  BadLength,     // header length differs from the byte count, or exceeds max_file_bytes, or is not a multiple of 4
  BadChunk,      // chunk length or type wrong, chunk beyond the file, misaligned, first chunk not JSON, extra chunks after BIN
  BadJson,       // the JSON chunk does not parse or exceeds the structural limits
  BadSchema,     // a required property is missing or has the wrong type or an out of range index
  BadBuffer,     // buffer or buffer view does not fit inside the BIN chunk, bad stride or alignment
  BadAccessor,   // accessor does not fit its view, non-finite value, index out of range, count not a multiple of 3
  LimitExceeded, // a Limits value is exceeded
  Unsupported    // valid glTF but outside the supported subset (see above)
};
// Short fixed text, lower-case words: "ok", "too small", "bad magic", "bad version", "bad length", "bad chunk", "bad json",
// "bad schema", "bad buffer", "bad accessor", "limit exceeded", "unsupported". Never contains input bytes.
const char* status_text(Status s);

constexpr std::uint32_t kNoMaterial = 0xFFFFFFFFu;

struct Primitive {
  std::vector<float> positions;  // xyz per vertex, all finite
  std::vector<float> normals;    // xyz per vertex, empty or same vertex count as positions
  std::vector<float> uvs;        // uv per vertex, empty or same vertex count as positions
  std::vector<std::uint32_t> indices;  // triangle list, size multiple of 3, every value < vertex count
  std::uint32_t material = kNoMaterial;  // the glTF material index, no validation beyond the integer range
  std::uint32_t vertex_count() const { return static_cast<std::uint32_t>(positions.size() / 3); }
};

struct Mesh {
  std::string name;  // may be empty; at most 128 bytes, longer names are cut (bytes, not characters)
  std::vector<Primitive> primitives;
};

struct Model {
  std::vector<Mesh> meshes;
};

struct LoadResult {
  Status status = Status::TooSmall;
  Model model;
  bool ok() const { return status == Status::Ok; }
};

LoadResult load(const std::uint8_t* data, std::size_t size, const Limits& limits = {});

// FNV-1a 64 over a canonical byte stream of the model: mesh count, then per mesh the name length, each name byte as a 32-bit value, primitive count,
// and per primitive the vertex count, index count, material, then the raw bit patterns of positions, normals, uvs (each
// preceded by its float count) and the indices; every integer as 4 little-endian bytes. For golden tests.
std::uint64_t digest_model(const Model& m);

}  // namespace lumora::glb
