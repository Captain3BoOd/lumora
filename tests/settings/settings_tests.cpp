// Tests for core/scene/settings: every header rule, every error code, every ValueCheck value, stored-value
// handling, visible_if, migrations, reset, hostile input and the aurora sample.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>

#include "pack_dir.h"
#include "settings.h"

using namespace lumora::scene;

namespace {

SettingsError err_of(const std::string& json) { return parse_settings_schema(json).error; }

// Wraps one setting object in a valid document.
std::string doc(const std::string& settings, const std::string& extra = "") {
  return "{\"settings_version\":3,\"settings\":[" + settings + "]" + extra + "}";
}

const char* kFloat = R"({"id":"density","label":"settings.density","type":"float","min":0,"max":1,"step":0.05,"default":0.5})";
const char* kInt = R"({"id":"count","label":"settings.count","type":"int","min":0,"max":10,"step":2,"default":4})";
const char* kEnum = R"({"id":"mode","label":"settings.mode","type":"enum","default":"calm","options":[{"id":"calm","label":"a"},{"id":"wild","label":"b"}]})";
const char* kColor = R"({"id":"sky","label":"settings.sky","type":"color","default":[0.1,0.2,0.5,1.0]})";
const char* kBool = R"({"id":"fast","label":"settings.fast","type":"bool","default":false})";

SettingsSchema full_schema() {
  const std::string d = doc(std::string(kFloat) + "," + kInt + "," + kEnum + "," + kColor + "," + kBool);
  const auto r = parse_settings_schema(d);
  REQUIRE(r.ok());
  return r.schema;
}

SettingValue bval(bool b) { SettingValue v; v.type = SettingType::Bool; v.b = b; return v; }
SettingValue ival(std::int64_t i) { SettingValue v; v.type = SettingType::Int; v.i = i; return v; }
SettingValue fval(double f) { SettingValue v; v.type = SettingType::Float; v.f = f; return v; }
SettingValue eval(const std::string& e) { SettingValue v; v.type = SettingType::Enum; v.e = e; return v; }
SettingValue cval(float r, float g, float b, float a) {
  SettingValue v;
  v.type = SettingType::Color;
  v.color[0] = r; v.color[1] = g; v.color[2] = b; v.color[3] = a;
  return v;
}

const SettingDef& def(const SettingsSchema& s, const std::string& id) {
  for (const auto& d : s.settings) if (d.id == id) return d;
  FAIL("no such setting " << id);
  return s.settings[0];
}

}  // namespace

TEST_CASE("a complete valid document parses and resolves to its defaults", "[settings]") {
  const std::string text = R"({
    "settings_version": 3,
    "settings": [
      { "id": "sky_color", "label": "settings.sky_color", "type": "color", "default": [0.1, 0.2, 0.5, 1.0], "color_space": "linear" },
      { "id": "density", "label": "settings.density", "type": "float", "min": 0, "max": 1, "step": 0.05, "default": 0.5, "scope": "device", "reset": "keep" },
      { "id": "mode", "label": "settings.mode", "type": "enum", "default": "calm",
        "options": [ {"id": "calm", "label": "settings.mode.calm"}, {"id": "wild", "label": "settings.mode.wild"} ], "future_field": [1, 2] },
      { "id": "fast", "label": "settings.fast", "type": "bool", "default": false, "visible_if": {"setting": "mode", "equals": "wild"} },
      { "id": "count", "label": "settings.count", "type": "int", "min": -5, "max": 5, "default": 1 }
    ],
    "migrations": [ { "from_version": 2, "rename": [ {"from": "speed", "to": "density"} ], "remove": ["old_id"] } ]
  })";
  const auto r = parse_settings_schema(text);
  REQUIRE(r.ok());
  const auto& s = r.schema;
  CHECK(s.settings_version == 3);
  REQUIRE(s.settings.size() == 5);
  CHECK(s.settings[0].color_space == "linear");
  CHECK(s.settings[0].default_value == cval(0.1f, 0.2f, 0.5f, 1.0f));
  CHECK(s.settings[1].scope == SettingScope::Device);
  CHECK(s.settings[1].reset == ResetBehavior::Keep);
  CHECK(s.settings[1].step == 0.05);
  CHECK(s.settings[2].options.size() == 2);
  CHECK(s.settings[3].visible_if.present);
  CHECK(s.settings[3].visible_if.value == eval("wild"));
  CHECK(s.settings[4].step == 0);
  CHECK(s.settings[4].scope == SettingScope::Scene);
  CHECK(s.settings[4].reset == ResetBehavior::Default);
  REQUIRE(s.migrations.size() == 1);
  CHECK(s.migrations[0].rename[0].second == "density");
  // Round trip: resolving nothing gives every default, resolving those values gives them back.
  std::vector<std::string> problems;
  const SettingsValues defaults = resolve_values(s, {}, &problems);
  CHECK(problems.empty());
  REQUIRE(defaults.size() == 5);
  CHECK(defaults.at("density") == fval(0.5));
  CHECK(defaults.at("mode") == eval("calm"));
  CHECK(defaults.at("fast") == bval(false));
  CHECK(defaults.at("count") == ival(1));
  CHECK(resolve_values(s, defaults, &problems) == defaults);
  CHECK(problems.empty());
}

