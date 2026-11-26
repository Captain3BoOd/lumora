// Tests for core/package/capabilities. Evidence label: unit tests only.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/capabilities_ref.py, an independent
// Python implementation. Regenerate it with: python3 tools/capabilities_ref.py
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "capabilities.h"

using namespace lumora::package;

namespace {

// BEGIN GENERATED REFERENCE (tools/capabilities_ref.py)
const char* const kRefNames[4] = {"microphone", "notifications", "activity_recognition", "coarse_location"};
// Enum values: DeclareVerdict Ok=0, UnknownCapability=1, DuplicateCapability=2, TooManyNames=3; CapabilityStatus Available=0, NotDeclared=1, PermissionDenied=2, Unsupported=3, TemporarilyUnavailable=4.
const std::uint8_t kRefCaps[] = {0, 1, 2, 3, 4, 5, 31, 32, 255};
const std::uint32_t kRefHigh[] = {0x00000000u, 0x00000010u, 0x80000000u, 0xFFFFFFF0u};
const std::size_t kRefMaxNames[] = {0ull, 1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 32ull, static_cast<std::size_t>(-1)};
const std::vector<std::string> kRefPool = {
    std::string("\x6d" "\x69" "\x63" "\x72" "\x6f" "\x70" "\x68" "\x6f" "\x6e" "\x65", 10),
    std::string("\x6e" "\x6f" "\x74" "\x69" "\x66" "\x69" "\x63" "\x61" "\x74" "\x69" "\x6f" "\x6e" "\x73", 13),
    std::string("\x61" "\x63" "\x74" "\x69" "\x76" "\x69" "\x74" "\x79" "\x5f" "\x72" "\x65" "\x63" "\x6f" "\x67" "\x6e" "\x69" "\x74" "\x69" "\x6f" "\x6e", 20),
    std::string("\x63" "\x6f" "\x61" "\x72" "\x73" "\x65" "\x5f" "\x6c" "\x6f" "\x63" "\x61" "\x74" "\x69" "\x6f" "\x6e", 15),
    std::string("\x4d" "\x69" "\x63" "\x72" "\x6f" "\x70" "\x68" "\x6f" "\x6e" "\x65", 10),
    std::string("\x6d" "\x69" "\x63" "\x72" "\x6f" "\x70" "\x68" "\x6f" "\x6e" "\x65" "\x20", 11),
    std::string(),
    std::string("\x63" "\x6f" "\x75" "\x6e" "\x74" "\x5f", 6),
    std::string("\x6d" "\x69" "\x63" "\x72" "\x6f" "\x70" "\x68" "\x6f" "\x6e" "\x65" "\x00" "\x78", 12),
    std::string("\x6d" "\x69" "\x63" "\x72" "\x6f" "\x70" "\x68" "\x6f" "\x6e" "\x65" "\x00", 11),
};
const std::uint64_t kRefDigest_query = 0x30A108C9189F9515ull;
const std::uint64_t kRefDigest_query_wide = 0x249FB7A48986B545ull;
const std::uint64_t kRefDigest_update = 0x71B7004DD0586325ull;
const std::uint64_t kRefDigest_grant_revoke = 0x85A5772FEA384005ull;
const std::uint64_t kRefDigest_parse = 0x475599A1E7CA4F19ull;
const std::size_t kRefParseCases = 999999;
// END GENERATED REFERENCE

// FNV-1a 64, the same byte streams as tools/capabilities_ref.py.
struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void num(std::uint64_t v, int n) {
    for (int i = 0; i < n; ++i) byte(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  }
};

Capability cap(unsigned v) { return static_cast<Capability>(v); }

GrantRecord rec(CapabilitySet declared, CapabilitySet granted) {
  GrantRecord r;
  r.declared = declared;
  r.granted = granted;
  return r;
}

PlatformState plat(CapabilitySet supported, CapabilitySet active) {
  PlatformState p;
  p.supported = supported;
  p.active = active;
  return p;
}

constexpr CapabilitySet kMic = 1, kNot = 2, kAct = 4, kLoc = 8;

}  // namespace

