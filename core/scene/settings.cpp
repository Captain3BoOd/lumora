// settings.json parser and validator. Rules are in settings.h; choices the header leaves open are in
// docs/settings.md.
#include "settings.h"
#include "../package/json_limits.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "json_value.h"

namespace lumora::scene {
namespace {

constexpr std::size_t kMaxSettings = 256;
constexpr std::size_t kMaxOptions = 64;
constexpr std::size_t kMaxLabelBytes = 128;
constexpr double kMaxMagnitude = 1e9;

SettingsParseResult fail(SettingsError e, std::string msg) {
  SettingsParseResult r;
  r.error = e;
  r.message = std::move(msg);
  return r;
}

bool valid_id(const std::string& s) {
  if (s.empty() || s.size() > 64) return false;
  if (s[0] < 'a' || s[0] > 'z') return false;
  for (char c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok) return false;
  }
  return true;
}

bool whole(double d) { return std::isfinite(d) && d == std::floor(d); }

const char* type_name(SettingType t) {
  switch (t) {
    case SettingType::Bool: return "bool";
    case SettingType::Int: return "int";
    case SettingType::Float: return "float";
    case SettingType::Enum: return "enum";
    case SettingType::Color: return "color";
  }
  return "?";
}

const char* check_name(ValueCheck c) {
  switch (c) {
    case ValueCheck::Ok: return "ok";
    case ValueCheck::UnknownSetting: return "unknown setting";
    case ValueCheck::WrongType: return "wrong type";
    case ValueCheck::OutOfRange: return "out of range";
    case ValueCheck::NotOnStep: return "not on the step grid";
    case ValueCheck::UnknownOption: return "unknown option";
    case ValueCheck::BadColor: return "bad color";
  }
  return "?";
}

const SettingDef* find_def(const SettingsSchema& schema, const std::string& id) {
  for (const SettingDef& d : schema.settings)
    if (d.id == id) return &d;
  return nullptr;
}

// Typed access to a field. `required` reports MissingField; a present field of the wrong JSON type is WrongType.
struct Ctx {
  std::string where;  // "setting 'density'"
  SettingsError err = SettingsError::None;
  std::string msg;