TEST_CASE("parse errors: NotJson, WrongType, MissingField", "[settings][errors]") {
  CHECK(err_of("") == SettingsError::NotJson);
  CHECK(err_of("{not json") == SettingsError::NotJson);
  CHECK(err_of("[]") == SettingsError::WrongType);
  CHECK(err_of("{\"settings_version\":\"3\",\"settings\":[]}") == SettingsError::WrongType);
  CHECK(err_of("{\"settings_version\":1,\"settings\":{}}") == SettingsError::WrongType);
  CHECK(err_of("{\"settings\":[]}") == SettingsError::MissingField);
  CHECK(err_of("{\"settings_version\":1}") == SettingsError::MissingField);
  CHECK(err_of(doc("5")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":7,"label":"x","type":"bool","default":true})")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"label":"x","type":"bool","default":true})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","type":"bool","default":true})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","label":"x","default":true})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"bool"})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"bool","default":1})")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"bool","default":true,"scope":3})")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"float","max":1,"default":0})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"float","min":"0","max":1,"default":0})")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"enum","default":"a"})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"enum","default":"a","options":"a"})")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"enum","default":"a","options":[{"label":"z"}]})")) == SettingsError::MissingField);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"color","default":"red"})")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"color","default":[0,0,0,"1"]})")) == SettingsError::WrongType);
  CHECK(err_of(doc(kBool, ",\"migrations\":5")) == SettingsError::WrongType);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"rename\":[]}]")) == SettingsError::MissingField);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":1,\"rename\":[{\"from\":\"a\"}]}]")) == SettingsError::MissingField);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":1,\"remove\":[3]}]")) == SettingsError::WrongType);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"bool","default":true,"visible_if":{"equals":true}})")) == SettingsError::MissingField);
}