TEST_CASE("capability names and enum values", "[capabilities]") {
  REQUIRE(static_cast<unsigned>(Capability::Microphone) == 0);
  REQUIRE(static_cast<unsigned>(Capability::Notifications) == 1);
  REQUIRE(static_cast<unsigned>(Capability::ActivityRecognition) == 2);
  REQUIRE(static_cast<unsigned>(Capability::CoarseLocation) == 3);
  REQUIRE(static_cast<unsigned>(Capability::Count_) == 4);
  for (unsigned i = 0; i < 4; ++i) {
    INFO("capability " << i);
    REQUIRE(std::string(capability_name(cap(i))) == kRefNames[i]);
    Capability out = Capability::Count_;
    REQUIRE(capability_from_name(kRefNames[i], &out));
    REQUIRE(static_cast<unsigned>(out) == i);
  }
  REQUIRE(std::string(capability_name(Capability::Microphone)) == "microphone");
  REQUIRE(std::string(capability_name(Capability::Notifications)) == "notifications");
  REQUIRE(std::string(capability_name(Capability::ActivityRecognition)) == "activity_recognition");
  REQUIRE(std::string(capability_name(Capability::CoarseLocation)) == "coarse_location");
  for (unsigned v : {4u, 5u, 31u, 32u, 255u}) {
    REQUIRE(capability_name(cap(v)) != nullptr);
    REQUIRE(std::string(capability_name(cap(v))).empty());
  }
}

TEST_CASE("capability_from_name is exact", "[capabilities]") {
  Capability out = Capability::CoarseLocation;
  for (const std::string& bad : kRefPool) {
    if (bad == "microphone" || bad == "notifications" || bad == "activity_recognition" ||
        bad == "coarse_location") {
      continue;
    }
    INFO("name of " << bad.size() << " bytes");
    out = Capability::CoarseLocation;
    REQUIRE_FALSE(capability_from_name(bad, &out));
    REQUIRE(out == Capability::CoarseLocation);  // untouched on false
  }
  for (const char* bad : {"MICROPHONE", " microphone", "micro", "microphones", "notification", "activity-recognition",
                          "activity_recognition ", "coarse_location\n", "coarse location", "Coarse_location"}) {
    REQUIRE_FALSE(capability_from_name(bad, &out));
  }
  REQUIRE_FALSE(capability_from_name(std::string_view("microphone\0", 11), &out));
  REQUIRE_FALSE(capability_from_name(std::string_view("microphone\0x", 12), &out));
  REQUIRE_FALSE(capability_from_name(std::string_view(), &out));
  SECTION("a null out still reports the match") {
    REQUIRE(capability_from_name("microphone", nullptr));
    REQUIRE_FALSE(capability_from_name("Microphone", nullptr));
  }
}

TEST_CASE("parse_declared by hand", "[capabilities]") {
  SECTION("empty list") {
    const DeclareResult r = parse_declared({});
    REQUIRE(r.verdict == DeclareVerdict::Ok);
    REQUIRE(r.declared == 0);
    REQUIRE(r.index == 0);
  }
  SECTION("each name and all four") {
    REQUIRE(parse_declared({"microphone"}).declared == kMic);
    REQUIRE(parse_declared({"notifications"}).declared == kNot);
    REQUIRE(parse_declared({"activity_recognition"}).declared == kAct);
    REQUIRE(parse_declared({"coarse_location"}).declared == kLoc);
    const DeclareResult all = parse_declared({"coarse_location", "microphone", "activity_recognition", "notifications"});
    REQUIRE(all.verdict == DeclareVerdict::Ok);
    REQUIRE(all.declared == 15);
  }
  SECTION("unknown and duplicate report the name index, declared is 0") {
    DeclareResult r = parse_declared({"microphone", "Microphone"});
    REQUIRE(r.verdict == DeclareVerdict::UnknownCapability);
    REQUIRE(r.index == 1);
    REQUIRE(r.declared == 0);
    r = parse_declared({"microphone", "notifications", "microphone"});
    REQUIRE(r.verdict == DeclareVerdict::DuplicateCapability);
    REQUIRE(r.index == 2);
    REQUIRE(r.declared == 0);
  }
  SECTION("unknown before duplicate for the same name, first failure wins") {
    DeclareResult r = parse_declared({"microphone", "bogus", "microphone"});
    REQUIRE(r.verdict == DeclareVerdict::UnknownCapability);
    REQUIRE(r.index == 1);
    r = parse_declared({"microphone", "microphone", "bogus"});
    REQUIRE(r.verdict == DeclareVerdict::DuplicateCapability);
    REQUIRE(r.index == 1);
  }
  SECTION("too many names wins over an earlier unknown name") {
    DeclareResult r = parse_declared({"bogus", "microphone", "notifications"}, 2);
    REQUIRE(r.verdict == DeclareVerdict::TooManyNames);
    REQUIRE(r.index == 2);
    REQUIRE(r.declared == 0);
    r = parse_declared({"microphone", "notifications"}, 2);
    REQUIRE(r.verdict == DeclareVerdict::Ok);
    r = parse_declared({"microphone"}, 0);
    REQUIRE(r.verdict == DeclareVerdict::TooManyNames);
    REQUIRE(r.index == 0);
    REQUIRE(parse_declared({}, 0).verdict == DeclareVerdict::Ok);
  }
  SECTION("default limit is 32 names") {
    std::vector<std::string> names(32, "microphone");
    REQUIRE(parse_declared(names).verdict == DeclareVerdict::DuplicateCapability);
    names.push_back("microphone");
    const DeclareResult r = parse_declared(names);
    REQUIRE(r.verdict == DeclareVerdict::TooManyNames);
    REQUIRE(r.index == 32);
  }
  SECTION("more than four names is decided by unknown and duplicate when the limit allows it") {
    const DeclareResult r = parse_declared({"microphone", "notifications", "activity_recognition", "coarse_location", "microphone"}, 5);
    REQUIRE(r.verdict == DeclareVerdict::DuplicateCapability);
    REQUIRE(r.index == 4);
    REQUIRE(parse_declared({"microphone", "notifications", "activity_recognition", "coarse_location", "x"},
                           static_cast<std::size_t>(-1)).verdict == DeclareVerdict::UnknownCapability);
  }
}

