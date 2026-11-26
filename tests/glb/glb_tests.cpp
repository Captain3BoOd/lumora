// Tests for core/glb. The files are built here byte by byte; the expected values are written out by hand, and the golden digest
// was computed by an independent Python script that builds the canonical stream from the intended model (not from the file).
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glb.h"

using namespace lumora::glb;
using Bytes = std::vector<std::uint8_t>;

namespace {

void put32(Bytes& b, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void putf(Bytes& b, float f) {
  std::uint32_t v;
  std::memcpy(&v, &f, 4);
  put32(b, v);
}
void put16(Bytes& b, std::uint32_t v) {
  b.push_back(static_cast<std::uint8_t>(v));
  b.push_back(static_cast<std::uint8_t>(v >> 8));
}

Bytes make_glb(std::string json, Bytes bin, bool with_bin = true) {
  while (json.size() % 4) json.push_back(' ');
  while (bin.size() % 4) bin.push_back(0);
  Bytes out;
  put32(out, 0x46546C67u);
  put32(out, 2);
  const std::uint32_t total = 12 + 8 + static_cast<std::uint32_t>(json.size()) + (with_bin ? 8 + static_cast<std::uint32_t>(bin.size()) : 0);
  put32(out, total);
  put32(out, static_cast<std::uint32_t>(json.size()));
  put32(out, 0x4E4F534Au);
  out.insert(out.end(), json.begin(), json.end());
  if (with_bin) {
    put32(out, static_cast<std::uint32_t>(bin.size()));
    put32(out, 0x004E4942u);
    out.insert(out.end(), bin.begin(), bin.end());
  }
  return out;
}

// BIN layout of the sample: [0,48) positions of the quad, [48,96) normals, [96,128) uvs, [128,140) indices u16, 12 bytes;
// [140,176) interleaved view of 3 vertices with stride 20: position (12) then uv (8).
Bytes sample_bin() {
  Bytes b;
  const float pos[12] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0};
  for (float f : pos) putf(b, f);
  for (int i = 0; i < 4; ++i) { putf(b, 0); putf(b, 0); putf(b, 1); }
  const float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
  for (float f : uv) putf(b, f);
  const std::uint32_t idx[6] = {0, 1, 2, 2, 1, 3};
  for (std::uint32_t i : idx) put16(b, i);
  const float p1[9] = {0, 0, 0, 2, 0, 0, 0, 2, 0};
  const float u1[6] = {0, 0, 1, 0, 0, 1};
  for (int v = 0; v < 3; ++v) {
    for (int k = 0; k < 3; ++k) putf(b, p1[v * 3 + k]);
    putf(b, u1[v * 2]);
    putf(b, u1[v * 2 + 1]);
  }
  return b;
}

const char* kSampleJson =
    R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":176}],"bufferViews":[)"
    R"({"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},)"
    R"({"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":12},)"
    R"({"buffer":0,"byteOffset":140,"byteLength":60,"byteStride":20}],)"
    R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},)"
    R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},)"
    R"({"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},)"
    R"({"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"},)"
    R"({"bufferView":4,"componentType":5126,"count":3,"type":"VEC3"},)"
    R"({"bufferView":4,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC2"}],)"
    R"("meshes":[{"name":"quad","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3},)"
    R"({"attributes":{"POSITION":4,"TEXCOORD_0":5},"material":7}]}]})";

}  // namespace

TEST_CASE("status_text names", "[glb]") {
  REQUIRE(std::string(status_text(Status::Ok)) == "ok");
  REQUIRE(std::string(status_text(Status::TooSmall)) == "too small");
  REQUIRE(std::string(status_text(Status::BadMagic)) == "bad magic");
  REQUIRE(std::string(status_text(Status::BadVersion)) == "bad version");
  REQUIRE(std::string(status_text(Status::BadLength)) == "bad length");
  REQUIRE(std::string(status_text(Status::BadChunk)) == "bad chunk");
  REQUIRE(std::string(status_text(Status::BadJson)) == "bad json");
  REQUIRE(std::string(status_text(Status::BadSchema)) == "bad schema");
  REQUIRE(std::string(status_text(Status::BadBuffer)) == "bad buffer");
  REQUIRE(std::string(status_text(Status::BadAccessor)) == "bad accessor");
  REQUIRE(std::string(status_text(Status::LimitExceeded)) == "limit exceeded");
  REQUIRE(std::string(status_text(Status::Unsupported)) == "unsupported");
  REQUIRE(std::string(status_text(static_cast<Status>(99))) == "unknown");
}