TEST_CASE("parse errors: BadValue", "[settings][errors]") {
  CHECK(err_of("{\"settings_version\":0,\"settings\":[]}") == SettingsError::BadValue);
  CHECK(err_of("{\"settings_version\":1.5,\"settings\":[]}") == SettingsError::BadValue);
  for (const char* id : {"", "A", "1a", "a-b", "a b", "_a", "ä"})
    CHECK(err_of(doc(std::string("{\"id\":\"") + id + R"(","label":"x","type":"bool","default":true})")) == SettingsError::BadValue);
  CHECK(err_of(doc("{\"id\":\"" + std::string(64, 'a') + R"(","label":"x","type":"bool","default":true})")) == SettingsError::None);
  CHECK(err_of(doc("{\"id\":\"" + std::string(65, 'a') + R"(","label":"x","type":"bool","default":true})")) == SettingsError::BadValue);
  CHECK(err_of(doc(R"({"id":"a","label":"","type":"bool","default":true})")) == SettingsError::BadValue);
  CHECK(err_of(doc("{\"id\":\"a\",\"label\":\"" + std::string(128, 'x') + R"(","type":"bool","default":true})")) == SettingsError::None);
  CHECK(err_of(doc("{\"id\":\"a\",\"label\":\"" + std::string(129, 'x') + R"(","type":"bool","default":true})")) == SettingsError::BadValue);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"vector","default":true})")) == SettingsError::BadValue);
  const std::string fl = R"({"id":"a","label":"x","type":"float",)";
  CHECK(err_of(doc(fl + R"("min":1,"max":1,"default":1})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":2,"max":1,"default":1})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":0,"max":2e9,"default":1})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":-2e9,"max":0,"default":0})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":0,"max":1e999,"default":0})")) != SettingsError::None);
  CHECK(err_of(doc(fl + R"("min":-1e9,"max":1e9,"default":0})")) == SettingsError::None);
  CHECK(err_of(doc(fl + R"("min":0,"max":1,"default":1.5})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":0,"max":1,"default":-0.1})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":0,"max":1,"step":0,"default":0.5})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":0,"max":1,"step":-0.1,"default":0.5})")) == SettingsError::BadValue);
  CHECK(err_of(doc(fl + R"("min":0,"max":1,"default":1})")) == SettingsError::None);  // whole-number float default is fine
  const std::string in = R"({"id":"a","label":"x","type":"int",)";
  CHECK(err_of(doc(in + R"("min":0.5,"max":3,"default":1})")) == SettingsError::BadValue);
  CHECK(err_of(doc(in + R"("min":0,"max":3.5,"default":1})")) == SettingsError::BadValue);
  CHECK(err_of(doc(in + R"("min":0,"max":4,"step":1.5,"default":0})")) == SettingsError::BadValue);
  CHECK(err_of(doc(in + R"("min":0,"max":4,"default":1.5})")) == SettingsError::BadValue);
  CHECK(err_of(doc(in + R"("min":0,"max":4,"default":5})")) == SettingsError::BadValue);
  CHECK(err_of(doc(in + R"("min":0,"max":4,"default":2})")) == SettingsError::None);
  const std::string en = R"({"id":"a","label":"x","type":"enum",)";
  CHECK(err_of(doc(en + R"("default":"a","options":[]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(en + R"("default":"zz","options":[{"id":"a","label":"l"}]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(en + R"("default":"a","options":[{"id":"A","label":"l"}]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(en + R"("default":"a","options":[{"id":"a","label":""}]})")) == SettingsError::BadValue);
  const std::string co = R"({"id":"a","label":"x","type":"color",)";
  CHECK(err_of(doc(co + R"("default":[0,0,0]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(co + R"("default":[0,0,0,1,1]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(co + R"("default":[0,0,0,1.5]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(co + R"("default":[-0.1,0,0,1]})")) == SettingsError::BadValue);
  CHECK(err_of(doc(co + R"("default":[0,0,0,1],"color_space":"p3"})")) == SettingsError::BadValue);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"bool","default":true,"scope":"global"})")) == SettingsError::BadValue);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"bool","default":true,"reset":"never"})")) == SettingsError::BadValue);
  // visible_if value must be valid for the referenced setting; both or neither comparison.
  const std::string base = std::string(kEnum) + ",";
  CHECK(err_of(doc(base + R"({"id":"b","label":"x","type":"bool","default":true,"visible_if":{"setting":"mode","equals":"nope"}})")) == SettingsError::BadValue);
  CHECK(err_of(doc(base + R"({"id":"b","label":"x","type":"bool","default":true,"visible_if":{"setting":"mode","equals":"calm","not_equals":"wild"}})")) == SettingsError::BadValue);
  CHECK(err_of(doc(base + R"({"id":"b","label":"x","type":"bool","default":true,"visible_if":{"setting":"mode"}})")) == SettingsError::MissingField);
  CHECK(err_of(doc(base + R"({"id":"b","label":"x","type":"bool","default":true,"visible_if":{"setting":"mode","equals":true}})")) == SettingsError::WrongType);
  CHECK(err_of(doc(base + R"({"id":"b","label":"x","type":"bool","default":true,"visible_if":7})")) == SettingsError::WrongType);
  // migrations
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":0}]")) == SettingsError::BadValue);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":1.5}]")) == SettingsError::BadValue);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":1,\"rename\":[{\"from\":\"A\",\"to\":\"b\"}]}]")) == SettingsError::BadValue);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":1,\"remove\":[\"Bad Id\"]}]")) == SettingsError::BadValue);
}