TEST_CASE("grant and revoke by hand", "[capabilities]") {
  SECTION("grant needs a declared capability") {
    GrantRecord r = rec(kMic | kLoc, 0);
    REQUIRE(grant(&r, Capability::Microphone));
    REQUIRE(r.granted == kMic);
    REQUIRE_FALSE(grant(&r, Capability::Notifications));
    REQUIRE(r.granted == kMic);
    REQUIRE(r.declared == (kMic | kLoc));
    REQUIRE(grant(&r, Capability::CoarseLocation));
    REQUIRE(r.granted == (kMic | kLoc));
  }
  SECTION("grant is repeatable") {
    GrantRecord r = rec(kAct, kAct);
    REQUIRE(grant(&r, Capability::ActivityRecognition));
    REQUIRE(r.granted == kAct);
  }
  SECTION("revoke") {
    GrantRecord r = rec(15, 15);
    REQUIRE(revoke(&r, Capability::Notifications));
    REQUIRE(r.granted == (kMic | kAct | kLoc));
    REQUIRE(revoke(&r, Capability::Notifications));  // not granted: allowed, unchanged
    REQUIRE(r.granted == (kMic | kAct | kLoc));
    REQUIRE(r.declared == 15);
    GrantRecord none = rec(0, 0);
    REQUIRE(revoke(&none, Capability::Microphone));
    REQUIRE(none.granted == 0);
  }
  SECTION("invalid capability values") {
    for (unsigned v : {4u, 5u, 31u, 32u, 255u}) {
      GrantRecord r = rec(0xFFFFFFFFu, 0xFFFFFFF0u);
      REQUIRE_FALSE(grant(&r, cap(v)));
      REQUIRE(r.declared == 0xFFFFFFFFu);
      REQUIRE(r.granted == 0xFFFFFFF0u);
      REQUIRE(revoke(&r, cap(v)));
      REQUIRE(r.declared == 0xFFFFFFFFu);
      REQUIRE(r.granted == 0xFFFFFFF0u);
    }
  }
  SECTION("a null record fails both") {
    REQUIRE_FALSE(grant(nullptr, Capability::Microphone));
    REQUIRE_FALSE(revoke(nullptr, Capability::Microphone));
    REQUIRE_FALSE(grant(nullptr, cap(40)));
    REQUIRE_FALSE(revoke(nullptr, cap(40)));
  }
  SECTION("stray granted bits are not repaired by grant or revoke") {
    GrantRecord r = rec(kMic, kNot | 0x10);
    REQUIRE(grant(&r, Capability::Microphone));
    REQUIRE(r.granted == (kMic | kNot | 0x10));
    REQUIRE(revoke(&r, Capability::Microphone));
    REQUIRE(r.granted == (kNot | 0x10));
  }
}