  bool bad(SettingsError e, const std::string& field, const char* what) {
    err = e;
    msg = where + ": field '" + field + "' " + what;
    return false;
  }
};

bool read_string(Ctx& c, const JsonValue& obj, const char* key, bool required, std::string& out, bool& present) {
  const JsonValue* v = obj.find(key);
  present = v != nullptr;
  if (!v) return required ? c.bad(SettingsError::MissingField, key, "is missing") : true;
  if (!v->is_string()) return c.bad(SettingsError::WrongType, key, "must be a string");
  out = v->as_string();
  return true;
}

bool read_number(Ctx& c, const JsonValue& obj, const char* key, bool required, double& out, bool& present) {
  const JsonValue* v = obj.find(key);
  present = v != nullptr;
  if (!v) return required ? c.bad(SettingsError::MissingField, key, "is missing") : true;
  if (!v->is_number()) return c.bad(SettingsError::WrongType, key, "must be a number");
  out = v->as_number();
  return true;
}

// Converts a JSON value to a SettingValue of `def`'s type. Range and grid are checked by validate_value later.
bool json_to_value(Ctx& c, const SettingDef& def, const JsonValue& v, const char* field, SettingValue& out) {
  out = SettingValue{};
  out.type = def.type;
  switch (def.type) {
    case SettingType::Bool:
      if (!v.is_bool()) return c.bad(SettingsError::WrongType, field, "must be a boolean");
      out.b = v.as_bool();
      return true;
    case SettingType::Int:
      if (!v.is_number()) return c.bad(SettingsError::WrongType, field, "must be a number");
      if (!whole(v.as_number()) || std::fabs(v.as_number()) > kMaxMagnitude)
        return c.bad(SettingsError::BadValue, field, "must be a whole number within 1e9 for an int setting");
      out.i = static_cast<std::int64_t>(v.as_number());
      return true;
    case SettingType::Float:
      if (!v.is_number()) return c.bad(SettingsError::WrongType, field, "must be a number");
      if (!std::isfinite(v.as_number())) return c.bad(SettingsError::BadValue, field, "must be finite");
      out.f = v.as_number();
      return true;
    case SettingType::Enum:
      if (!v.is_string()) return c.bad(SettingsError::WrongType, field, "must be a string");
      out.e = v.as_string();
      return true;
    case SettingType::Color: {
      if (!v.is_array()) return c.bad(SettingsError::WrongType, field, "must be an array of 4 numbers");
      const auto& a = v.as_array();
      if (a.size() != 4) return c.bad(SettingsError::BadValue, field, "must have exactly 4 components");
      for (std::size_t i = 0; i < 4; ++i) {
        if (!a[i].is_number()) return c.bad(SettingsError::WrongType, field, "must contain only numbers");
        out.color[i] = static_cast<float>(a[i].as_number());
      }
      return true;
    }
  }
  return false;
}

// Reads min, max, optional step for int and float settings.
bool read_numeric_limits(Ctx& c, const JsonValue& obj, SettingDef& def) {
  bool present = false;
  if (!read_number(c, obj, "min", true, def.min, present)) return false;
  if (!read_number(c, obj, "max", true, def.max, present)) return false;
  if (!std::isfinite(def.min) || !std::isfinite(def.max) || std::fabs(def.min) > kMaxMagnitude ||
      std::fabs(def.max) > kMaxMagnitude)
    return c.bad(SettingsError::BadValue, "min/max", "must be finite with magnitude at most 1e9");
  if (!(def.min < def.max)) return c.bad(SettingsError::BadValue, "min/max", "need min < max");
  if (!read_number(c, obj, "step", false, def.step, present)) return false;
  if (present && (!std::isfinite(def.step) || !(def.step > 0) || def.step > 2 * kMaxMagnitude))
    return c.bad(SettingsError::BadValue, "step", "must be a positive finite number");
  if (!present) def.step = 0;
  if (def.type == SettingType::Int && (!whole(def.min) || !whole(def.max) || !whole(def.step)))
    return c.bad(SettingsError::BadValue, "min/max/step", "must be whole numbers for an int setting");
  return true;
}

bool read_enum_options(Ctx& c, const JsonValue& obj, SettingDef& def, SettingsError& too_many) {
  const JsonValue* opts = obj.find("options");
  if (!opts) return c.bad(SettingsError::MissingField, "options", "is missing");
  if (!opts->is_array()) return c.bad(SettingsError::WrongType, "options", "must be an array");
  const auto& arr = opts->as_array();
  if (arr.size() > kMaxOptions) {
    too_many = SettingsError::TooMany;
    return c.bad(SettingsError::TooMany, "options", "has more than 64 entries");
  }
  if (arr.empty()) return c.bad(SettingsError::BadValue, "options", "needs at least 1 option");
  std::set<std::string> seen;
  for (const JsonValue& o : arr) {
    if (!o.is_object()) return c.bad(SettingsError::WrongType, "options", "entries must be objects");
    EnumOption opt;
    bool present = false;
    if (!read_string(c, o, "id", true, opt.id, present)) return false;
    if (!valid_id(opt.id)) return c.bad(SettingsError::BadValue, "options.id", "is not a valid id");
    if (!read_string(c, o, "label", true, opt.label, present)) return false;
    if (opt.label.empty() || opt.label.size() > kMaxLabelBytes)
      return c.bad(SettingsError::BadValue, "options.label", "must be 1 to 128 bytes");
    if (!seen.insert(opt.id).second) return c.bad(SettingsError::DuplicateId, "options.id", "is duplicated");
    def.options.push_back(std::move(opt));
  }
  return true;
}

SettingsParseResult parse_one_setting(const JsonValue& obj, const SettingsSchema& so_far, std::size_t index,
                                      SettingDef& def) {
  Ctx c;
  c.where = "settings[" + std::to_string(index) + "]";
  if (!obj.is_object()) return fail(SettingsError::WrongType, c.where + " must be an object");
  bool present = false;
  if (!read_string(c, obj, "id", true, def.id, present)) return fail(c.err, c.msg);
  if (!valid_id(def.id)) return fail(SettingsError::BadValue, c.where + ": id '" + def.id + "' is not a valid id");
  c.where = "setting '" + def.id + "'";
  if (find_def(so_far, def.id)) return fail(SettingsError::DuplicateId, c.where + ": id is duplicated");
  if (!read_string(c, obj, "label", true, def.label, present)) return fail(c.err, c.msg);
  if (def.label.empty() || def.label.size() > kMaxLabelBytes)
    return fail(SettingsError::BadValue, c.where + ": field 'label' must be 1 to 128 bytes");
  std::string type;
  if (!read_string(c, obj, "type", true, type, present)) return fail(c.err, c.msg);
  if (type == "bool") def.type = SettingType::Bool;
  else if (type == "int") def.type = SettingType::Int;
  else if (type == "float") def.type = SettingType::Float;
  else if (type == "enum") def.type = SettingType::Enum;
  else if (type == "color") def.type = SettingType::Color;
  else return fail(SettingsError::BadValue, c.where + ": field 'type' is not one of bool, int, float, enum, color");

  if (def.type == SettingType::Int || def.type == SettingType::Float) {
    if (!read_numeric_limits(c, obj, def)) return fail(c.err, c.msg);
  }
  if (def.type == SettingType::Enum) {
    SettingsError unused = SettingsError::None;
    if (!read_enum_options(c, obj, def, unused)) return fail(c.err, c.msg);
  }
  if (def.type == SettingType::Color) {
    std::string space;
    if (!read_string(c, obj, "color_space", false, space, present)) return fail(c.err, c.msg);
    if (present) {
      if (space != "srgb" && space != "linear")
        return fail(SettingsError::BadValue, c.where + ": field 'color_space' must be srgb or linear");
      def.color_space = space;
    }
  }
  const JsonValue* dv = obj.find("default");
  if (!dv) return fail(SettingsError::MissingField, c.where + ": field 'default' is missing");
  if (!json_to_value(c, def, *dv, "default", def.default_value)) return fail(c.err, c.msg);
  {
    // The default must be valid for the definition. A default that is inside the range but off the step grid
    // is kept as written (see docs/settings.md); everything else must pass.
    SettingValue probe = def.default_value;
    const ValueCheck chk = validate_value(def, probe);
    if (chk != ValueCheck::Ok)
      return fail(SettingsError::BadValue, c.where + ": field 'default' is invalid: " + check_name(chk));
  }
  std::string text;
  if (!read_string(c, obj, "scope", false, text, present)) return fail(c.err, c.msg);
  if (present) {
    if (text == "scene") def.scope = SettingScope::Scene;
    else if (text == "device") def.scope = SettingScope::Device;
    else return fail(SettingsError::BadValue, c.where + ": field 'scope' must be scene or device");
  }
  if (!read_string(c, obj, "reset", false, text, present)) return fail(c.err, c.msg);
  if (present) {
    if (text == "default") def.reset = ResetBehavior::Default;
    else if (text == "keep") def.reset = ResetBehavior::Keep;
    else return fail(SettingsError::BadValue, c.where + ": field 'reset' must be default or keep");
  }
  if (const JsonValue* vi = obj.find("visible_if")) {
    if (!vi->is_object()) return fail(SettingsError::WrongType, c.where + ": field 'visible_if' must be an object");
    std::string target;
    if (!read_string(c, *vi, "setting", true, target, present))
      return fail(c.err, c.where + ": visible_if" + c.msg.substr(c.where.size()));
    const SettingDef* ref = find_def(so_far, target);
    if (!ref)
      return fail(SettingsError::BadReference,
                  c.where + ": visible_if refers to '" + target + "', which is not an earlier setting");
    const JsonValue* eq = vi->find("equals");
    const JsonValue* ne = vi->find("not_equals");
    if (!eq && !ne) return fail(SettingsError::MissingField, c.where + ": visible_if needs equals or not_equals");
    if (eq && ne) return fail(SettingsError::BadValue, c.where + ": visible_if has both equals and not_equals");
    SettingValue val;
    if (!json_to_value(c, *ref, eq ? *eq : *ne, "visible_if value", val)) return fail(c.err, c.msg);
    SettingValue probe = val;
    const ValueCheck chk = validate_value(*ref, probe);
    if (chk != ValueCheck::Ok)
      return fail(SettingsError::BadValue,
                  c.where + ": visible_if value is not valid for '" + target + "': " + check_name(chk));
    def.visible_if.present = true;
    def.visible_if.setting = target;
    def.visible_if.negate = ne != nullptr;
    def.visible_if.value = probe;
  }
  return {};
}

SettingsParseResult parse_migrations(const JsonValue& arr_v, SettingsSchema& schema) {
  if (!arr_v.is_array()) return fail(SettingsError::WrongType, "field 'migrations' must be an array");
  std::set<std::uint32_t> seen;
  std::size_t index = 0;
  for (const JsonValue& m : arr_v.as_array()) {
    Ctx c;
    c.where = "migrations[" + std::to_string(index++) + "]";
    if (!m.is_object()) return fail(SettingsError::WrongType, c.where + " must be an object");
    double from = 0;
    bool present = false;
    if (!read_number(c, m, "from_version", true, from, present)) return fail(c.err, c.msg);
    if (!whole(from) || from < 1 || from >= 2147483648.0)
      return fail(SettingsError::BadValue, c.where + ": field 'from_version' must be a whole number of at least 1");
    Migration mig;
    mig.from_version = static_cast<std::uint32_t>(from);
    if (!seen.insert(mig.from_version).second)
      return fail(SettingsError::DuplicateId, c.where + ": from_version " + std::to_string(mig.from_version) + " is duplicated");
    if (const JsonValue* ren = m.find("rename")) {
      if (!ren->is_array()) return fail(SettingsError::WrongType, c.where + ": field 'rename' must be an array");
      for (const JsonValue& pr : ren->as_array()) {
        if (!pr.is_object()) return fail(SettingsError::WrongType, c.where + ": rename entries must be objects");
        std::string a, b;
        if (!read_string(c, pr, "from", true, a, present) || !read_string(c, pr, "to", true, b, present))
          return fail(c.err, c.msg);
        if (!valid_id(a) || !valid_id(b))
          return fail(SettingsError::BadValue, c.where + ": rename ids must be valid ids");
        mig.rename.emplace_back(std::move(a), std::move(b));
      }
    }
    if (const JsonValue* rem = m.find("remove")) {
      if (!rem->is_array()) return fail(SettingsError::WrongType, c.where + ": field 'remove' must be an array");
      for (const JsonValue& s : rem->as_array()) {
        if (!s.is_string()) return fail(SettingsError::WrongType, c.where + ": remove entries must be strings");
        if (!valid_id(s.as_string())) return fail(SettingsError::BadValue, c.where + ": remove ids must be valid ids");
        mig.remove.push_back(s.as_string());
      }
    }
    schema.migrations.push_back(std::move(mig));
  }
  return {};
}

}  // namespace

