// Per-scene capability declarations and grants. See capabilities.h for the contract and docs/capabilities.md.
#include "capabilities.h"

namespace lumora::package {

namespace {

constexpr unsigned kCount = static_cast<unsigned>(Capability::Count_);
constexpr CapabilitySet kValidBits = (CapabilitySet{1} << kCount) - 1;

// The four capabilities in enum order; the name table is indexed the same way.
const char* const kNames[kCount] = {"microphone", "notifications", "activity_recognition", "coarse_location"};

bool valid(Capability c) { return static_cast<unsigned>(c) < kCount; }

// Callers check valid(c) first, so the shift is always below 32.
CapabilitySet bit_of(Capability c) { return CapabilitySet{1} << static_cast<unsigned>(c); }

}  // namespace

const char* capability_name(Capability c) {
  if (!valid(c)) return "";
  return kNames[static_cast<unsigned>(c)];
}

bool capability_from_name(std::string_view name, Capability* out) {
  for (unsigned i = 0; i < kCount; ++i) {
    if (name == std::string_view(kNames[i])) {
      if (out != nullptr) *out = static_cast<Capability>(i);
      return true;
    }
  }
  return false;
}

DeclareResult parse_declared(const std::vector<std::string>& names, std::size_t max_names) {
  DeclareResult r;
  if (names.size() > max_names) {
    r.verdict = DeclareVerdict::TooManyNames;
    r.index = max_names;
    return r;
  }
  CapabilitySet set = 0;
  for (std::size_t i = 0; i < names.size(); ++i) {
    Capability c = Capability::Count_;
    if (!capability_from_name(names[i], &c)) {
      r.verdict = DeclareVerdict::UnknownCapability;
      r.index = i;
      return r;
    }
    if ((set & bit_of(c)) != 0) {
      r.verdict = DeclareVerdict::DuplicateCapability;
      r.index = i;
      return r;
    }
    set |= bit_of(c);
  }
  r.declared = set;
  return r;
}

bool grant(GrantRecord* r, Capability c) {
  if (r == nullptr) return false;
  if (!valid(c)) return false;
  if ((r->declared & bit_of(c)) == 0) return false;
  r->granted |= bit_of(c);
  return true;
}

bool revoke(GrantRecord* r, Capability c) {
  if (r == nullptr) return false;
  if (!valid(c)) return true;
  r->granted &= ~bit_of(c);
  return true;
}

GrantRecord reconcile_update(const GrantRecord& old_record, CapabilitySet new_declared) {
  const CapabilitySet old_declared = old_record.declared & kValidBits;
  const CapabilitySet old_granted = old_record.granted & kValidBits & old_declared;
  const CapabilitySet next = new_declared & kValidBits;
  GrantRecord out;
  out.declared = next;
  if ((next & ~old_declared) != 0) {
    out.granted = 0;
  } else {
    out.granted = old_granted & next;
  }
  return out;
}

CapabilityStatus query(const GrantRecord& r, Capability c, const PlatformState& platform) {
  if (!valid(c)) return CapabilityStatus::NotDeclared;
  const CapabilitySet b = bit_of(c);
  if ((r.declared & b) == 0) return CapabilityStatus::NotDeclared;
  if ((r.granted & b) == 0) return CapabilityStatus::PermissionDenied;
  if ((platform.supported & b) == 0) return CapabilityStatus::Unsupported;
  if ((platform.active & b) == 0) return CapabilityStatus::TemporarilyUnavailable;
  return CapabilityStatus::Available;
}

const char* to_string(DeclareVerdict v) {
  switch (v) {
    case DeclareVerdict::Ok: return "Ok";
    case DeclareVerdict::UnknownCapability: return "UnknownCapability";
    case DeclareVerdict::DuplicateCapability: return "DuplicateCapability";
    case DeclareVerdict::TooManyNames: return "TooManyNames";
  }
  return "Unknown";
}

const char* to_string(CapabilityStatus s) {
  switch (s) {
    case CapabilityStatus::Available: return "Available";
    case CapabilityStatus::NotDeclared: return "NotDeclared";
    case CapabilityStatus::PermissionDenied: return "PermissionDenied";
    case CapabilityStatus::Unsupported: return "Unsupported";
    case CapabilityStatus::TemporarilyUnavailable: return "TemporarilyUnavailable";
  }
  return "Unknown";
}

}  // namespace lumora::package