TEST_CASE("reconcile_update by hand", "[capabilities]") {
  SECTION("a subset keeps the grants that are still declared") {
    const GrantRecord r = reconcile_update(rec(kMic | kNot | kAct, kMic | kAct), kMic | kNot);
    REQUIRE(r.declared == (kMic | kNot));
    REQUIRE(r.granted == kMic);
  }
  SECTION("equal sets are a subset") {
    const GrantRecord r = reconcile_update(rec(kMic | kNot, kNot), kMic | kNot);
    REQUIRE(r.declared == (kMic | kNot));
    REQUIRE(r.granted == kNot);
  }
  SECTION("a new capability resets every grant") {
    const GrantRecord r = reconcile_update(rec(kMic | kNot, kMic | kNot), kMic | kLoc);
    REQUIRE(r.declared == (kMic | kLoc));
    REQUIRE(r.granted == 0);
    const GrantRecord s = reconcile_update(rec(kMic, kMic), kMic | kNot);
    REQUIRE(s.granted == 0);
    REQUIRE(s.declared == (kMic | kNot));
  }
  SECTION("empty sets") {
    GrantRecord r = reconcile_update(rec(kMic, kMic), 0);
    REQUIRE(r.declared == 0);
    REQUIRE(r.granted == 0);
    r = reconcile_update(rec(0, 0), kMic);
    REQUIRE(r.declared == kMic);
    REQUIRE(r.granted == 0);
    r = reconcile_update(rec(0, 0), 0);
    REQUIRE(r.declared == 0);
    REQUIRE(r.granted == 0);
  }
  SECTION("stray granted bits are repaired first") {
    // granted has Notifications but declared does not: it must not come back when the update declares it.
    const GrantRecord r = reconcile_update(rec(kMic, kMic | kNot), kMic);
    REQUIRE(r.granted == kMic);
    const GrantRecord s = reconcile_update(rec(kMic, kNot), kMic);
    REQUIRE(s.granted == 0);
  }
  SECTION("high bits are masked and never reset or appear") {
    GrantRecord r = reconcile_update(rec(kMic | 0xFFFFFFF0u, kMic | 0xFFFFFFF0u), kMic | 0x10);
    REQUIRE(r.declared == kMic);
    REQUIRE(r.granted == kMic);
    r = reconcile_update(rec(kMic, kMic), kMic | 0x80000000u);
    REQUIRE(r.declared == kMic);
    REQUIRE(r.granted == kMic);
    r = reconcile_update(rec(0, 0xFFFFFFF0u), 0xFFFFFFF0u);
    REQUIRE(r.declared == 0);
    REQUIRE(r.granted == 0);
  }
}