TEST_CASE("a valid file loads with exact values", "[glb]") {
  // The sample's BIN is 200 bytes in the buffer declaration; fix the declared length to the real size first.
  std::string json = kSampleJson;
  json.replace(json.find("\"byteLength\":176"), 16, "\"byteLength\":200");
  const Bytes f = make_glb(json, sample_bin());
  const LoadResult r = load(f.data(), f.size());
  REQUIRE(r.status == Status::Ok);
  REQUIRE(r.model.meshes.size() == 1);
  const Mesh& m = r.model.meshes[0];
  REQUIRE(m.name == "quad");
  REQUIRE(m.primitives.size() == 2);
  const Primitive& a = m.primitives[0];
  REQUIRE(a.vertex_count() == 4);
  REQUIRE(a.positions == std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0});
  REQUIRE(a.normals == std::vector<float>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1});
  REQUIRE(a.uvs == std::vector<float>{0, 0, 1, 0, 0, 1, 1, 1});
  REQUIRE(a.indices == std::vector<std::uint32_t>{0, 1, 2, 2, 1, 3});
  REQUIRE(a.material == kNoMaterial);
  const Primitive& b = m.primitives[1];
  REQUIRE(b.positions == std::vector<float>{0, 0, 0, 2, 0, 0, 0, 2, 0});
  REQUIRE(b.normals.empty());
  REQUIRE(b.uvs == std::vector<float>{0, 0, 1, 0, 0, 1});
  REQUIRE(b.indices == std::vector<std::uint32_t>{0, 1, 2});
  REQUIRE(b.material == 7);
  REQUIRE(digest_model(r.model) == 0x0a3d1eac2b13743dull);  // from the independent Python reference
}

namespace {
Bytes good() {
  std::string json = kSampleJson;
  json.replace(json.find("\"byteLength\":176"), 16, "\"byteLength\":200");
  return make_glb(json, sample_bin());
}
Status st(const Bytes& b, const Limits& l = {}) { return load(b.data(), b.size(), l).status; }
std::string with(const std::string& from, const std::string& to) {
  std::string json = kSampleJson;
  json.replace(json.find("\"byteLength\":176"), 16, "\"byteLength\":200");
  const std::size_t at = json.find(from);
  REQUIRE(at != std::string::npos);
  json.replace(at, from.size(), to);
  return json;
}
}  // namespace

TEST_CASE("container errors", "[glb]") {
  Bytes g = good();
  REQUIRE(st(Bytes{}) == Status::TooSmall);
  REQUIRE(st(Bytes(19, 0)) == Status::TooSmall);
  { Bytes b = g; b[0] = 'X'; REQUIRE(st(b) == Status::BadMagic); }
  { Bytes b = g; b[4] = 1; REQUIRE(st(b) == Status::BadVersion); }
  { Bytes b = g; b[8] ^= 4; REQUIRE(st(b) == Status::BadLength); }
  { Bytes b = g; b.push_back(0); REQUIRE(st(b) == Status::BadLength); }
  { Bytes b = g; b[16] = 'X'; REQUIRE(st(b) == Status::BadChunk); }  // first chunk type is not JSON
  { Bytes b = g; b[12] += 4; REQUIRE(st(b) == Status::BadChunk); }   // JSON chunk length moves the BIN header
  { Bytes b = g; b[12] = 0xF9; b[13] = 0xFF; REQUIRE(st(b) == Status::BadChunk); }  // misaligned and beyond the file
  { Limits l; l.max_file_bytes = 100; REQUIRE(st(g, l) == Status::BadLength); }
  { Limits l; l.max_json_bytes = 16; REQUIRE(st(g, l) == Status::LimitExceeded); }
  { Bytes b = g; put32(b, 8); put32(b, 0x004E4942u); b[8] += 8; REQUIRE(st(b) == Status::BadChunk); }  // a second chunk after BIN
  { Bytes b = make_glb(kSampleJson, Bytes(), false); REQUIRE(st(b) == Status::BadBuffer); }  // no BIN, buffer declared
}