TEST_CASE("parse errors: DuplicateId, BadReference, TooMany", "[settings][errors]") {
  CHECK(err_of(doc(std::string(kBool) + "," + kBool)) == SettingsError::DuplicateId);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"enum","default":"a","options":[{"id":"a","label":"l"},{"id":"a","label":"m"}]})")) == SettingsError::DuplicateId);
  CHECK(err_of(doc(kBool, ",\"migrations\":[{\"from_version\":1},{\"from_version\":1}]")) == SettingsError::DuplicateId);
  const std::string tail = R"("label":"x","type":"bool","default":true)";
  // Self, forward and unknown references.
  CHECK(err_of(doc("{\"id\":\"a\"," + tail + R"(,"visible_if":{"setting":"a","equals":true}})")) == SettingsError::BadReference);
  CHECK(err_of(doc("{\"id\":\"a\"," + tail + R"(,"visible_if":{"setting":"b","equals":true}},{"id":"b",)" + tail + "}")) == SettingsError::BadReference);
  CHECK(err_of(doc("{\"id\":\"a\"," + tail + R"(,"visible_if":{"setting":"zzz","equals":true}})")) == SettingsError::BadReference);
  CHECK(err_of(doc("{\"id\":\"b\"," + tail + "},{\"id\":\"a\"," + tail + R"(,"visible_if":{"setting":"b","equals":true}})")) == SettingsError::None);
  // Limits: 256 settings are fine, 257 are too many; 64 options fine, 65 too many.
  auto many = [&](int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += (i ? "," : "") + std::string("{\"id\":\"s") + std::to_string(i) + "\"," + tail + "}";
    return doc(s);
  };
  CHECK(err_of(many(256)) == SettingsError::None);
  CHECK(err_of(many(257)) == SettingsError::TooMany);
  auto opts = [&](int n) {
    std::string o;
    for (int i = 0; i < n; ++i) o += (i ? "," : "") + std::string("{\"id\":\"o") + std::to_string(i) + R"(","label":"l"})";
    return doc("{\"id\":\"e\",\"label\":\"x\",\"type\":\"enum\",\"default\":\"o0\",\"options\":[" + o + "]}");
  };
  CHECK(err_of(opts(64)) == SettingsError::None);
  CHECK(err_of(opts(65)) == SettingsError::TooMany);
}

TEST_CASE("validate_value reaches every ValueCheck", "[settings][values]") {
  const SettingsSchema s = full_schema();
  SettingValue v = bval(true);
  CHECK(validate_value(def(s, "fast"), v) == ValueCheck::Ok);
  v = fval(0.5);
  CHECK(validate_value(def(s, "fast"), v) == ValueCheck::WrongType);
  v = ival(3);
  CHECK(validate_value(def(s, "density"), v) == ValueCheck::WrongType);  // int is not a float
  v = fval(4);
  CHECK(validate_value(def(s, "count"), v) == ValueCheck::WrongType);    // float is not an int
  v = fval(1.5);
  CHECK(validate_value(def(s, "density"), v) == ValueCheck::OutOfRange);
  v = ival(-2);
  CHECK(validate_value(def(s, "count"), v) == ValueCheck::OutOfRange);
  v = eval("wild");
  CHECK(validate_value(def(s, "mode"), v) == ValueCheck::Ok);
  v = eval("nope");
  CHECK(validate_value(def(s, "mode"), v) == ValueCheck::UnknownOption);
  v = cval(0.5f, 0.5f, 0.5f, 2.0f);
  CHECK(validate_value(def(s, "sky"), v) == ValueCheck::BadColor);
  v = cval(1.0f, 0.0f, 0.0f, 1.0f);
  CHECK(validate_value(def(s, "sky"), v) == ValueCheck::Ok);
  v = cval(std::nanf(""), 0, 0, 1);
  CHECK(validate_value(def(s, "sky"), v) == ValueCheck::BadColor);
  // UnknownSetting: a definition with no id (see docs/settings.md, header gap).
  v = bval(true);
  CHECK(validate_value(SettingDef{}, v) == ValueCheck::UnknownSetting);
  // NotOnStep: with min 0, max 1, step 0.4 the grid is 0, 0.4, 0.8; 1.0 would round to 1.2.
  const auto r = parse_settings_schema(doc(R"({"id":"g","label":"x","type":"float","min":0,"max":1,"step":0.4,"default":0.4})"));
  REQUIRE(r.ok());
  v = fval(1.0);
  CHECK(validate_value(r.schema.settings[0], v) == ValueCheck::NotOnStep);
  v = fval(0.95);
  CHECK(validate_value(r.schema.settings[0], v) == ValueCheck::Ok);
  CHECK(v.f == 0.8);
  const auto ri = parse_settings_schema(doc(R"({"id":"g","label":"x","type":"int","min":0,"max":5,"step":2,"default":2})"));
  REQUIRE(ri.ok());
  v = ival(5);  // 5 is 2.5 steps from 0: rounds up to 6 > max
  CHECK(validate_value(ri.schema.settings[0], v) == ValueCheck::NotOnStep);
}