TEST_CASE("query by hand", "[capabilities]") {
  SECTION("the five statuses in order") {
    REQUIRE(query(rec(0, 0), Capability::Microphone, plat(15, 15)) == CapabilityStatus::NotDeclared);
    REQUIRE(query(rec(kMic, 0), Capability::Microphone, plat(15, 15)) == CapabilityStatus::PermissionDenied);
    REQUIRE(query(rec(kMic, kMic), Capability::Microphone, plat(0, 15)) == CapabilityStatus::Unsupported);
    REQUIRE(query(rec(kMic, kMic), Capability::Microphone, plat(15, 0)) == CapabilityStatus::TemporarilyUnavailable);
    REQUIRE(query(rec(kMic, kMic), Capability::Microphone, plat(15, 15)) == CapabilityStatus::Available);
  }
  SECTION("each capability uses its own bit") {
    for (unsigned i = 0; i < 4; ++i) {
      const CapabilitySet b = CapabilitySet{1} << i;
      REQUIRE(query(rec(b, b), cap(i), plat(b, b)) == CapabilityStatus::Available);
      REQUIRE(query(rec(15 & ~b, 15 & ~b), cap(i), plat(15, 15)) == CapabilityStatus::NotDeclared);
      REQUIRE(query(rec(b, 15 & ~b), cap(i), plat(15, 15)) == CapabilityStatus::PermissionDenied);
      REQUIRE(query(rec(b, b), cap(i), plat(15 & ~b, 15)) == CapabilityStatus::Unsupported);
      REQUIRE(query(rec(b, b), cap(i), plat(15, 15 & ~b)) == CapabilityStatus::TemporarilyUnavailable);
    }
  }
  SECTION("active without supported is Unsupported") {
    REQUIRE(query(rec(kMic, kMic), Capability::Microphone, plat(0, kMic)) == CapabilityStatus::Unsupported);
  }
  SECTION("not granted beats unsupported, not declared beats everything") {
    REQUIRE(query(rec(kMic, 0), Capability::Microphone, plat(0, 0)) == CapabilityStatus::PermissionDenied);
    REQUIRE(query(rec(0, kMic), Capability::Microphone, plat(0, 0)) == CapabilityStatus::NotDeclared);
    REQUIRE(query(rec(0, kMic), Capability::Microphone, plat(15, 15)) == CapabilityStatus::NotDeclared);
  }
  SECTION("Count_ and above are NotDeclared whatever the bits say") {
    for (unsigned v : {4u, 5u, 31u, 32u, 255u}) {
      REQUIRE(query(rec(0xFFFFFFFFu, 0xFFFFFFFFu), cap(v), plat(0xFFFFFFFFu, 0xFFFFFFFFu)) ==
              CapabilityStatus::NotDeclared);
    }
  }
  SECTION("revocation shows in the next query, and a re-grant too") {
    GrantRecord r = rec(kMic, 0);
    const PlatformState p = plat(15, 15);
    REQUIRE(query(r, Capability::Microphone, p) == CapabilityStatus::PermissionDenied);
    REQUIRE(grant(&r, Capability::Microphone));
    REQUIRE(query(r, Capability::Microphone, p) == CapabilityStatus::Available);
    REQUIRE(revoke(&r, Capability::Microphone));
    REQUIRE(query(r, Capability::Microphone, p) == CapabilityStatus::PermissionDenied);
    REQUIRE(grant(&r, Capability::Microphone));
    const GrantRecord updated = reconcile_update(r, kMic | kNot);
    REQUIRE(query(updated, Capability::Microphone, p) == CapabilityStatus::PermissionDenied);
  }
  SECTION("query does not change the record") {
    GrantRecord r = rec(0xFFu, 0xF0u);
    (void)query(r, Capability::Microphone, plat(15, 15));
    REQUIRE(r.declared == 0xFFu);
    REQUIRE(r.granted == 0xF0u);
  }
}

TEST_CASE("to_string", "[capabilities]") {
  REQUIRE(std::string(to_string(DeclareVerdict::Ok)) == "Ok");
  REQUIRE(std::string(to_string(DeclareVerdict::UnknownCapability)) == "UnknownCapability");
  REQUIRE(std::string(to_string(DeclareVerdict::DuplicateCapability)) == "DuplicateCapability");
  REQUIRE(std::string(to_string(DeclareVerdict::TooManyNames)) == "TooManyNames");
  REQUIRE(std::string(to_string(static_cast<DeclareVerdict>(9))) == "Unknown");
  REQUIRE(std::string(to_string(CapabilityStatus::Available)) == "Available");
  REQUIRE(std::string(to_string(CapabilityStatus::NotDeclared)) == "NotDeclared");
  REQUIRE(std::string(to_string(CapabilityStatus::PermissionDenied)) == "PermissionDenied");
  REQUIRE(std::string(to_string(CapabilityStatus::Unsupported)) == "Unsupported");
  REQUIRE(std::string(to_string(CapabilityStatus::TemporarilyUnavailable)) == "TemporarilyUnavailable");
  REQUIRE(std::string(to_string(static_cast<CapabilityStatus>(9))) == "Unknown");
  REQUIRE(static_cast<int>(DeclareVerdict::TooManyNames) == 3);
  REQUIRE(static_cast<int>(CapabilityStatus::TemporarilyUnavailable) == 4);
}

// ---------------------------------------------------------------------------------------------------------
// Exhaustive checks. Digests come from tools/capabilities_ref.py; the oracle below keeps every set as an array
// of booleans and shares no code with capabilities.cpp.