TEST_CASE("json and schema errors", "[glb]") {
  Bytes bin = sample_bin();
  REQUIRE(st(make_glb("{", bin)) == Status::BadJson);
  REQUIRE(st(make_glb("[]", bin)) == Status::BadJson);
  REQUIRE(st(make_glb(R"({"asset":{}})", bin)) == Status::BadSchema);
  REQUIRE(st(make_glb(R"({"asset":{"version":"1.0"}})", bin)) == Status::BadSchema);
  REQUIRE(st(make_glb(R"({"asset":{"version":"2.0"}})", bin)) == Status::Ok);  // an empty model is valid
  REQUIRE(st(make_glb(R"({"asset":{"version":"2.0"},"extensionsRequired":["KHR_draco_mesh_compression"]})", bin)) == Status::Unsupported);
  REQUIRE(st(make_glb(with("\"NORMAL\":1", "\"NORMAL\":99"), sample_bin())) == Status::BadSchema);
  REQUIRE(st(make_glb(with("\"POSITION\":0", "\"POSITION\":-1"), sample_bin())) == Status::BadSchema);
  REQUIRE(st(make_glb(with("\"POSITION\":0", "\"POSITION\":0.5"), sample_bin())) == Status::BadSchema);
  REQUIRE(st(make_glb(with("\"type\":\"VEC3\"},{\"bufferView\":1", "\"type\":7},{\"bufferView\":1"), sample_bin())) == Status::BadSchema);
  REQUIRE(st(make_glb(with("\"primitives\":[", "\"primitives\":{}, \"x\":["), sample_bin())) == Status::BadSchema);
  REQUIRE(st(make_glb(with("\"name\":\"quad\"", "\"name\":5"), sample_bin())) == Status::BadSchema);
}

TEST_CASE("unsupported subset", "[glb]") {
  REQUIRE(st(make_glb(with("\"material\":7", "\"material\":7,\"mode\":1"), sample_bin())) == Status::Unsupported);
  REQUIRE(st(make_glb(with("\"componentType\":5123", "\"componentType\":5123,\"sparse\":{}"), sample_bin())) == Status::Unsupported);
  REQUIRE(st(make_glb(with("\"byteLength\":48},{\"buffer\":0,\"byteOffset\":48", "\"byteLength\":48},{\"buffer\":1,\"byteOffset\":48"), sample_bin())) == Status::Unsupported);
  REQUIRE(st(make_glb(with("\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"},{\"bufferView\":1", "\"componentType\":5122,\"count\":4,\"type\":\"VEC3\"},{\"bufferView\":1"), sample_bin())) == Status::Unsupported);
  REQUIRE(st(make_glb(with("\"buffers\":[{\"byteLength\":200}]", "\"buffers\":[{\"byteLength\":200},{\"byteLength\":4}]"), sample_bin())) == Status::Unsupported);
  REQUIRE(st(make_glb(with("{\"byteLength\":200}", "{\"byteLength\":200,\"uri\":\"a.bin\"}"), sample_bin())) == Status::Unsupported);
}

TEST_CASE("buffer and accessor errors", "[glb]") {
  REQUIRE(st(make_glb(with("\"byteLength\":200", "\"byteLength\":300"), sample_bin())) == Status::BadBuffer);
  REQUIRE(st(make_glb(with("\"byteLength\":200", "\"byteLength\":100"), sample_bin())) == Status::BadBuffer);  // BIN longer than the buffer by 4 or more
  REQUIRE(st(make_glb(with("\"byteOffset\":128,\"byteLength\":12", "\"byteOffset\":190,\"byteLength\":12"), sample_bin())) == Status::BadBuffer);
  REQUIRE(st(make_glb(with("\"byteStride\":20", "\"byteStride\":18"), sample_bin())) == Status::BadBuffer);
  REQUIRE(st(make_glb(with("\"byteStride\":20", "\"byteStride\":256"), sample_bin())) == Status::BadBuffer);
  REQUIRE(st(make_glb(with("\"byteStride\":20", "\"byteStride\":8"), sample_bin())) == Status::BadBuffer);  // smaller than the element
  REQUIRE(st(make_glb(with("\"count\":4,\"type\":\"VEC3\"},{\"bufferView\":1", "\"count\":5,\"type\":\"VEC3\"},{\"bufferView\":1"), sample_bin())) == Status::BadAccessor);  // beyond its view
  REQUIRE(st(make_glb(with("\"count\":6,\"type\":\"SCALAR\"", "\"count\":5,\"type\":\"SCALAR\""), sample_bin())) == Status::BadAccessor);  // not a multiple of 3
  REQUIRE(st(make_glb(with("\"count\":6,\"type\":\"SCALAR\"", "\"count\":0,\"type\":\"SCALAR\""), sample_bin())) == Status::BadAccessor);
  REQUIRE(st(make_glb(with("\"bufferView\":4,\"byteOffset\":12", "\"bufferView\":4,\"byteOffset\":2"), sample_bin())) == Status::BadBuffer);  // misaligned float
  REQUIRE(st(make_glb(with("\"bufferView\":1,\"componentType\":5126,\"count\":4", "\"bufferView\":1,\"componentType\":5126,\"count\":3"), sample_bin())) == Status::BadAccessor);  // normal count differs
}

