#include "manifest.h"
#include "../package/json_limits.h"

#include <cmath>
#include <limits>

namespace lumora::scene {

bool parse_semver(std::string_view text, SemVer& out) {
  std::uint64_t parts[3] = {0, 0, 0};
  int index = 0;
  bool have_digit = false;
  for (char c : text) {
    if (c >= '0' && c <= '9') {
      parts[index] = parts[index] * 10 + static_cast<std::uint64_t>(c - '0');
      if (parts[index] > std::numeric_limits<std::uint32_t>::max()) return false;
      have_digit = true;
    } else if (c == '.' && have_digit && index < 2) {
      ++index;
      have_digit = false;
    } else {
      return false;
    }
  }
  if (index != 2 || !have_digit) return false;
  out = {static_cast<std::uint32_t>(parts[0]), static_cast<std::uint32_t>(parts[1]),
         static_cast<std::uint32_t>(parts[2])};
  return true;
}

namespace {

ManifestResult fail(ManifestError e, std::string msg) {
  ManifestResult r;
  r.error = e;
  r.message = std::move(msg);
  return r;
}

// Package-relative path: not empty, no backslash, no leading slash, no drive prefix, no ".." or empty
// component. The full archive policy lives in core/package/entry_policy.h.
bool safe_relative_path(const std::string& p) {
  if (p.empty() || p.size() > 255 || p.front() == '/') return false;
  if (p.size() >= 2 && p[1] == ':') return false;
  std::size_t start = 0;
  while (start <= p.size()) {
    const std::size_t end = p.find('/', start);
    const std::string part = p.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (part.empty() || part == "." || part == "..") return false;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  for (char c : p)
    if (c == '\\' || static_cast<unsigned char>(c) < 0x20 || c == 0x7F) return false;
  return true;
}

// Reads a required whole-number field in [1, 2^31).
ManifestError read_version(const JsonValue& root, const char* key, std::uint32_t& out, std::string& msg) {
  const JsonValue* v = root.find(key);
  if (!v) {
    msg = std::string("missing field: ") + key;
    return ManifestError::MissingField;
  }
  if (!v->is_number()) {
    msg = std::string("field must be a number: ") + key;
    return ManifestError::WrongType;
  }
  const double d = v->as_number();
  if (d < 1 || d >= 2147483648.0 || d != std::floor(d)) {
    msg = std::string("field must be a whole number of at least 1: ") + key;
    return ManifestError::BadValue;
  }
  out = static_cast<std::uint32_t>(d);
  return ManifestError::None;
}

}  // namespace

ManifestResult parse_manifest(std::string_view json_text, const EngineSupport& support) {
  {
    const auto scan = lumora::package::scan_json(json_text);
    if (scan.verdict != lumora::package::JsonVerdict::Ok)
      return fail(ManifestError::NotJson,
                  std::string("manifest.json rejected by the JSON limits: ") + lumora::package::to_string(scan.verdict));
  }
  JsonParseResult parsed = parse_json(json_text);
  if (!parsed.ok) return fail(ManifestError::NotJson, "manifest.json is not valid JSON: " + parsed.error);
  const JsonValue& root = parsed.value;
  if (!root.is_object()) return fail(ManifestError::NotAnObject, "manifest.json must be a JSON object");

  Manifest m;
  std::string msg;
  if (auto e = read_version(root, "format_version", m.format_version, msg); e != ManifestError::None) return fail(e, msg);
  if (auto e = read_version(root, "scene_schema_version", m.scene_schema_version, msg); e != ManifestError::None)
    return fail(e, msg);
  if (auto e = read_version(root, "lua_api_version", m.lua_api_version, msg); e != ManifestError::None) return fail(e, msg);
  if (m.format_version > support.max_format_version)
    return fail(ManifestError::FormatTooNew, "package format version " + std::to_string(m.format_version) +
                                                 " is newer than this engine supports (" +
                                                 std::to_string(support.max_format_version) + ")");
  if (m.scene_schema_version > support.max_scene_schema_version)
    return fail(ManifestError::SchemaTooNew, "scene schema version " + std::to_string(m.scene_schema_version) +
                                                 " is newer than this engine supports");
  if (m.lua_api_version > support.max_lua_api_version)
    return fail(ManifestError::LuaApiTooNew, "Lua API version " + std::to_string(m.lua_api_version) +
                                                 " is newer than this engine supports");

  const JsonValue* ver = root.find("min_engine_version");
  if (!ver) return fail(ManifestError::MissingField, "missing field: min_engine_version");
  if (!ver->is_string()) return fail(ManifestError::WrongType, "field must be a string: min_engine_version");
  if (!parse_semver(ver->as_string(), m.min_engine_version))
    return fail(ManifestError::BadValue, "min_engine_version must look like 1.2.3");
  if (m.min_engine_version > support.engine_version)
    return fail(ManifestError::EngineTooOld, "the package needs a newer engine than this one");

  const JsonValue* name = root.find("name");
  if (!name) return fail(ManifestError::MissingField, "missing field: name");
  if (!name->is_string()) return fail(ManifestError::WrongType, "field must be a string: name");
  if (name->as_string().empty() || name->as_string().size() > 128)
    return fail(ManifestError::BadValue, "name must be 1 to 128 bytes");
  m.name = name->as_string();

  if (const JsonValue* author = root.find("author")) {
    if (!author->is_string()) return fail(ManifestError::WrongType, "field must be a string: author");
    if (author->as_string().size() > 128) return fail(ManifestError::BadValue, "author is longer than 128 bytes");
    m.author = author->as_string();
  }

  const JsonValue* entry = root.find("entry");
  if (!entry) return fail(ManifestError::MissingField, "missing field: entry");
  if (!entry->is_object()) return fail(ManifestError::WrongType, "field must be an object: entry");
  const JsonValue* scene = entry->find("scene");
  if (!scene) return fail(ManifestError::MissingField, "missing field: entry.scene");
  if (!scene->is_string()) return fail(ManifestError::WrongType, "field must be a string: entry.scene");
  if (!safe_relative_path(scene->as_string())) return fail(ManifestError::BadValue, "entry.scene is not a safe package path");
  m.scene_path = scene->as_string();
  if (const JsonValue* settings = entry->find("settings")) {
    if (!settings->is_string()) return fail(ManifestError::WrongType, "field must be a string: entry.settings");
    if (!safe_relative_path(settings->as_string()))
      return fail(ManifestError::BadValue, "entry.settings is not a safe package path");
    m.settings_path = settings->as_string();
  }
  if (const JsonValue* script = entry->find("script")) {
    if (!script->is_string()) return fail(ManifestError::WrongType, "field must be a string: entry.script");
    if (!safe_relative_path(script->as_string()))
      return fail(ManifestError::BadValue, "entry.script is not a safe package path");
    m.script_path = script->as_string();
  }

  if (const JsonValue* backends = root.find("backends")) {
    if (!backends->is_array()) return fail(ManifestError::WrongType, "field must be an array: backends");
    for (const JsonValue& b : backends->as_array()) {
      if (!b.is_string()) return fail(ManifestError::WrongType, "backends must contain strings");
      const std::string& s = b.as_string();
      if (s != "vulkan" && s != "gles" && s != "d3d12")
        return fail(ManifestError::UnknownBackend, "unknown backend in backends: " + s);
      m.backends.push_back(s);
    }
  }

  ManifestResult r;
  r.manifest = std::move(m);
  return r;
}

bool manifest_supports_backend(const Manifest& m, std::string_view backend) {
  if (m.backends.empty()) return true;
  for (const auto& b : m.backends)
    if (b == backend) return true;
  return false;
}

}  // namespace lumora::scene
