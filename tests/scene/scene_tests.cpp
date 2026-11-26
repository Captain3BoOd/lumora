// Tests for the scene package metadata code: the JSON model and the manifest policy.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "json_value.h"
#include "manifest.h"

using namespace lumora::scene;

namespace {
const char* kGood = R"({
  "format_version": 1, "scene_schema_version": 1, "lua_api_version": 1,
  "min_engine_version": "0.0.1", "name": "Aurora", "author": "A. Writer",
  "entry": {"scene": "scene.json", "settings": "settings.json"},
  "backends": ["vulkan", "gles"], "future_field": {"anything": [1, 2, 3]}
})";

std::string with(const std::string& from, const std::string& to) {
  std::string s = kGood;
  const auto p = s.find(from);
  REQUIRE(p != std::string::npos);
  s.replace(p, from.size(), to);
  return s;
}
}  // namespace

TEST_CASE("json parses scalars, containers and keeps key order") {
  auto r = parse_json(R"({"b": [1, 2.5, -3e2, true, false, null, "x"], "a": {}})");
  REQUIRE(r.ok);
  REQUIRE(r.value.is_object());
  CHECK(r.value.as_object()[0].first == "b");
  CHECK(r.value.as_object()[1].first == "a");
  const auto& arr = r.value.find("b")->as_array();
  REQUIRE(arr.size() == 7);
  CHECK(arr[1].as_number() == 2.5);
  CHECK(arr[2].as_number() == -300.0);
  CHECK(arr[3].as_bool());
  CHECK(arr[5].is_null());
  CHECK(arr[6].as_string() == "x");
}

TEST_CASE("json decodes escapes and surrogate pairs") {
  auto r = parse_json(R"("a\n\t\"\\\/\u00e9\ud83d\ude00")");
  REQUIRE(r.ok);
  CHECK(r.value.as_string() == "a\n\t\"\\/\xC3\xA9\xF0\x9F\x98\x80");
}

TEST_CASE("json rejects malformed input with an offset") {
  const char* bad[] = {"", "   ", "{", "[1,]", "{\"a\":}", "{\"a\" 1}", "[1 2]", "tru", "01", "1.", "1e", "-",
                       "\"abc", "\"\\x\"", "\"\\u12\"", "\"\\ud800\"", "\"\\udc00\"", "{} x", "[\"a\nb\"]",
                       "{\"a\":1,\"a\":2}", "nul", "{1:2}"};
  for (const char* t : bad) {
    INFO(t);
    auto r = parse_json(t);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
  }
}

TEST_CASE("json depth limit is enforced without recursion blow-up") {
  std::string deep(100000, '[');
  CHECK_FALSE(parse_json(deep, 32).ok);
  std::string ok32 = std::string(32, '[') + std::string(32, ']');
  CHECK(parse_json(ok32, 32).ok);
  std::string over = std::string(33, '[') + std::string(33, ']');
  CHECK_FALSE(parse_json(over, 32).ok);
}

TEST_CASE("semver parser accepts only major.minor.patch digits") {
  SemVer v;
  REQUIRE(parse_semver("1.22.333", v));
  CHECK(v.major == 1);
  CHECK(v.minor == 22);
  CHECK(v.patch == 333);
  for (const char* bad : {"", "1", "1.2", "1.2.3.4", "1..2", "v1.2.3", "1.2.3-rc1", "1.2.x", ".1.2", "1.2.", "99999999999.0.0"}) {
    INFO(bad);
    CHECK_FALSE(parse_semver(bad, v));
  }
  SemVer a{0, 9, 9}, b{1, 0, 0};
  CHECK(a < b);
}

TEST_CASE("manifest accepts a good package and ignores unknown fields") {
  auto r = parse_manifest(kGood);
  REQUIRE(r.ok());
  CHECK(r.manifest.name == "Aurora");
  CHECK(r.manifest.author == "A. Writer");
  CHECK(r.manifest.scene_path == "scene.json");
  CHECK(r.manifest.settings_path == "settings.json");
  CHECK(r.manifest.backends.size() == 2);
}