TEST_CASE("an index beyond the vertex count and non-finite floats are rejected", "[glb]") {
  Bytes bin = sample_bin();
  bin[128] = 9;  // first index becomes 9 (vertex count 4)
  Bytes f = make_glb(with("\"byteLength\":200", "\"byteLength\":200"), bin);
  REQUIRE(st(f) == Status::BadAccessor);
  Bytes nan = sample_bin();
  nan[0] = 0; nan[1] = 0; nan[2] = 0xC0; nan[3] = 0x7F;  // quiet NaN as the first position x
  REQUIRE(st(make_glb(with("\"byteLength\":200", "\"byteLength\":200"), nan)) == Status::BadAccessor);
  Bytes inf = sample_bin();
  inf[0] = 0; inf[1] = 0; inf[2] = 0x80; inf[3] = 0x7F;
  REQUIRE(st(make_glb(with("\"byteLength\":200", "\"byteLength\":200"), inf)) == Status::BadAccessor);
}

TEST_CASE("limits", "[glb]") {
  const Bytes g = good();
  { Limits l; l.max_meshes = 0; REQUIRE(st(g, l) == Status::LimitExceeded); }
  { Limits l; l.max_primitives = 1; REQUIRE(st(g, l) == Status::LimitExceeded); }
  { Limits l; l.max_vertices = 3; REQUIRE(st(g, l) == Status::LimitExceeded); }
  { Limits l; l.max_indices = 5; REQUIRE(st(g, l) == Status::LimitExceeded); }
  { Limits l; l.max_total_vertices = 6; REQUIRE(st(g, l) == Status::LimitExceeded); }
  { Limits l; l.max_total_vertices = 7; REQUIRE(st(g, l) == Status::Ok); }
}

TEST_CASE("index types and implicit indices", "[glb]") {
  // u8 and u32 indices over the same 4 vertices
  for (int kind = 0; kind < 2; ++kind) {
    Bytes bin;
    const float pos[12] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0};
    for (float f : pos) putf(bin, f);
    const std::uint32_t idx[6] = {3, 2, 1, 0, 1, 2};
    for (std::uint32_t i : idx) { if (kind == 0) bin.push_back(static_cast<std::uint8_t>(i)); else put32(bin, i); }
    while (bin.size() % 4) bin.push_back(0);
    const std::string ct = kind == 0 ? "5121" : "5125";
    const std::string ilen = kind == 0 ? "6" : "24";
    const std::string json = std::string(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)") + std::to_string(bin.size()) +
        R"(}],"bufferViews":[{"buffer":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":)" + ilen +
        R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":)" + ct +
        R"(,"count":6,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
    const Bytes f = make_glb(json, bin);
    const LoadResult r = load(f.data(), f.size());
    REQUIRE(r.status == Status::Ok);
    REQUIRE(r.model.meshes[0].primitives[0].indices == std::vector<std::uint32_t>{3, 2, 1, 0, 1, 2});
  }
  // no indices: sequential, vertex count must be a multiple of 3
  Bytes bin;
  for (int i = 0; i < 9; ++i) putf(bin, static_cast<float>(i));
  const std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],)"
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}]})";
  const Bytes f = make_glb(json, bin);
  const LoadResult r = load(f.data(), f.size());
  REQUIRE(r.status == Status::Ok);
  REQUIRE(r.model.meshes[0].primitives[0].indices == std::vector<std::uint32_t>{0, 1, 2});
  const std::string bad = std::string(json).replace(json.find("\"count\":3"), 9, "\"count\":2");
  const Bytes g = make_glb(bad, bin);
  REQUIRE(st(g) == Status::BadAccessor);
}

TEST_CASE("long mesh names are cut to 128 bytes", "[glb]") {
  const std::string name(300, 'n');
  const Bytes f = make_glb(with("\"quad\"", "\"" + name + "\""), sample_bin());
  const LoadResult r = load(f.data(), f.size());
  REQUIRE(r.status == Status::Ok);
  REQUIRE(r.model.meshes[0].name == std::string(128, 'n'));
}

TEST_CASE("every truncation and every single byte change is handled without a crash", "[glb]") {
  const Bytes g = good();
  for (std::size_t n = 0; n < g.size(); ++n) REQUIRE(load(g.data(), n).status != Status::Ok);
  std::size_t ok = 0;
  for (std::size_t i = 0; i < g.size(); ++i) {
    for (int bit = 0; bit < 8; ++bit) {
      Bytes b = g;
      b[i] ^= static_cast<std::uint8_t>(1u << bit);
      const LoadResult r = load(b.data(), b.size());
      if (r.ok()) {
        ++ok;
        for (const Mesh& m : r.model.meshes)
          for (const Primitive& p : m.primitives)
            for (std::uint32_t idx : p.indices) REQUIRE(idx < p.vertex_count());
      } else {
        REQUIRE(r.model.meshes.empty());
      }
    }
  }
  REQUIRE(ok < g.size() * 8);
}