TEST_CASE("step snapping, float and int", "[settings][values]") {
  const SettingsSchema s = full_schema();
  SettingValue v = fval(0.52);
  CHECK(validate_value(def(s, "density"), v) == ValueCheck::Ok);
  CHECK(std::fabs(v.f - 0.5) < 1e-12);
  v = fval(0.53);
  CHECK(validate_value(def(s, "density"), v) == ValueCheck::Ok);
  CHECK(std::fabs(v.f - 0.55) < 1e-12);
  v = fval(0.0);
  CHECK(validate_value(def(s, "density"), v) == ValueCheck::Ok);
  CHECK(v.f == 0.0);
  v = fval(1.0);
  CHECK(validate_value(def(s, "density"), v) == ValueCheck::Ok);
  CHECK(v.f == 1.0);
  // Half-step boundary on a binary-exact grid: min 0, step 0.5. Just under half snaps down, exactly half and
  // just over snap up.
  const auto r = parse_settings_schema(doc(R"({"id":"g","label":"x","type":"float","min":0,"max":2,"step":0.5,"default":0})"));
  REQUIRE(r.ok());
  const SettingDef& g = r.schema.settings[0];
  v = fval(0.2499);
  validate_value(g, v);
  CHECK(v.f == 0.0);
  v = fval(0.25);
  validate_value(g, v);
  CHECK(v.f == 0.5);
  v = fval(0.2501);
  validate_value(g, v);
  CHECK(v.f == 0.5);
  // Int grid min 0 step 2: 3 is a tie and snaps up to 4; 5 snaps to 6; 1 snaps to 2.
  v = ival(3);
  CHECK(validate_value(def(s, "count"), v) == ValueCheck::Ok);
  CHECK(v.i == 4);
  v = ival(5);
  validate_value(def(s, "count"), v);
  CHECK(v.i == 6);
  v = ival(4);
  validate_value(def(s, "count"), v);
  CHECK(v.i == 4);
  v = ival(10);
  CHECK(validate_value(def(s, "count"), v) == ValueCheck::Ok);
  CHECK(v.i == 10);
  // A grid anchored at a negative min.
  const auto n = parse_settings_schema(doc(R"({"id":"g","label":"x","type":"int","min":-5,"max":5,"step":5,"default":0})"));
  REQUIRE(n.ok());
  v = ival(-1);
  validate_value(n.schema.settings[0], v);
  CHECK(v.i == 0);
  v = ival(-3);
  validate_value(n.schema.settings[0], v);
  CHECK(v.i == -5);
}

TEST_CASE("NaN, infinity and huge numbers are rejected", "[settings][values]") {
  const SettingsSchema s = full_schema();
  for (double bad : {std::nan(""), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                     1e300, -1e300, 1e10}) {
    SettingValue v = fval(bad);
    CHECK(validate_value(def(s, "density"), v) == ValueCheck::OutOfRange);
  }
  SettingValue big = ival(std::numeric_limits<std::int64_t>::max());
  CHECK(validate_value(def(s, "count"), big) == ValueCheck::OutOfRange);
  big = ival(std::numeric_limits<std::int64_t>::min());
  CHECK(validate_value(def(s, "count"), big) == ValueCheck::OutOfRange);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"float","min":0,"max":1,"default":1e999})")) != SettingsError::None);
  CHECK(err_of(doc(R"({"id":"a","label":"x","type":"int","min":0,"max":1e9,"default":5000000000})")) == SettingsError::BadValue);
}

TEST_CASE("resolve_values drops unknown ids and replaces invalid values", "[settings][values]") {
  const SettingsSchema s = full_schema();
  SettingsValues stored;
  stored["density"] = fval(0.52);        // snaps to 0.5
  stored["count"] = fval(3);             // wrong type
  stored["mode"] = eval("wild");         // fine
  stored["sky"] = cval(2, 0, 0, 1);      // bad colour
  stored["gone"] = bval(true);           // unknown id
  std::vector<std::string> problems;
  const SettingsValues out = resolve_values(s, stored, &problems);
  CHECK(out.size() == 5);
  CHECK(out.count("gone") == 0);
  CHECK(std::fabs(out.at("density").f - 0.5) < 1e-12);
  CHECK(out.at("count") == ival(4));
  CHECK(out.at("mode") == eval("wild"));
  CHECK(out.at("sky") == cval(0.1f, 0.2f, 0.5f, 1.0f));
  CHECK(out.at("fast") == bval(false));
  REQUIRE(problems.size() == 3);
  CHECK(problems[0].rfind("count:", 0) == 0);
  CHECK(problems[1].rfind("sky:", 0) == 0);
  CHECK(problems[2].rfind("gone:", 0) == 0);
  // problems may be omitted.
  CHECK(resolve_values(s, stored).size() == 5);
}