TEST_CASE("manifest rejects newer-than-supported versions and engines") {
  CHECK(parse_manifest(with("\"format_version\": 1", "\"format_version\": 2")).error == ManifestError::FormatTooNew);
  CHECK(parse_manifest(with("\"scene_schema_version\": 1", "\"scene_schema_version\": 2")).error == ManifestError::SchemaTooNew);
  CHECK(parse_manifest(with("\"lua_api_version\": 1", "\"lua_api_version\": 2")).error == ManifestError::LuaApiTooNew);
  CHECK(parse_manifest(with("0.0.1", "0.0.2")).error == ManifestError::EngineTooOld);
  EngineSupport newer;
  newer.engine_version = {0, 0, 2};
  CHECK(parse_manifest(with("0.0.1", "0.0.2"), newer).ok());
}

TEST_CASE("manifest reports missing fields and wrong types by name") {
  for (const char* key : {"format_version", "scene_schema_version", "lua_api_version", "min_engine_version", "name", "entry"}) {
    INFO(key);
    // Build a minimal manifest by hand, leaving out the one key under test.
    std::string text = "{";
    bool first = true;
    auto add = [&](const std::string& k, const std::string& v) {
      if (k == key) return;
      text += (first ? "" : ",") + std::string("\"") + k + "\":" + v;
      first = false;
    };
    add("format_version", "1");
    add("scene_schema_version", "1");
    add("lua_api_version", "1");
    add("min_engine_version", "\"0.0.1\"");
    add("name", "\"x\"");
    add("entry", "{\"scene\":\"scene.json\"}");
    text += "}";
    auto r = parse_manifest(text);
    CHECK(r.error == ManifestError::MissingField);
    CHECK(r.message.find(key) != std::string::npos);
  }
  CHECK(parse_manifest(with("\"name\": \"Aurora\"", "\"name\": 5")).error == ManifestError::WrongType);
  CHECK(parse_manifest(with("\"format_version\": 1", "\"format_version\": \"1\"")).error == ManifestError::WrongType);
  CHECK(parse_manifest("[]").error == ManifestError::NotAnObject);
  CHECK(parse_manifest("{").error == ManifestError::NotJson);
}

TEST_CASE("manifest rejects bad values") {
  CHECK(parse_manifest(with("\"format_version\": 1", "\"format_version\": 0")).error == ManifestError::BadValue);
  CHECK(parse_manifest(with("\"format_version\": 1", "\"format_version\": 1.5")).error == ManifestError::BadValue);
  CHECK(parse_manifest(with("\"format_version\": 1", "\"format_version\": -1")).error == ManifestError::BadValue);
  CHECK(parse_manifest(with("\"Aurora\"", "\"\"")).error == ManifestError::BadValue);
  CHECK(parse_manifest(with("\"Aurora\"", "\"" + std::string(129, 'a') + "\"")).error == ManifestError::BadValue);
  CHECK(parse_manifest(with("0.0.1", "0.1")).error == ManifestError::BadValue);
}

TEST_CASE("manifest entry paths must stay inside the package") {
  for (const char* p : {"../scene.json", "/scene.json", "a//b.json", "a\\\\b.json", "C:/x.json", "./scene.json", "", "a/../b"}) {
    INFO(p);
    CHECK(parse_manifest(with("\"scene.json\"", std::string("\"") + p + "\"")).error == ManifestError::BadValue);
  }
  CHECK(parse_manifest(with("\"scene.json\"", "\"scenes/main.scene.json\"")).ok());
}

TEST_CASE("manifest backends are checked and queried") {
  CHECK(parse_manifest(with("\"vulkan\", \"gles\"", "\"metal\"")).error == ManifestError::UnknownBackend);
  CHECK(parse_manifest(with("[\"vulkan\", \"gles\"]", "\"vulkan\"")).error == ManifestError::WrongType);
  auto r = parse_manifest(kGood);
  REQUIRE(r.ok());
  CHECK(manifest_supports_backend(r.manifest, "vulkan"));
  CHECK(manifest_supports_backend(r.manifest, "gles"));
  CHECK_FALSE(manifest_supports_backend(r.manifest, "d3d12"));
  Manifest full;
  CHECK(manifest_supports_backend(full, "d3d12"));  // no list means a full package
}

TEST_CASE("manifest parser applies the JSON limits before parsing", "[scene][limits]") {
  const std::string huge(70000, 'x');
  const auto r = parse_manifest("{\"author\":\"" + huge + "\"}");
  CHECK(r.error == ManifestError::NotJson);
  CHECK(r.message.find("JSON limits") != std::string::npos);
}