TEST_CASE("exhaustive digests match the Python reference", "[capabilities]") {
  SECTION("query: 16 declared x 16 granted x 16 supported x 16 active x 5 capabilities = 327680 cases") {
    Fnv h;
    std::size_t n = 0;
    for (CapabilitySet d = 0; d < 16; ++d)
      for (CapabilitySet g = 0; g < 16; ++g)
        for (CapabilitySet s = 0; s < 16; ++s)
          for (CapabilitySet a = 0; a < 16; ++a)
            for (unsigned c = 0; c < 5; ++c) {
              h.byte(static_cast<std::uint8_t>(query(rec(d, g), cap(c), plat(s, a))));
              ++n;
            }
    REQUIRE(n == 327680);
    REQUIRE(h.h == kRefDigest_query);
  }
  SECTION("query with stray high bits and invalid capability values") {
    Fnv h;
    for (CapabilitySet d = 0; d < 16; ++d)
      for (CapabilitySet g = 0; g < 16; ++g)
        for (std::uint32_t hi : kRefHigh)
          for (std::uint8_t c : kRefCaps) {
            h.byte(static_cast<std::uint8_t>(query(rec(d, g), cap(c), plat(0xF, 0xF))));
            h.byte(static_cast<std::uint8_t>(query(rec(d | hi, g | hi), cap(c), plat(5 | hi, 3 | hi))));
          }
    REQUIRE(h.h == kRefDigest_query_wide);
  }
  SECTION("reconcile_update: 16 x 16 x 16 sets, each with the 4 high-bit patterns on both sides") {
    Fnv h;
    for (CapabilitySet d = 0; d < 16; ++d)
      for (CapabilitySet g = 0; g < 16; ++g)
        for (CapabilitySet n = 0; n < 16; ++n)
          for (std::uint32_t hi : kRefHigh)
            for (std::uint32_t hj : kRefHigh) {
              const GrantRecord r = reconcile_update(rec(d | hi, g | hj), n | hi);
              h.num(r.declared, 4);
              h.num(r.granted, 4);
            }
    REQUIRE(h.h == kRefDigest_update);
  }
  SECTION("grant and revoke over 16 declared x 32 granted x 9 capability values") {
    Fnv h;
    for (CapabilitySet d = 0; d < 16; ++d)
      for (CapabilitySet g = 0; g < 32; ++g)
        for (std::uint8_t c : kRefCaps) {
          GrantRecord r = rec(d, g);
          const bool ok = grant(&r, cap(c));
          h.byte(ok ? 1 : 0);
          h.num(r.granted, 4);
          r = rec(d, g);
          const bool ok2 = revoke(&r, cap(c));
          h.byte(ok2 ? 1 : 0);
          h.num(r.granted, 4);
        }
    REQUIRE(h.h == kRefDigest_grant_revoke);
  }
  SECTION("parse_declared over every list of up to 5 names from 10 and 9 limits") {
    Fnv h;
    std::size_t cases = 0;
    for (std::size_t len = 0; len <= 5; ++len) {
      std::size_t total = 1;
      for (std::size_t i = 0; i < len; ++i) total *= kRefPool.size();
      for (std::size_t k = 0; k < total; ++k) {
        std::vector<std::string> names;
        std::size_t x = k;
        // Most significant digit first, as itertools.product orders them.
        std::vector<std::size_t> digits(len);
        for (std::size_t i = len; i > 0; --i) {
          digits[i - 1] = x % kRefPool.size();
          x /= kRefPool.size();
        }
        for (std::size_t dgt : digits) names.push_back(kRefPool[dgt]);
        for (std::size_t m : kRefMaxNames) {
          const DeclareResult r = parse_declared(names, m);
          h.byte(static_cast<std::uint8_t>(r.verdict));
          h.num(r.index, 8);
          h.num(r.declared, 4);
          ++cases;
        }
      }
    }
    REQUIRE(cases == kRefParseCases);
    REQUIRE(h.h == kRefDigest_parse);
  }
}

namespace {

using Flags = std::array<bool, 4>;

Flags flags_of(std::uint32_t mask) {
  Flags f{};
  for (int i = 0; i < 4; ++i) f[static_cast<std::size_t>(i)] = ((mask >> i) & 1u) != 0;
  return f;
}

std::uint32_t mask_of(const Flags& f) {
  std::uint32_t m = 0;
  for (std::size_t i = 0; i < 4; ++i)
    if (f[i]) m += 1u << i;
  return m;
}

CapabilityStatus oracle_query(const Flags& decl, const Flags& gr, const Flags& sup, const Flags& act, unsigned c) {
  if (c >= 4) return CapabilityStatus::NotDeclared;
  if (!decl[c]) return CapabilityStatus::NotDeclared;
  if (!gr[c]) return CapabilityStatus::PermissionDenied;
  if (!sup[c]) return CapabilityStatus::Unsupported;
  if (!act[c]) return CapabilityStatus::TemporarilyUnavailable;
  return CapabilityStatus::Available;
}

}  // namespace