TEST_CASE("visible_if: equals, not_equals, chains", "[settings][visible]") {
  const auto r = parse_settings_schema(doc(
      std::string(kEnum) + R"(,
      {"id":"a","label":"x","type":"bool","default":false,"visible_if":{"setting":"mode","equals":"wild"}},
      {"id":"b","label":"x","type":"bool","default":false,"visible_if":{"setting":"a","equals":true}},
      {"id":"c","label":"x","type":"bool","default":false,"visible_if":{"setting":"mode","not_equals":"wild"}},
      {"id":"plain","label":"x","type":"bool","default":false})"));
  REQUIRE(r.ok());
  const auto& s = r.schema;
  SettingsValues v = resolve_values(s, {});
  CHECK(is_visible(s, def(s, "plain"), v));
  CHECK(is_visible(s, def(s, "mode"), v));
  CHECK_FALSE(is_visible(s, def(s, "a"), v));  // mode is calm
  CHECK(is_visible(s, def(s, "c"), v));
  v["mode"] = eval("wild");
  CHECK(is_visible(s, def(s, "a"), v));
  CHECK_FALSE(is_visible(s, def(s, "c"), v));
  CHECK_FALSE(is_visible(s, def(s, "b"), v));  // a is false
  v["a"] = bval(true);
  CHECK(is_visible(s, def(s, "b"), v));
  // Chain: b equals its own rule but its parent a is hidden when mode goes back to calm.
  v["mode"] = eval("calm");
  CHECK_FALSE(is_visible(s, def(s, "a"), v));
  CHECK_FALSE(is_visible(s, def(s, "b"), v));
  // Missing values fall back to the referenced setting's default.
  CHECK_FALSE(is_visible(s, def(s, "a"), {}));
  CHECK(is_visible(s, def(s, "c"), {}));
}

TEST_CASE("visible_if on numeric and colour values", "[settings][visible]") {
  const auto r = parse_settings_schema(doc(
      std::string(kFloat) + "," + kInt + R"(,
      {"id":"x","label":"x","type":"bool","default":false,"visible_if":{"setting":"density","equals":0.52}},
      {"id":"y","label":"x","type":"bool","default":false,"visible_if":{"setting":"count","not_equals":4}})"));
  REQUIRE(r.ok());
  const auto& s = r.schema;
  CHECK(std::fabs(def(s, "x").visible_if.value.f - 0.5) < 1e-12);  // compared value is stored snapped
  SettingsValues v = resolve_values(s, {});
  CHECK(is_visible(s, def(s, "x"), v));
  CHECK_FALSE(is_visible(s, def(s, "y"), v));
  v["count"] = ival(6);
  CHECK(is_visible(s, def(s, "y"), v));
}

TEST_CASE("migrations: rename then remove, ordering and versions", "[settings][migrate]") {
  const auto r = parse_settings_schema(
      "{\"settings_version\":4,\"settings\":[" + std::string(kFloat) + "," + kBool + "],\"migrations\":["
      R"({"from_version":3,"rename":[{"from":"b","to":"c"}]},)"
      R"({"from_version":1,"rename":[{"from":"a","to":"b"},{"from":"zz","to":"density"}],"remove":["old","b"]},)"
      R"({"from_version":2,"rename":[{"from":"b","to":"fast"}],"remove":["junk"]},)"
      R"({"from_version":9,"remove":["fast"]}]})");
  REQUIRE(r.ok());
  const auto& s = r.schema;
  SettingsValues in;
  in["a"] = bval(true);
  in["old"] = bval(true);
  in["junk"] = bval(true);
  in["keep_me"] = ival(1);
  // From version 1: v1 renames a->b, then removes old and b (rename before remove, so b is gone); v2, v3 find nothing.
  SettingsValues out = migrate_values(s, 1, in);
  CHECK(out.count("a") == 0);
  CHECK(out.count("b") == 0);
  CHECK(out.count("old") == 0);
  CHECK(out.count("junk") == 0);
  CHECK(out.count("keep_me") == 1);
  CHECK(out.count("fast") == 0);
  // Ordering: a stored value under "b" at version 2 skips the version 1 entry, v2 renames b->fast, v3 does nothing.
  SettingsValues b_in;
  b_in["b"] = bval(true);
  b_in["junk"] = bval(true);
  out = migrate_values(s, 2, b_in);
  CHECK(out.count("b") == 0);
  CHECK(out.at("fast") == bval(true));
  CHECK(out.count("junk") == 0);
  // Version 3 only applies the version 3 entry.
  out = migrate_values(s, 3, b_in);
  CHECK(out.count("c") == 1);
  CHECK(out.count("junk") == 1);
  // Stored version equal to settings_version: untouched. Above: untouched. The from_version 9 entry never applies.
  CHECK(migrate_values(s, 4, b_in) == b_in);
  CHECK(migrate_values(s, 5, b_in) == b_in);
  CHECK(migrate_values(s, 4000000000u, b_in) == b_in);
  // Version 0 (below everything) applies every applicable entry in ascending order.
  SettingsValues chain;
  chain["a"] = bval(true);
  chain["zz"] = fval(0.25);
  out = migrate_values(s, 0, chain);
  CHECK(out.count("a") == 0);  // a -> b -> removed
  CHECK(std::fabs(out.at("density").f - 0.25) < 1e-12);
  // Document order of entries does not matter: ascending from_version is used.
  SettingsValues ab;
  ab["a"] = bval(true);
  CHECK(migrate_values(s, 1, ab).count("fast") == 0);
  // An existing value under the new id wins over a renamed one.
  const auto r2 = parse_settings_schema(doc(kBool, R"(,"migrations":[{"from_version":1,"rename":[{"from":"old","to":"fast"}]}])"));
  REQUIRE(r2.ok());
  SettingsValues clash;
  clash["old"] = bval(true);
  clash["fast"] = bval(false);
  out = migrate_values(r2.schema, 1, clash);
  CHECK(out.at("fast") == bval(false));
  CHECK(out.count("old") == 0);
  // Chained renames in one entry apply in listed order.
  const auto r3 = parse_settings_schema(doc(kBool, R"(,"migrations":[{"from_version":1,"rename":[{"from":"p","to":"q"},{"from":"q","to":"fast"}]}])"));
  REQUIRE(r3.ok());
  SettingsValues pq;
  pq["p"] = bval(true);
  CHECK(migrate_values(r3.schema, 1, pq).at("fast") == bval(true));
}

TEST_CASE("reset_values honours keep and scope", "[settings][reset]") {
  const auto r = parse_settings_schema(doc(
      R"({"id":"scene_a","label":"x","type":"int","min":0,"max":9,"default":1},
         {"id":"scene_keep","label":"x","type":"int","min":0,"max":9,"default":1,"reset":"keep"},
         {"id":"dev_a","label":"x","type":"int","min":0,"max":9,"default":1,"scope":"device"},
         {"id":"dev_keep","label":"x","type":"int","min":0,"max":9,"default":1,"scope":"device","reset":"keep"})"));
  REQUIRE(r.ok());
  SettingsValues cur;
  for (const char* id : {"scene_a", "scene_keep", "dev_a", "dev_keep", "unknown"}) cur[id] = ival(7);
  SettingsValues out = reset_values(r.schema, cur);  // scene scope only (default)
  CHECK(out.at("scene_a") == ival(1));
  CHECK(out.at("scene_keep") == ival(7));
  CHECK(out.at("dev_a") == ival(7));
  CHECK(out.at("dev_keep") == ival(7));
  CHECK(out.at("unknown") == ival(7));
  out = reset_values(r.schema, cur, false);  // every scope
  CHECK(out.at("scene_a") == ival(1));
  CHECK(out.at("scene_keep") == ival(7));
  CHECK(out.at("dev_a") == ival(1));
  CHECK(out.at("dev_keep") == ival(7));
  // Settings missing from the current map get their default unless they are kept.
  out = reset_values(r.schema, {});
  CHECK(out.at("scene_a") == ival(1));
  CHECK(out.count("scene_keep") == 0);
  CHECK(out.count("dev_a") == 0);
}

TEST_CASE("hostile input fails cleanly", "[settings][hostile]") {
  // Deep nesting.
  CHECK(err_of(std::string(100000, '[') + std::string(100000, ']')) == SettingsError::NotJson);
  CHECK(err_of("{\"settings_version\":1,\"settings\":" + std::string(5000, '[') + std::string(5000, ']') + "}") ==
        SettingsError::NotJson);
  CHECK(err_of("{\"a\":" + std::string(5000, '{') + "}") == SettingsError::NotJson);
  // Huge arrays: 200000 items are above the JSON pre-scan item limit (65536), so they are rejected as NotJson
  // before any parsing.
  {
    std::string s = "{\"settings_version\":1,\"settings\":[";
    for (int i = 0; i < 200000; ++i) s += (i ? ",1" : "1");
    s += "]}";
    CHECK(err_of(s) == SettingsError::NotJson);
  }
  {
    std::string s = R"({"settings_version":1,"settings":[{"id":"e","label":"x","type":"enum","default":"a","options":[)";
    for (int i = 0; i < 200000; ++i) s += (i ? ",1" : "1");
    s += "]}]}";
    CHECK(err_of(s) == SettingsError::NotJson);
  }
  // A huge colour array is above the pre-scan item limit, a huge migrations list parses.
  {
    std::string s = R"({"id":"c","label":"x","type":"color","default":[)";
    for (int i = 0; i < 100000; ++i) s += (i ? ",0" : "0");
    s += "]}";
    CHECK(err_of(doc(s)) == SettingsError::NotJson);  // above the pre-scan item limit
  }
  // 10 MB of whitespace around a valid document, and 10 MB of whitespace alone.
  {
    const std::string ws(10 * 1024 * 1024, ' ');
    const auto padded = parse_settings_schema(ws + doc(kBool) + ws);
    CHECK((padded.ok() || padded.error == SettingsError::NotJson));
    CHECK(err_of(ws) == SettingsError::NotJson);
  }
  // Duplicate keys are rejected by the JSON layer.
  CHECK(err_of(R"({"settings_version":1,"settings_version":2,"settings":[]})") == SettingsError::NotJson);
  CHECK(err_of(doc(R"({"id":"a","id":"b","label":"x","type":"bool","default":true})")) == SettingsError::NotJson);
  // Binary junk and truncation at every offset of a valid document.
  const std::string good = doc(std::string(kEnum) + "," + kColor,
                               R"(,"migrations":[{"from_version":1,"rename":[{"from":"a","to":"b"}]}])");
  CHECK(parse_settings_schema(good).ok());
  for (std::size_t cut = 0; cut < good.size(); ++cut) CHECK_FALSE(parse_settings_schema(good.substr(0, cut)).ok());
  CHECK_FALSE(parse_settings_schema(std::string("\0\xff\xfe{", 4)).ok());
  for (std::size_t i = 0; i < good.size(); ++i) {
    std::string m = good;
    m[i] = static_cast<char>(m[i] ^ 0x5A);
    (void)parse_settings_schema(m);  // any outcome, no crash
  }
}

TEST_CASE("the aurora sample's settings load packed and parse", "[settings][sample]") {
  const auto packed = lumora::scene::pack_directory(LUMORA_SAMPLES_DIR "/aurora");
  REQUIRE(packed.ok);
  const auto loaded = load_package(packed.files);
  REQUIRE(loaded.ok());
  CHECK(loaded.manifest.settings_path == "settings.json");
  const auto it = packed.files.find(loaded.manifest.settings_path);
  REQUIRE(it != packed.files.end());
  const auto r = parse_settings_schema(std::string(it->second.begin(), it->second.end()));
  REQUIRE(r.ok());
  REQUIRE(r.schema.settings.size() == 3);
  CHECK(r.schema.settings[0].type == SettingType::Float);  // parallax_strength, read by the sample script
  CHECK(r.schema.settings[1].type == SettingType::Float);
  CHECK(r.schema.settings[2].type == SettingType::Color);
  const SettingsValues v = resolve_values(r.schema, {});
  CHECK(v.at("parallax_strength") == fval(1.0));
  CHECK(v.at("glow") == fval(0.5));
  CHECK(v.at("tint") == cval(0.2f, 0.6f, 0.5f, 1.0f));
}