SettingsParseResult parse_settings_schema(const std::string& json_text) {
  {
    const auto scan = lumora::package::scan_json(json_text);
    if (scan.verdict != lumora::package::JsonVerdict::Ok)
      return fail(SettingsError::NotJson,
                  std::string("rejected by the JSON limits: ") + lumora::package::to_string(scan.verdict));
  }
  JsonParseResult parsed = parse_json(json_text);
  if (!parsed.ok) return fail(SettingsError::NotJson, "not valid JSON: " + parsed.error);
  const JsonValue& root = parsed.value;
  if (!root.is_object()) return fail(SettingsError::WrongType, "the document must be a JSON object");
  SettingsParseResult result;
  Ctx c;
  c.where = "document";
  double version = 0;
  bool present = false;
  if (!read_number(c, root, "settings_version", true, version, present)) return fail(c.err, c.msg);
  if (!whole(version) || version < 1 || version >= 2147483648.0)
    return fail(SettingsError::BadValue, "field 'settings_version' must be a whole number of at least 1");
  result.schema.settings_version = static_cast<std::uint32_t>(version);
  const JsonValue* list = root.find("settings");
  if (!list) return fail(SettingsError::MissingField, "field 'settings' is missing");
  if (!list->is_array()) return fail(SettingsError::WrongType, "field 'settings' must be an array");
  if (list->as_array().size() > kMaxSettings) return fail(SettingsError::TooMany, "more than 256 settings");
  std::size_t index = 0;
  for (const JsonValue& item : list->as_array()) {
    SettingDef def;
    SettingsParseResult r = parse_one_setting(item, result.schema, index++, def);
    if (!r.ok()) return r;
    result.schema.settings.push_back(std::move(def));
  }
  if (const JsonValue* mig = root.find("migrations")) {
    SettingsParseResult r = parse_migrations(*mig, result.schema);
    if (!r.ok()) return r;
  }
  return result;
}