TEST_CASE("exhaustive comparison with a boolean-array oracle", "[capabilities]") {
  SECTION("query") {
    for (std::uint32_t d = 0; d < 16; ++d)
      for (std::uint32_t g = 0; g < 16; ++g)
        for (std::uint32_t s = 0; s < 16; ++s)
          for (std::uint32_t a = 0; a < 16; ++a)
            for (unsigned c = 0; c < 6; ++c) {
              const CapabilityStatus want = oracle_query(flags_of(d), flags_of(g), flags_of(s), flags_of(a), c);
              REQUIRE(query(rec(d, g), cap(c), plat(s, a)) == want);
            }
  }
  SECTION("update") {
    for (std::uint32_t d = 0; d < 16; ++d)
      for (std::uint32_t g = 0; g < 16; ++g)
        for (std::uint32_t n = 0; n < 16; ++n) {
          const Flags od = flags_of(d), og = flags_of(g), nd = flags_of(n);
          bool grows = false;
          for (std::size_t i = 0; i < 4; ++i)
            if (nd[i] && !od[i]) grows = true;
          Flags want{};
          if (!grows)
            for (std::size_t i = 0; i < 4; ++i) want[i] = og[i] && od[i] && nd[i];
          const GrantRecord r = reconcile_update(rec(d, g), n);
          REQUIRE(r.declared == mask_of(nd));
          REQUIRE(r.granted == mask_of(want));
        }
  }
  SECTION("parse_declared over lists of up to 4 names") {
    const std::size_t pool = kRefPool.size();
    for (std::size_t len = 0; len <= 4; ++len) {
      std::size_t total = 1;
      for (std::size_t i = 0; i < len; ++i) total *= pool;
      for (std::size_t k = 0; k < total; ++k) {
        std::vector<std::size_t> digits(len);
        std::size_t x = k;
        for (std::size_t i = 0; i < len; ++i) {
          digits[i] = x % pool;
          x /= pool;
        }
        std::vector<std::string> names;
        for (std::size_t dgt : digits) names.push_back(kRefPool[dgt]);
        for (std::size_t m : kRefMaxNames) {
          DeclareVerdict want = DeclareVerdict::Ok;
          std::size_t want_index = 0;
          Flags seen{};
          if (names.size() > m) {
            want = DeclareVerdict::TooManyNames;
            want_index = m;
          } else {
            for (std::size_t i = 0; i < names.size() && want == DeclareVerdict::Ok; ++i) {
              int which = -1;
              for (int j = 0; j < 4; ++j) {
                const std::string ref = kRefNames[j];
                if (names[i] == ref) which = j;
              }
              if (which < 0) {
                want = DeclareVerdict::UnknownCapability;
                want_index = i;
              } else if (seen[static_cast<std::size_t>(which)]) {
                want = DeclareVerdict::DuplicateCapability;
                want_index = i;
              } else {
                seen[static_cast<std::size_t>(which)] = true;
              }
            }
          }
          const DeclareResult r = parse_declared(names, m);
          REQUIRE(r.verdict == want);
          REQUIRE(r.index == want_index);
          REQUIRE(r.declared == (want == DeclareVerdict::Ok ? mask_of(seen) : 0u));
        }
      }
    }
  }
  SECTION("grant and revoke") {
    for (std::uint32_t d = 0; d < 16; ++d)
      for (std::uint32_t g = 0; g < 32; ++g)
        for (unsigned c = 0; c < 7; ++c) {
          GrantRecord r = rec(d, g);
          const bool ok = grant(&r, cap(c));
          const bool want_ok = c < 4 && flags_of(d)[c];
          REQUIRE(ok == want_ok);
          REQUIRE(r.declared == d);
          REQUIRE(r.granted == (want_ok ? (g | (1u << c)) : g));
          GrantRecord q = rec(d, g);
          REQUIRE(revoke(&q, cap(c)));
          REQUIRE(q.declared == d);
          REQUIRE(q.granted == (c < 4 ? (g & ~(1u << c)) : g));
        }
  }
}
