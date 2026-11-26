#include "glb.h"

#include <cmath>
#include <cstring>
#include <string_view>

#include "json_value.h"

namespace lumora::glb {

namespace {

using lumora::scene::JsonValue;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr u32 kMagic = 0x46546C67u;  // "glTF" little endian
constexpr u32 kChunkJson = 0x4E4F534Au;
constexpr u32 kChunkBin = 0x004E4942u;

u32 rd32(const std::uint8_t* p) {
  return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
}

// A JSON number that is a non-negative integer below 2^31, else false.
bool as_index(const JsonValue* v, u64* out) {
  if (v == nullptr || !v->is_number()) return false;
  const double d = v->as_number();
  if (!(d >= 0.0) || d >= 2147483648.0 || d != std::floor(d)) return false;
  *out = static_cast<u64>(d);
  return true;
}

struct BufferView {
  u64 offset = 0, length = 0, stride = 0;  // stride 0 means tightly packed
};

struct Accessor {
  u64 view = 0, offset = 0, count = 0;
  u32 component = 0;
  int components = 0;  // 1, 2, 3
};

int type_components(const std::string& t) {
  if (t == "SCALAR") return 1;
  if (t == "VEC2") return 2;
  if (t == "VEC3") return 3;
  return 0;
}

u32 component_size(u32 c) {
  switch (c) {
    case 5121: return 1;
    case 5123: return 2;
    case 5125: return 4;
    case 5126: return 4;
    default: return 0;
  }
}

struct Ctx {
  const std::uint8_t* bin = nullptr;
  u64 bin_size = 0;
  std::vector<BufferView> views;
  std::vector<Accessor> accessors;
};

// Checks that the accessor lies inside its view and the view inside the BIN chunk. Returns the byte address of element 0 and the
// stride in bytes through the out parameters.
Status locate_accessor(const Ctx& c, const Accessor& a, u32 elem_size, u64* start, u64* stride) {
  if (a.view >= c.views.size()) return Status::BadSchema;
  const BufferView& v = c.views[static_cast<std::size_t>(a.view)];
  const u64 csize = component_size(a.component);
  const u64 packed = csize * static_cast<u64>(a.components);
  if (packed != elem_size) return Status::BadAccessor;
  const u64 st = v.stride != 0 ? v.stride : packed;
  if (st < packed) return Status::BadBuffer;
  if (a.count == 0) return Status::BadAccessor;
  // The last element ends at offset + (count - 1) * stride + packed; count < 2^31 and stride <= 252, so this fits in u64.
  const u64 last_end = a.offset + (a.count - 1) * st + packed;
  if (last_end > v.length) return Status::BadAccessor;
  const u64 abs = v.offset + a.offset;
  if (abs % csize != 0 || st % csize != 0) return Status::BadBuffer;
  if (v.offset + v.length > c.bin_size) return Status::BadBuffer;
  *start = abs;
  *stride = st;
  return Status::Ok;
}

float rd_float(const std::uint8_t* p) {
  const u32 bits = rd32(p);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

Status read_floats(const Ctx& c, const Accessor& a, std::vector<float>* out) {
  if (a.component != 5126) return Status::Unsupported;
  u64 start = 0, stride = 0;
  const Status s = locate_accessor(c, a, 4u * static_cast<u32>(a.components), &start, &stride);
  if (s != Status::Ok) return s;
  out->reserve(static_cast<std::size_t>(a.count) * static_cast<std::size_t>(a.components));
  for (u64 i = 0; i < a.count; ++i) {
    const std::uint8_t* p = c.bin + start + i * stride;
    for (int k = 0; k < a.components; ++k) {
      const float f = rd_float(p + 4 * k);
      if (!std::isfinite(f)) return Status::BadAccessor;
      out->push_back(f);
    }
  }
  return Status::Ok;
}

Status read_indices(const Ctx& c, const Accessor& a, u32 vertex_count, std::vector<u32>* out) {
  if (a.components != 1) return Status::BadAccessor;
  if (a.component != 5121 && a.component != 5123 && a.component != 5125) return Status::BadAccessor;
  u64 start = 0, stride = 0;
  const Status s = locate_accessor(c, a, component_size(a.component), &start, &stride);
  if (s != Status::Ok) return s;
  if (a.count % 3 != 0) return Status::BadAccessor;
  out->reserve(static_cast<std::size_t>(a.count));
  for (u64 i = 0; i < a.count; ++i) {
    const std::uint8_t* p = c.bin + start + i * stride;
    u32 v;
    if (a.component == 5121) v = p[0];
    else if (a.component == 5123) v = static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8);
    else v = rd32(p);
    if (v >= vertex_count) return Status::BadAccessor;
    out->push_back(v);
  }
  return Status::Ok;
}

const Accessor* accessor_at(const Ctx& c, u64 i) { return i < c.accessors.size() ? &c.accessors[static_cast<std::size_t>(i)] : nullptr; }

Status parse_views(const JsonValue& root, Ctx* c, const Limits&) {
  const JsonValue* bv = root.find("bufferViews");
  if (bv == nullptr) return Status::Ok;
  if (!bv->is_array()) return Status::BadSchema;
  for (const JsonValue& e : bv->as_array()) {
    if (!e.is_object()) return Status::BadSchema;
    u64 buffer = 0, len = 0, off = 0, stride = 0;
    if (!as_index(e.find("buffer"), &buffer) || !as_index(e.find("byteLength"), &len)) return Status::BadSchema;
    if (const JsonValue* o = e.find("byteOffset")) { if (!as_index(o, &off)) return Status::BadSchema; }
    if (const JsonValue* s = e.find("byteStride")) {
      if (!as_index(s, &stride)) return Status::BadSchema;
      if (stride < 4 || stride > 252 || stride % 4 != 0) return Status::BadBuffer;
    }
    if (buffer != 0) return Status::Unsupported;  // only the embedded BIN chunk
    if (off + len > c->bin_size) return Status::BadBuffer;
    c->views.push_back(BufferView{off, len, stride});
  }
  return Status::Ok;
}

Status parse_accessors(const JsonValue& root, Ctx* c) {
  const JsonValue* ac = root.find("accessors");
  if (ac == nullptr) return Status::Ok;
  if (!ac->is_array()) return Status::BadSchema;
  for (const JsonValue& e : ac->as_array()) {
    if (!e.is_object()) return Status::BadSchema;
    Accessor a;
    u64 ct = 0;
    if (e.find("sparse") != nullptr) return Status::Unsupported;
    const JsonValue* t = e.find("type");
    if (t == nullptr || !t->is_string()) return Status::BadSchema;
    a.components = type_components(t->as_string());
    if (!as_index(e.find("componentType"), &ct) || !as_index(e.find("count"), &a.count)) return Status::BadSchema;
    if (a.components == 0) return Status::Unsupported;
    a.component = static_cast<u32>(ct);
    if (component_size(a.component) == 0) return Status::Unsupported;
    const JsonValue* bvw = e.find("bufferView");
    if (bvw == nullptr) return Status::Unsupported;  // zero-filled accessors are not supported
    if (!as_index(bvw, &a.view)) return Status::BadSchema;
    if (const JsonValue* o = e.find("byteOffset")) { if (!as_index(o, &a.offset)) return Status::BadSchema; }
    c->accessors.push_back(a);
  }
  return Status::Ok;
}

Status parse_meshes(const JsonValue& root, const Ctx& c, const Limits& lim, Model* model) {
  const JsonValue* ms = root.find("meshes");
  if (ms == nullptr) return Status::Ok;
  if (!ms->is_array()) return Status::BadSchema;
  if (ms->as_array().size() > lim.max_meshes) return Status::LimitExceeded;
  u32 prims = 0;
  u64 total_vertices = 0;
  for (const JsonValue& m : ms->as_array()) {
    if (!m.is_object()) return Status::BadSchema;
    Mesh mesh;
    if (const JsonValue* n = m.find("name")) {
      if (!n->is_string()) return Status::BadSchema;
      mesh.name = n->as_string().substr(0, 128);
    }
    const JsonValue* ps = m.find("primitives");
    if (ps == nullptr || !ps->is_array()) return Status::BadSchema;
    for (const JsonValue& p : ps->as_array()) {
      if (!p.is_object()) return Status::BadSchema;
      if (++prims > lim.max_primitives) return Status::LimitExceeded;
      u64 mode = 4;
      if (const JsonValue* md = p.find("mode")) { if (!as_index(md, &mode)) return Status::BadSchema; }
      if (mode != 4) return Status::Unsupported;
      const JsonValue* attrs = p.find("attributes");
      if (attrs == nullptr || !attrs->is_object()) return Status::BadSchema;
      u64 ai = 0;
      if (!as_index(attrs->find("POSITION"), &ai)) return Status::BadSchema;
      Primitive prim;
      const Accessor* pos = accessor_at(c, ai);
      if (pos == nullptr) return Status::BadSchema;
      if (pos->components != 3) return Status::BadAccessor;
      if (pos->count > lim.max_vertices) return Status::LimitExceeded;
      total_vertices += pos->count;
      if (total_vertices > lim.max_total_vertices) return Status::LimitExceeded;
      Status s = read_floats(c, *pos, &prim.positions);
      if (s != Status::Ok) return s;
      if (const JsonValue* nv = attrs->find("NORMAL")) {
        if (!as_index(nv, &ai)) return Status::BadSchema;
        const Accessor* a = accessor_at(c, ai);
        if (a == nullptr) return Status::BadSchema;
        if (a->components != 3 || a->count != pos->count) return Status::BadAccessor;
        s = read_floats(c, *a, &prim.normals);
        if (s != Status::Ok) return s;
      }
      if (const JsonValue* tv = attrs->find("TEXCOORD_0")) {
        if (!as_index(tv, &ai)) return Status::BadSchema;
        const Accessor* a = accessor_at(c, ai);
        if (a == nullptr) return Status::BadSchema;
        if (a->components != 2 || a->count != pos->count) return Status::BadAccessor;
        s = read_floats(c, *a, &prim.uvs);
        if (s != Status::Ok) return s;
      }
      if (const JsonValue* iv = p.find("indices")) {
        if (!as_index(iv, &ai)) return Status::BadSchema;
        const Accessor* a = accessor_at(c, ai);
        if (a == nullptr) return Status::BadSchema;
        if (a->count > lim.max_indices) return Status::LimitExceeded;
        s = read_indices(c, *a, static_cast<u32>(pos->count), &prim.indices);
        if (s != Status::Ok) return s;
      } else {
        if (pos->count % 3 != 0) return Status::BadAccessor;
        prim.indices.reserve(static_cast<std::size_t>(pos->count));
        for (u32 i = 0; i < static_cast<u32>(pos->count); ++i) prim.indices.push_back(i);
      }
      if (const JsonValue* mt = p.find("material")) {
        if (!as_index(mt, &ai)) return Status::BadSchema;
        prim.material = static_cast<u32>(ai);
      }
      mesh.primitives.push_back(std::move(prim));
    }
    model->meshes.push_back(std::move(mesh));
  }
  return Status::Ok;
}

Status load_impl(const std::uint8_t* data, std::size_t size, const Limits& lim, Model* model) {
  if (size < 12 + 8) return Status::TooSmall;
  if (data == nullptr) return Status::TooSmall;
  if (rd32(data) != kMagic) return Status::BadMagic;
  if (rd32(data + 4) != 2) return Status::BadVersion;
  const u64 total = rd32(data + 8);
  if (total != size || total > lim.max_file_bytes || total % 4 != 0) return Status::BadLength;
  u64 pos = 12;
  const u64 jlen = rd32(data + pos);
  if (rd32(data + pos + 4) != kChunkJson) return Status::BadChunk;
  pos += 8;
  if (jlen % 4 != 0 || pos + jlen > total) return Status::BadChunk;
  if (jlen > lim.max_json_bytes) return Status::LimitExceeded;
  const std::string_view json(reinterpret_cast<const char*>(data + pos), static_cast<std::size_t>(jlen));
  pos += jlen;
  Ctx c;
  if (pos < total) {
    if (pos + 8 > total) return Status::BadChunk;
    const u64 blen = rd32(data + pos);
    if (rd32(data + pos + 4) != kChunkBin) return Status::BadChunk;
    pos += 8;
    if (blen % 4 != 0 || pos + blen > total) return Status::BadChunk;
    c.bin = data + pos;
    c.bin_size = blen;
    pos += blen;
    if (pos != total) return Status::BadChunk;
  }
  // The JSON size is capped by max_json_bytes above and the parser enforces its own depth cap (16 here).
  const lumora::scene::JsonParseResult pr = lumora::scene::parse_json(json, 16);
  if (!pr.ok || !pr.value.is_object()) return Status::BadJson;
  const JsonValue& root = pr.value;
  const JsonValue* asset = root.find("asset");
  if (asset == nullptr || !asset->is_object()) return Status::BadSchema;
  const JsonValue* ver = asset->find("version");
  if (ver == nullptr || !ver->is_string() || ver->as_string().compare(0, 2, "2.") != 0) return Status::BadSchema;
  if (const JsonValue* req = root.find("extensionsRequired")) {
    if (!req->is_array()) return Status::BadSchema;
    if (!req->as_array().empty()) return Status::Unsupported;
  }
  if (const JsonValue* bufs = root.find("buffers")) {
    if (!bufs->is_array()) return Status::BadSchema;
    if (bufs->as_array().size() > 1) return Status::Unsupported;
    for (const JsonValue& b : bufs->as_array()) {
      if (!b.is_object()) return Status::BadSchema;
      u64 len = 0;
      if (!as_index(b.find("byteLength"), &len)) return Status::BadSchema;
      if (b.find("uri") != nullptr) return Status::Unsupported;
      if (len > c.bin_size || c.bin_size - len >= 4) return Status::BadBuffer;
    }
  }
  Status s = parse_views(root, &c, lim);
  if (s != Status::Ok) return s;
  s = parse_accessors(root, &c);
  if (s != Status::Ok) return s;
  return parse_meshes(root, c, lim, model);
}

void put32(std::uint64_t* h, u32 v) {
  for (int b = 0; b < 4; ++b) {
    *h ^= static_cast<u64>(v & 0xFFu);
    *h *= 0x100000001b3ull;
    v >>= 8;
  }
}

void put_floats(std::uint64_t* h, const std::vector<float>& f) {
  put32(h, static_cast<u32>(f.size()));
  for (float x : f) {
    u32 bits;
    std::memcpy(&bits, &x, 4);
    put32(h, bits);
  }
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::TooSmall: return "too small";
    case Status::BadMagic: return "bad magic";
    case Status::BadVersion: return "bad version";
    case Status::BadLength: return "bad length";
    case Status::BadChunk: return "bad chunk";
    case Status::BadJson: return "bad json";
    case Status::BadSchema: return "bad schema";
    case Status::BadBuffer: return "bad buffer";
    case Status::BadAccessor: return "bad accessor";
    case Status::LimitExceeded: return "limit exceeded";
    case Status::Unsupported: return "unsupported";
  }
  return "unknown";
}

LoadResult load(const std::uint8_t* data, std::size_t size, const Limits& limits) {
  LoadResult r;
  Model m;
  r.status = load_impl(data, size, limits, &m);
  if (r.status == Status::Ok) r.model = std::move(m);
  return r;
}

std::uint64_t digest_model(const Model& m) {
  u64 h = 0xcbf29ce484222325ull;
  put32(&h, static_cast<u32>(m.meshes.size()));
  for (const Mesh& me : m.meshes) {
    put32(&h, static_cast<u32>(me.name.size()));
    for (char ch : me.name) put32(&h, static_cast<u32>(static_cast<unsigned char>(ch)));
    put32(&h, static_cast<u32>(me.primitives.size()));
    for (const Primitive& p : me.primitives) {
      put32(&h, p.vertex_count());
      put32(&h, static_cast<u32>(p.indices.size()));
      put32(&h, p.material);
      put_floats(&h, p.positions);
      put_floats(&h, p.normals);
      put_floats(&h, p.uvs);
      for (u32 i : p.indices) put32(&h, i);
    }
  }
  return h;
}

}  // namespace lumora::glb