ValueCheck validate_value(const SettingDef& def, SettingValue& value) {
  if (def.id.empty()) return ValueCheck::UnknownSetting;
  if (value.type != def.type) return ValueCheck::WrongType;
  switch (def.type) {
    case SettingType::Bool:
      return ValueCheck::Ok;
    case SettingType::Int: {
      const double v = static_cast<double>(value.i);
      if (v < def.min || v > def.max) return ValueCheck::OutOfRange;
      if (def.step > 0) {
        const std::int64_t lo = static_cast<std::int64_t>(def.min);
        const std::int64_t step = static_cast<std::int64_t>(def.step);
        const std::int64_t k = (2 * (value.i - lo) + step) / (2 * step);  // nearest, ties up
        const std::int64_t snapped = lo + k * step;
        if (static_cast<double>(snapped) > def.max) return ValueCheck::NotOnStep;
        value.i = snapped;
      }
      return ValueCheck::Ok;
    }
    case SettingType::Float: {
      if (!std::isfinite(value.f)) return ValueCheck::OutOfRange;
      if (value.f < def.min || value.f > def.max) return ValueCheck::OutOfRange;
      if (def.step > 0) {
        const double k = std::floor((value.f - def.min) / def.step + 0.5);  // nearest, ties up
        double snapped = def.min + k * def.step;
        if (snapped > def.max) {
          if (snapped - def.max > def.step * 1e-9) return ValueCheck::NotOnStep;
          snapped = def.max;
        }
        value.f = snapped;
      }
      return ValueCheck::Ok;
    }
    case SettingType::Enum:
      for (const EnumOption& o : def.options)
        if (o.id == value.e) return ValueCheck::Ok;
      return ValueCheck::UnknownOption;
    case SettingType::Color:
      for (float comp : value.color)
        if (!(comp >= 0.0f && comp <= 1.0f)) return ValueCheck::BadColor;
      return ValueCheck::Ok;
  }
  return ValueCheck::WrongType;
}

