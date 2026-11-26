#include "scene.h"
#include "../package/json_limits.h"

#include <set>

#include "checksums.h"
#include "json_value.h"

namespace lumora::scene {
namespace {
struct Fail {
  SceneError e;
  std::string m;
};

float num(const JsonValue& o, const char* key, float def, const std::string& where) {
  const JsonValue* v = o.find(key);
  if (!v) return def;
  if (!v->is_number()) throw Fail{SceneError::WrongType, where + "." + key + " must be a number"};
  const double d = v->as_number();
  if (!(d > -1e9 && d < 1e9)) throw Fail{SceneError::BadValue, where + "." + key + " out of range"};
  return static_cast<float>(d);
}

void vec3(const JsonValue& o, const char* key, float out[3], const std::string& where) {
  const JsonValue* v = o.find(key);
  if (!v) return;
  if (!v->is_array() || v->as_array().size() != 3) throw Fail{SceneError::WrongType, where + "." + key + " must be [x,y,z]"};
  for (int i = 0; i < 3; ++i) {
    const auto& e = v->as_array()[static_cast<std::size_t>(i)];
    if (!e.is_number() || !(e.as_number() > -1e9 && e.as_number() < 1e9))
      throw Fail{SceneError::BadValue, where + "." + key + " has a bad component"};
    out[i] = static_cast<float>(e.as_number());
  }
}

std::string str(const JsonValue& o, const char* key, bool required, const std::string& where) {
  const JsonValue* v = o.find(key);
  if (!v) {
    if (required) throw Fail{SceneError::MissingField, where + "." + key + " is required"};
    return {};
  }
  if (!v->is_string()) throw Fail{SceneError::WrongType, where + "." + key + " must be a string"};
  if (required && v->as_string().empty()) throw Fail{SceneError::BadValue, where + "." + key + " is empty"};
  return v->as_string();
}

Node parse_node(const JsonValue& j, unsigned depth, unsigned max_depth, std::set<std::string>& ids,
                const std::set<std::string>& assets, const std::string& where) {
  if (!j.is_object()) throw Fail{SceneError::WrongType, where + " must be an object"};
  if (depth > max_depth) throw Fail{SceneError::TooDeep, where + " nests too deeply"};
  Node n;
  n.id = str(j, "id", true, where);
  if (!ids.insert(n.id).second) throw Fail{SceneError::DuplicateId, "duplicate node id " + n.id};
  n.name = str(j, "name", false, where);
  if (const JsonValue* t = j.find("transform")) {
    if (!t->is_object()) throw Fail{SceneError::WrongType, where + ".transform must be an object"};
    vec3(*t, "position", n.transform.position, where + ".transform");
    vec3(*t, "scale", n.transform.scale, where + ".transform");
    n.transform.rotation_z_degrees = num(*t, "rotation_z_degrees", 0, where + ".transform");
  }
  if (const JsonValue* l = j.find("layer")) {
    if (!l->is_object()) throw Fail{SceneError::WrongType, where + ".layer must be an object"};
    n.layer_asset = str(*l, "asset", true, where + ".layer");
    if (!assets.count(n.layer_asset)) throw Fail{SceneError::UnknownAsset, "unknown asset " + n.layer_asset};
    n.parallax_depth = num(*l, "parallax_depth", 0, where + ".layer");
    if (n.parallax_depth < 0 || n.parallax_depth > 1) throw Fail{SceneError::BadValue, where + ".layer.parallax_depth must be in [0,1]"};
  }
  if (const JsonValue* c = j.find("children")) {
    if (!c->is_array()) throw Fail{SceneError::WrongType, where + ".children must be an array"};
    std::size_t i = 0;
    for (const auto& ch : c->as_array())
      n.children.push_back(parse_node(ch, depth + 1, max_depth, ids, assets, where + ".children[" + std::to_string(i++) + "]"));
  }
  return n;
}
}  // namespace

SceneResult parse_scene(std::string_view text, unsigned max_node_depth) {
  SceneResult r;
  try {
    {
      // Two JSON levels per node (object plus children array); keep the pre-scan above the node depth check so
      // the TooDeep error keeps its meaning.
      lumora::package::JsonLimits limits;
      limits.max_depth = 2 * max_node_depth + 8;
      const auto scan = lumora::package::scan_json(text, limits);
      if (scan.verdict != lumora::package::JsonVerdict::Ok)
        throw Fail{SceneError::NotJson,
                   std::string("rejected by the JSON limits: ") + lumora::package::to_string(scan.verdict)};
    }
    auto p = parse_json(text);
    if (!p.ok) throw Fail{SceneError::NotJson, p.error};
    if (!p.value.is_object()) throw Fail{SceneError::WrongType, "scene must be an object"};
    std::set<std::string> asset_ids;
    if (const JsonValue* a = p.value.find("assets")) {
      if (!a->is_array()) throw Fail{SceneError::WrongType, "assets must be an array"};
      std::size_t i = 0;
      for (const auto& e : a->as_array()) {
        const std::string w = "assets[" + std::to_string(i++) + "]";
        if (!e.is_object()) throw Fail{SceneError::WrongType, w + " must be an object"};
        Asset as{str(e, "id", true, w), str(e, "kind", true, w), str(e, "path", true, w)};
        if (as.kind != "texture") throw Fail{SceneError::BadValue, w + ".kind is not supported"};
        if (!asset_ids.insert(as.id).second) throw Fail{SceneError::DuplicateId, "duplicate asset id " + as.id};
        r.scene.assets.push_back(std::move(as));
      }
    }
    const JsonValue* root = p.value.find("root");
    if (!root) throw Fail{SceneError::MissingField, "root is required"};
    std::set<std::string> ids;
    r.scene.root = parse_node(*root, 1, max_node_depth, ids, asset_ids, "root");
  } catch (const Fail& f) {
    r = SceneResult{};
    r.error = f.e;
    r.message = f.m;
  }
  return r;
}

LoadResult load_package(const FileMap& files, const EngineSupport& support) {
  LoadResult r;
  auto fail = [&](LoadError e, std::string m) {
    r.error = e;
    r.message = std::move(m);
    return r;
  };
  // A package that carries a signature block cannot be trusted by a loader that cannot verify it: reject, never ignore.
  if (files.count(kSignatureEntry)) return fail(LoadError::SignatureUnsupported, "package carries a signature block that this version cannot verify");
  const auto cs = files.find("checksums.txt");
  if (cs == files.end()) return fail(LoadError::MissingChecksums, "checksums.txt is missing");
  std::map<std::string, Sha256Digest> sums;
  std::string err;
  const std::string cs_text(cs->second.begin(), cs->second.end());
  if (!parse_checksums(cs_text, sums, err)) return fail(LoadError::BadChecksums, err);
  // Verify before any content is parsed. Every file except checksums.txt must be listed and match.
  for (const auto& [path, data] : files) {
    if (path == "checksums.txt") continue;
    if (!sums.count(path)) return fail(LoadError::UnlistedFile, "file not covered by checksums.txt: " + path);
    if (!verify_file(sums, path, data.data(), data.size())) return fail(LoadError::ChecksumMismatch, "checksum mismatch: " + path);
  }
  for (const auto& kv : sums)
    if (!files.count(kv.first)) return fail(LoadError::MissingFile, "listed file missing: " + kv.first);

  auto text_of = [&](const std::string& p, std::string& out) {
    const auto it = files.find(p);
    if (it == files.end()) return false;
    out.assign(it->second.begin(), it->second.end());
    return true;
  };
  std::string t;
  if (!text_of("manifest.json", t)) return fail(LoadError::MissingFile, "manifest.json is missing");
  auto m = parse_manifest(t, support);
  if (!m.ok()) return fail(LoadError::BadManifest, m.message);
  r.manifest = m.manifest;
  if (!text_of(r.manifest.scene_path, t)) return fail(LoadError::MissingFile, "scene file is missing: " + r.manifest.scene_path);
  auto s = parse_scene(t);
  if (!s.ok()) return fail(LoadError::BadScene, s.message);
  for (const auto& a : s.scene.assets)
    if (!files.count(a.path)) return fail(LoadError::MissingFile, "asset file is missing: " + a.path);
  if (!r.manifest.script_path.empty()) {
    const auto it = files.find(r.manifest.script_path);
    if (it == files.end()) return fail(LoadError::MissingFile, "script file is missing: " + r.manifest.script_path);
    if (it->second.size() > kMaxScriptBytes)
      return fail(LoadError::BadManifest, "script file is larger than the allowed size: " + r.manifest.script_path);
  }
  r.scene = std::move(s.scene);
  return r;
}

}  // namespace lumora::scene