SettingsValues resolve_values(const SettingsSchema& schema, const SettingsValues& stored,
                              std::vector<std::string>* problems) {
  SettingsValues out;
  for (const SettingDef& def : schema.settings) {
    const auto it = stored.find(def.id);
    if (it != stored.end()) {
      SettingValue v = it->second;
      const ValueCheck chk = validate_value(def, v);
      if (chk == ValueCheck::Ok) {
        out[def.id] = v;
        continue;
      }
      if (problems) problems->push_back(def.id + ": " + check_name(chk) + " (" + type_name(def.type) + "), default used");
    }
    out[def.id] = def.default_value;
  }
  if (problems) {
    for (const auto& kv : stored)
      if (!find_def(schema, kv.first)) problems->push_back(kv.first + ": " + check_name(ValueCheck::UnknownSetting) + ", dropped");
  }
  return out;
}

SettingsValues migrate_values(const SettingsSchema& schema, std::uint32_t stored_version,
                              const SettingsValues& stored) {
  SettingsValues out = stored;
  if (stored_version >= schema.settings_version) return out;
  std::vector<const Migration*> order;
  for (const Migration& m : schema.migrations)
    if (m.from_version < schema.settings_version && stored_version <= m.from_version) order.push_back(&m);
  std::stable_sort(order.begin(), order.end(),
                   [](const Migration* a, const Migration* b) { return a->from_version < b->from_version; });
  for (const Migration* m : order) {
    for (const auto& pr : m->rename) {
      const auto it = out.find(pr.first);
      if (it == out.end() || pr.first == pr.second) continue;
      SettingValue v = it->second;
      out.erase(it);
      out.emplace(pr.second, v);  // an existing value under the new id wins
    }
    for (const std::string& id : m->remove) out.erase(id);
  }
  return out;
}

bool is_visible(const SettingsSchema& schema, const SettingDef& def, const SettingsValues& values) {
  // Walk up the chain of visible_if references (always to earlier settings, so it ends). A setting is shown
  // only when its own rule holds and every setting it depends on is shown too.
  const SettingDef* cur = &def;
  for (std::size_t steps = 0; steps <= schema.settings.size() && cur; ++steps) {
    if (!cur->visible_if.present) return true;
    const SettingDef* ref = find_def(schema, cur->visible_if.setting);
    if (!ref) return false;
    const auto it = values.find(ref->id);
    const SettingValue& actual = it != values.end() ? it->second : ref->default_value;
    const bool equal = actual == cur->visible_if.value;
    if (equal == cur->visible_if.negate) return false;
    cur = ref;
  }
  return true;
}

SettingsValues reset_values(const SettingsSchema& schema, const SettingsValues& current, bool only_scene_scope) {
  SettingsValues out = current;
  for (const SettingDef& def : schema.settings) {
    if (def.reset == ResetBehavior::Keep) continue;
    if (only_scene_scope && def.scope != SettingScope::Scene) continue;
    out[def.id] = def.default_value;
  }
  return out;
}

}  // namespace lumora::scene
