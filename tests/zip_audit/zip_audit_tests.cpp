// Tests for the ZIP container audit (core/package/zip_audit.h). Pure logic on byte buffers: no
// inflate, no files, no GPU. Archives come from three places: a small builder in this file with
// per-field control, hex constants made by tools/zip_audit_ref.py (written independently with
// struct and zlib, and opened with Python's zipfile), and a byte-flip mutation run. See
// docs/zip-audit.md for the rules, their order and the exempt fields of the mutation run.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "entry_policy.h"
#include "zip_audit.h"

using namespace lumora::package;

namespace {

using Bytes = std::vector<std::uint8_t>;
constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

// BEGIN GENERATED REFERENCE (tools/zip_audit_ref.py)
struct RefCase {
  const char* name;
  const char* hex;
  const char* verdict;
  std::uint64_t offset;
  std::size_t index;
};
const RefCase kRefCases[] = {
    {"valid_basic",
     "504b03041400000000000060215a20303a36060000000600000005000000612e74787468656c6c6f0a504b03041400000000000060215a000000000000000000000000040000006469722f504b03041400000008000060215a0f55262937000000c8000000090000006469722f622e62696e6360646266616563e7e0e4e2e6e1e5e3171014121611151397909492969195935750545256515553d7d0d4d2d6d1d5d337306418943a00504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48100000000612e747874504b01021e031400000000000060215a000000000000000000000000040000000000000000001000ed41290000006469722f504b01021e031400000008000060215a0f55262937000000c8000000090000000000000000000000a4814b0000006469722f622e62696e504b050600000000030003009c000000a90000000000",
     "Ok", 0u, 0u},
    {"valid_empty",
     "504b0506000000000000000000000000000000000000",
     "Ok", 0u, 0u},
    {"valid_comment",
     "504b03041400000000000060215a20303a36060000000600000005000000612e74787468656c6c6f0a504b03041400000000000060215a000000000000000000000000040000006469722f504b03041400000008000060215a0f55262937000000c8000000090000006469722f622e62696e6360646266616563e7e0e4e2e6e1e5e3171014121611151397909492969195935750545256515553d7d0d4d2d6d1d5d337306418943a00504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48100000000612e747874504b01021e031400000000000060215a000000000000000000000000040000000000000000001000ed41290000006469722f504b01021e031400000008000060215a0f55262937000000c8000000090000000000000000000000a4814b0000006469722f622e62696e504b050600000000030003009c000000a90000000c006d6164652062792068616e64",
     "Ok", 0u, 0u},
    {"prepended",
     "4a554e4b21504b03041400000000000060215a20303a36060000000600000005000000612e74787468656c6c6f0a504b03041400000000000060215a000000000000000000000000040000006469722f504b03041400000008000060215a0f55262937000000c8000000090000006469722f622e62696e6360646266616563e7e0e4e2e6e1e5e3171014121611151397909492969195935750545256515553d7d0d4d2d6d1d5d337306418943a00504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48105000000612e747874504b01021e031400000000000060215a000000000000000000000000040000000000000000001000ed412e0000006469722f504b01021e031400000008000060215a0f55262937000000c8000000090000000000000000000000a481500000006469722f622e62696e504b050600000000030003009c000000ae0000000000",
     "PrependedData", 5u, 0u},
    {"appended",
     "504b03041400000000000060215a20303a36060000000600000005000000612e74787468656c6c6f0a504b03041400000000000060215a000000000000000000000000040000006469722f504b03041400000008000060215a0f55262937000000c8000000090000006469722f622e62696e6360646266616563e7e0e4e2e6e1e5e3171014121611151397909492969195935750545256515553d7d0d4d2d6d1d5d337306418943a00504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48100000000612e747874504b01021e031400000000000060215a000000000000000000000000040000000000000000001000ed41290000006469722f504b01021e031400000008000060215a0f55262937000000c8000000090000000000000000000000a4814b0000006469722f622e62696e504b050600000000030003009c000000a900000000007461696c",
     "TrailingData", 347u, 0u},
    {"second_end_record_in_comment",
     "504b03041400000000000060215a20303a36060000000600000005000000612e74787468656c6c6f0a504b03041400000000000060215a000000000000000000000000040000006469722f504b03041400000008000060215a0f55262937000000c8000000090000006469722f622e62696e6360646266616563e7e0e4e2e6e1e5e3171014121611151397909492969195935750545256515553d7d0d4d2d6d1d5d337306418943a00504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48100000000612e747874504b01021e031400000000000060215a000000000000000000000000040000000000000000001000ed41290000006469722f504b01021e031400000008000060215a0f55262937000000c8000000090000000000000000000000a4814b0000006469722f622e62696e504b050600000000030003009c000000a90000001600504b0506000000000000000000000000000000000000",
     "MultipleEndRecords", 347u, 0u},
    {"local_name_mismatch",
     "504b03041400000000000060215a20303a36060000000600000005000000622e74787468656c6c6f0a504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48100000000612e747874504b0506000000000100010033000000290000000000",
     "HeaderMismatch", 0u, 0u},
    {"overlap_inside_first_entry",
     "504b03041400000000000060215acac6889964000000640000000100000061000000000000000000504b03041400000000000060215a1c3af5cb05000000050000000100000062313233343500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000504b01021e031400000000000060215acac688996400000064000000010000000000000000000000a4810000000061504b01021e031400000000000060215a1c3af5cb0500000005000000010000000000000000000000a4812800000062504b050600000000020002005e000000830000000000",
     "Overlap", 40u, 1u},
    {"gap_between_entries",
     "504b03041400000000000060215a8316dc8c010000000100000001000000617853545241595858504b03041400000000000060215a1526dbfb0100000001000000010000006279504b01021e031400000000000060215a8316dc8c0100000001000000010000000000000000000000a4810000000061504b01021e031400000000000060215a1526dbfb0100000001000000010000000000000000000000a4812700000062504b050600000000020002005e000000470000000000",
     "Gap", 32u, 1u},
    {"zip64_locator_without_sentinel",
     "504b03041400000000000060215a20303a36060000000600000005000000612e74787468656c6c6f0a504b03041400000000000060215a000000000000000000000000040000006469722f504b03041400000008000060215a0f55262937000000c8000000090000006469722f622e62696e6360646266616563e7e0e4e2e6e1e5e3171014121611151397909492969195935750545256515553d7d0d4d2d6d1d5d337306418943a00504b01021e031400000000000060215a20303a360600000006000000050000000000000000000000a48100000000612e747874504b01021e031400000000000060215a000000000000000000000000040000000000000000001000ed41290000006469722f504b01021e031400000008000060215a0f55262937000000c8000000090000000000000000000000a4814b0000006469722f622e62696e504b06062c000000000000002d002d000000000000000000030000000000000003000000000000009c00000000000000a900000000000000504b060700000000450100000000000001000000504b050600000000030003009c000000a90000000000",
     "Zip64Mismatch", 381u, 0u},
};
// END GENERATED REFERENCE

void put16(Bytes& b, std::size_t at, std::uint64_t v) {
  for (int i = 0; i < 2; ++i) b[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void put32(Bytes& b, std::size_t at, std::uint64_t v) {
  for (int i = 0; i < 4; ++i) b[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void put64(Bytes& b, std::size_t at, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) b[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void add16(Bytes& b, std::uint64_t v) {
  b.push_back(static_cast<std::uint8_t>(v));
  b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void add32(Bytes& b, std::uint64_t v) {
  add16(b, v & 0xFFFF);
  add16(b, (v >> 16) & 0xFFFF);
}
void add64(Bytes& b, std::uint64_t v) {
  add32(b, v & 0xFFFFFFFFu);
  add32(b, v >> 32);
}
std::uint64_t get16(const Bytes& b, std::size_t at) { return b[at] | (b[at + 1] << 8); }
std::uint64_t get32(const Bytes& b, std::size_t at) {
  return get16(b, at) | (get16(b, at + 2) << 16);
}

// Field positions inside a central header, a local header and the end record.
enum Central : std::size_t {
  cMadeBy = 4, cNeeded = 6, cFlags = 8, cMethod = 10, cTime = 12, cDate = 14, cCrc = 16,
  cCompressed = 20, cUncompressed = 24, cNameLen = 28, cExtraLen = 30, cCommentLen = 32,
  cDisk = 34, cInternal = 36, cExternal = 38, cOffset = 42, cName = 46
};
enum Local : std::size_t {
  lNeeded = 4, lFlags = 6, lMethod = 8, lTime = 10, lDate = 12, lCrc = 14, lCompressed = 18,
  lUncompressed = 22, lNameLen = 26, lExtraLen = 28, lName = 30
};
enum End : std::size_t {
  eDisk = 4, eCdDisk = 6, eEntriesDisk = 8, eEntriesTotal = 10, eCdSize = 12, eCdOffset = 16,
  eCommentLen = 20
};

struct Item {
  std::string name = "f";
  Bytes data;
  std::uint16_t method = 0;
  std::uint16_t flags = 0;
  std::uint16_t made_by = 0x031E;  // Unix, version 3.0
  bool custom_external = false;
  std::uint32_t external = 0;
  Bytes cextra, lextra;
  std::string comment;
  std::size_t gap_before = 0;  // stray bytes in front of this entry's local header
};

struct Opts {
  std::string comment;
  std::size_t pad_before_cd = 0;
  int zip64 = 0;  // 1: record and locator, count field set to the sentinel; 2: record and locator only
};

struct Built {
  Bytes bytes;
  std::vector<std::size_t> local, data, central;
  std::size_t cd_off = 0, cd_size = 0, eocd = 0, z64_record = 0, z64_locator = 0;
};

Bytes block(std::uint16_t id, std::size_t payload) {
  Bytes b;
  add16(b, id);
  add16(b, payload);
  b.insert(b.end(), payload, 0x5A);
  return b;
}

Built build(const std::vector<Item>& items, const Opts& opts = {}) {
  Built r;
  Bytes& o = r.bytes;
  for (const Item& it : items) {
    o.insert(o.end(), it.gap_before, 0x58);
    r.local.push_back(o.size());
    add32(o, 0x04034B50);
    add16(o, 20);
    add16(o, it.flags);
    add16(o, it.method);
    add16(o, 0x6000);
    add16(o, 0x5A21);
    add32(o, 0x0A0B0C0D);
    add32(o, it.data.size());
    add32(o, it.data.size());
    add16(o, it.name.size());
    add16(o, it.lextra.size());
    o.insert(o.end(), it.name.begin(), it.name.end());
    o.insert(o.end(), it.lextra.begin(), it.lextra.end());
    r.data.push_back(o.size());
    o.insert(o.end(), it.data.begin(), it.data.end());
  }
  o.insert(o.end(), opts.pad_before_cd, 0x59);
  r.cd_off = o.size();
  for (std::size_t i = 0; i < items.size(); ++i) {
    const Item& it = items[i];
    r.central.push_back(o.size());
    add32(o, 0x02014B50);
    add16(o, it.made_by);
    add16(o, 20);
    add16(o, it.flags);
    add16(o, it.method);
    add16(o, 0x6000);
    add16(o, 0x5A21);
    add32(o, 0x0A0B0C0D);
    add32(o, it.data.size());
    add32(o, it.data.size());
    add16(o, it.name.size());
    add16(o, it.cextra.size());
    add16(o, it.comment.size());
    add16(o, 0);
    add16(o, 0);
    const bool dir = !it.name.empty() && it.name.back() == '/';
    add32(o, it.custom_external ? it.external : (dir ? 0x41ED0010u : 0x81A40000u));
    add32(o, r.local[i]);
    o.insert(o.end(), it.name.begin(), it.name.end());
    o.insert(o.end(), it.cextra.begin(), it.cextra.end());
    o.insert(o.end(), it.comment.begin(), it.comment.end());
  }
  r.cd_size = o.size() - r.cd_off;
  if (opts.zip64 != 0) {
    r.z64_record = o.size();
    add32(o, 0x06064B50);
    add64(o, 44);
    add16(o, 45);
    add16(o, 45);
    add32(o, 0);
    add32(o, 0);
    add64(o, items.size());
    add64(o, items.size());
    add64(o, r.cd_size);
    add64(o, r.cd_off);
    r.z64_locator = o.size();
    add32(o, 0x07064B50);
    add32(o, 0);
    add64(o, r.z64_record);
    add32(o, 1);
  }
  r.eocd = o.size();
  const std::uint64_t count16 = opts.zip64 == 1 ? 0xFFFF : items.size();
  add32(o, 0x06054B50);
  add16(o, 0);
  add16(o, 0);
  add16(o, count16);
  add16(o, count16);
  add32(o, r.cd_size);
  add32(o, r.cd_off);
  add16(o, opts.comment.size());
  o.insert(o.end(), opts.comment.begin(), opts.comment.end());
  return r;
}

Item file(const std::string& name, std::size_t n = 4, std::uint16_t method = 0) {
  Item it;
  it.name = name;
  it.data.assign(n, 0x41);
  it.method = method;
  return it;
}
Item dir(const std::string& name) {
  Item it;
  it.name = name;
  return it;
}

std::vector<Item> basic_items() { return {file("a.txt", 6), dir("d/"), file("d/b.bin", 20, 8)}; }
Built basic() { return build(basic_items()); }

void check(const Bytes& b, ZipVerdict v, const ZipAuditLimits& limits = {}) {
  const ZipAuditResult r = audit_zip(b.data(), b.size(), limits);
  INFO("got " << to_string(r.verdict) << " at " << r.offset << " index " << r.entry_index);
  REQUIRE(r.verdict == v);
  if (v != ZipVerdict::Ok) {
    REQUIRE(r.entries.empty());
    REQUIRE(r.directory_count == 0);
  }
}
void check_at(const Bytes& b, ZipVerdict v, std::uint64_t offset, std::size_t index = 0,
              const ZipAuditLimits& limits = {}) {
  const ZipAuditResult r = audit_zip(b.data(), b.size(), limits);
  INFO("got " << to_string(r.verdict) << " at " << r.offset << " index " << r.entry_index);
  REQUIRE(r.verdict == v);
  REQUIRE(r.offset == offset);
  REQUIRE(r.entry_index == index);
  REQUIRE(r.entries.empty());
  REQUIRE(r.directory_count == 0);
}

Bytes from_hex(const char* hex) {
  Bytes out;
  auto nib = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  for (std::size_t i = 0; hex[i] != 0 && hex[i + 1] != 0; i += 2) {
    out.push_back(static_cast<std::uint8_t>(nib(hex[i]) * 16 + nib(hex[i + 1])));
  }
  return out;
}

// Same value into the central and the local header of entry i.
void both16(Bytes& b, const Built& r, std::size_t i, std::size_t c, std::size_t l,
            std::uint64_t v) {
  put16(b, r.central[i] + c, v);
  put16(b, r.local[i] + l, v);
}
void both32(Bytes& b, const Built& r, std::size_t i, std::size_t c, std::size_t l,
            std::uint64_t v) {
  put32(b, r.central[i] + c, v);
  put32(b, r.local[i] + l, v);
}

// A ZIP64 extra block with the given 64-bit values.
Bytes zip64_extra(std::initializer_list<std::uint64_t> values) {
  Bytes b;
  add16(b, 1);
  add16(b, 8 * values.size());
  for (std::uint64_t v : values) add64(b, v);
  return b;
}

}  // namespace

TEST_CASE("zip audit accepts a valid archive and reports its entries", "[zip_audit]") {
  const Built r = basic();
  const ZipAuditResult a = audit_zip(r.bytes.data(), r.bytes.size());
  REQUIRE(a.verdict == ZipVerdict::Ok);
  REQUIRE(a.offset == 0);
  REQUIRE(a.directory_count == 1);
  REQUIRE(a.entries.size() == 2);
  REQUIRE(a.entries[0].name == "a.txt");
  REQUIRE(a.entries[0].method == 0);
  REQUIRE(a.entries[0].crc32 == 0x0A0B0C0Du);
  REQUIRE(a.entries[0].compressed_bytes == 6);
  REQUIRE(a.entries[0].uncompressed_bytes == 6);
  REQUIRE(a.entries[0].local_header_offset == 0);
  REQUIRE(a.entries[0].data_offset == 30 + 5);
  REQUIRE(a.entries[1].name == "d/b.bin");
  REQUIRE(a.entries[1].method == 8);
  REQUIRE(a.entries[1].local_header_offset == r.local[2]);
  REQUIRE(a.entries[1].data_offset == r.data[2]);

  const std::vector<EntryInfo> infos = to_entry_infos(a.entries);
  REQUIRE(infos.size() == 2);
  REQUIRE(infos[0].name == "a.txt");
  REQUIRE(infos[0].compressed_bytes == 6);
  REQUIRE(infos[1].uncompressed_bytes == 20);
  REQUIRE(validate_entries(infos).verdict == SetVerdict::Ok);
  REQUIRE(to_entry_infos({}).empty());
}

TEST_CASE("zip audit accepts empty and commented archives", "[zip_audit]") {
  const Built empty = build({});
  check(empty.bytes, ZipVerdict::Ok);
  REQUIRE(empty.bytes.size() == 22);
  const ZipAuditResult a = audit_zip(empty.bytes.data(), empty.bytes.size());
  REQUIRE(a.entries.empty());
  REQUIRE(a.directory_count == 0);

  Opts o;
  o.comment = "hello";
  check(build(basic_items(), o).bytes, ZipVerdict::Ok);
  check(build({dir("only/")}).bytes, ZipVerdict::Ok);
  check(build({file("z", 0)}).bytes, ZipVerdict::Ok);  // empty stored file
}

TEST_CASE("zip audit names every verdict", "[zip_audit]") {
  const ZipVerdict all[] = {
      ZipVerdict::Ok, ZipVerdict::TooLarge, ZipVerdict::TooSmall, ZipVerdict::NoEndRecord,
      ZipVerdict::MultipleEndRecords, ZipVerdict::TrailingData, ZipVerdict::PrependedData,
      ZipVerdict::MultiDisk, ZipVerdict::Zip64Mismatch, ZipVerdict::TooManyEntries,
      ZipVerdict::CentralDirectoryBounds, ZipVerdict::BadCentralHeader,
      ZipVerdict::BadLocalHeader, ZipVerdict::HeaderMismatch, ZipVerdict::Encrypted,
      ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedMethod, ZipVerdict::SpecialFile,
      ZipVerdict::BadSizes, ZipVerdict::Overlap, ZipVerdict::Gap, ZipVerdict::OutOfOrder};
  for (std::size_t i = 0; i < std::size(all); ++i) {
    REQUIRE(std::string(to_string(all[i])) != "Unknown");
    for (std::size_t j = i + 1; j < std::size(all); ++j) {
      REQUIRE(std::string(to_string(all[i])) != std::string(to_string(all[j])));
    }
  }
  REQUIRE(std::string(to_string(static_cast<ZipVerdict>(250))) == "Unknown");
}

TEST_CASE("zip audit checks the archive size first", "[zip_audit]") {
  const Built r = basic();
  const std::uint64_t n = r.bytes.size();
  ZipAuditLimits lim;
  lim.max_archive_bytes = n;
  check(r.bytes, ZipVerdict::Ok, lim);
  lim.max_archive_bytes = n - 1;
  check_at(r.bytes, ZipVerdict::TooLarge, 0, 0, lim);
  lim.max_archive_bytes = 0;
  check(r.bytes, ZipVerdict::TooLarge, lim);
  lim.max_archive_bytes = kMax;
  check(r.bytes, ZipVerdict::Ok, lim);
  // TooLarge wins over TooSmall and over everything else.
  Bytes junk(100, 0);
  lim.max_archive_bytes = 50;
  check(junk, ZipVerdict::TooLarge, lim);
  // Empty input, with and without a pointer.
  REQUIRE(audit_zip(nullptr, 0).verdict == ZipVerdict::TooSmall);
  lim.max_archive_bytes = 0;
  REQUIRE(audit_zip(nullptr, 0, lim).verdict == ZipVerdict::TooSmall);
}

TEST_CASE("zip audit rejects buffers shorter than an end record", "[zip_audit]") {
  const Built e = build({});
  for (std::size_t n = 0; n < 22; ++n) {
    const ZipAuditResult r = audit_zip(e.bytes.data(), n);
    REQUIRE(r.verdict == ZipVerdict::TooSmall);
  }
  check(e.bytes, ZipVerdict::Ok);
}

TEST_CASE("zip audit finds exactly one end record", "[zip_audit]") {
  SECTION("no signature") {
    check_at(Bytes(64, 0), ZipVerdict::NoEndRecord, 0);
    Built r = basic();
    r.bytes[r.eocd] ^= 0xFF;
    check_at(r.bytes, ZipVerdict::NoEndRecord, 0);
  }
  SECTION("comment length runs past the buffer") {
    Opts o;
    o.comment = "abc";
    Built r = build(basic_items(), o);
    r.bytes.pop_back();
    check_at(r.bytes, ZipVerdict::NoEndRecord, r.eocd);
    r = build(basic_items(), o);
    put16(r.bytes, r.eocd + eCommentLen, 4);
    check_at(r.bytes, ZipVerdict::NoEndRecord, r.eocd);
  }
  SECTION("bytes after the comment") {
    Built r = basic();
    const std::size_t end = r.bytes.size();
    r.bytes.push_back(0);
    check_at(r.bytes, ZipVerdict::TrailingData, end);
    Opts o;
    o.comment = "abc";
    Built c = build(basic_items(), o);
    const std::size_t cend = c.bytes.size();
    c.bytes.insert(c.bytes.end(), 100, 7);
    check_at(c.bytes, ZipVerdict::TrailingData, cend);
    // A comment that is shorter than the real tail is trailing data too.
    Built s = build(basic_items(), o);
    put16(s.bytes, s.eocd + eCommentLen, 2);
    check_at(s.bytes, ZipVerdict::TrailingData, s.eocd + 22 + 2);
  }
  SECTION("a second record signature in the comment") {
    Bytes inner = build({}).bytes;  // a whole end record
    Opts o;
    o.comment.assign(inner.begin(), inner.end());
    const Built r = build(basic_items(), o);
    check_at(r.bytes, ZipVerdict::MultipleEndRecords, r.eocd + 22);
    // Only the signature plus enough room for a record counts; a short tail does not.
    Opts shortc;
    shortc.comment = std::string("PK\x05\x06", 4) + "xx";
    check(build(basic_items(), shortc).bytes, ZipVerdict::Ok);
    Opts barely;
    barely.comment = std::string("PK\x05\x06", 4) + std::string(14, 'x');  // 18 bytes: no room
    check(build(basic_items(), barely).bytes, ZipVerdict::Ok);
    Opts room;
    room.comment = std::string("PK\x05\x06", 4) + std::string(18, 'x');  // exactly 22 bytes
    check(build(basic_items(), room).bytes, ZipVerdict::MultipleEndRecords);
  }
  SECTION("ZIP64 signatures in a comment do not count") {
    Opts o;
    o.comment = std::string("PK\x06\x06", 4) + std::string("PK\x06\x07", 4) + std::string(40, 'z');
    check(build(basic_items(), o).bytes, ZipVerdict::Ok);
  }
  SECTION("two archives back to back") {
    const Built a = basic();
    Bytes both = a.bytes;
    both.insert(both.end(), a.bytes.begin(), a.bytes.end());
    check(both, ZipVerdict::MultipleEndRecords);
  }
  SECTION("a stored file that contains a record signature is rejected too") {
    Item it = file("n", 30);
    std::memcpy(it.data.data() + 2, "PK\x05\x06", 4);
    check(build({it}).bytes, ZipVerdict::MultipleEndRecords);
  }
}

TEST_CASE("zip audit limits the archive comment", "[zip_audit]") {
  ZipAuditLimits lim;
  Opts o;
  o.comment.assign(1024, 'c');
  const Built at_limit = build(basic_items(), o);
  check(at_limit.bytes, ZipVerdict::Ok, lim);
  o.comment.assign(1025, 'c');
  const Built over = build(basic_items(), o);
  check_at(over.bytes, ZipVerdict::NoEndRecord, over.eocd, 0, lim);
  lim.max_comment_bytes = 1025;
  check(over.bytes, ZipVerdict::Ok, lim);
  lim.max_comment_bytes = 0;
  check_at(at_limit.bytes, ZipVerdict::NoEndRecord, at_limit.eocd, 0, lim);
  check(basic().bytes, ZipVerdict::Ok, lim);
  // The largest comment the format allows sits at the edge of the scan window.
  o.comment.assign(65535, 'c');
  const Built biggest = build(basic_items(), o);
  lim.max_comment_bytes = kMax;
  check(biggest.bytes, ZipVerdict::Ok, lim);
  lim.max_comment_bytes = 65535;
  check(biggest.bytes, ZipVerdict::Ok, lim);
  lim.max_comment_bytes = 65534;
  check_at(biggest.bytes, ZipVerdict::NoEndRecord, biggest.eocd, 0, lim);
}

TEST_CASE("zip audit rejects multi disk archives", "[zip_audit]") {
  Built r = basic();
  put16(r.bytes, r.eocd + eDisk, 1);
  check_at(r.bytes, ZipVerdict::MultiDisk, r.eocd);
  r = basic();
  put16(r.bytes, r.eocd + eCdDisk, 1);
  check_at(r.bytes, ZipVerdict::MultiDisk, r.eocd);
  r = basic();
  put16(r.bytes, r.eocd + eEntriesDisk, 2);
  check_at(r.bytes, ZipVerdict::MultiDisk, r.eocd);
  r = basic();
  put16(r.bytes, r.eocd + eDisk, 0xFFFF);
  check(r.bytes, ZipVerdict::MultiDisk);
  r = basic();
  put16(r.bytes, r.central[1] + cDisk, 1);
  check_at(r.bytes, ZipVerdict::MultiDisk, r.central[1], 1);
}

TEST_CASE("zip audit limits the entry count", "[zip_audit]") {
  const Built r = basic();  // 3 entries, one of them a directory
  ZipAuditLimits lim;
  lim.max_entries = 3;
  check(r.bytes, ZipVerdict::Ok, lim);
  lim.max_entries = 2;
  check_at(r.bytes, ZipVerdict::TooManyEntries, r.eocd, 0, lim);
  lim.max_entries = 0;
  check(r.bytes, ZipVerdict::TooManyEntries, lim);
  check(build({}).bytes, ZipVerdict::Ok, lim);
  lim.max_entries = kMax;
  check(r.bytes, ZipVerdict::Ok, lim);
}

TEST_CASE("zip audit checks the central directory bounds", "[zip_audit]") {
  SECTION("start or size off by one") {
    Built r = basic();
    put32(r.bytes, r.eocd + eCdOffset, r.cd_off + 1);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
    r = basic();
    put32(r.bytes, r.eocd + eCdSize, r.cd_size + 1);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
    r = basic();
    put32(r.bytes, r.eocd + eCdSize, r.cd_size - 1);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
  }
  SECTION("range outside the buffer") {
    Built r = basic();
    put32(r.bytes, r.eocd + eCdOffset, r.eocd + 1);
    put32(r.bytes, r.eocd + eCdSize, 0);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
    r = basic();
    put32(r.bytes, r.eocd + eCdSize, 0xFFFFFFFEu);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
    r = basic();
    put32(r.bytes, r.eocd + eCdOffset, 0xFFFFFFFEu);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
  }
  SECTION("entry count disagrees with the directory") {
    Built r = basic();
    put16(r.bytes, r.eocd + eEntriesDisk, 4);
    put16(r.bytes, r.eocd + eEntriesTotal, 4);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
    r = basic();
    put16(r.bytes, r.eocd + eEntriesDisk, 2);
    put16(r.bytes, r.eocd + eEntriesTotal, 2);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
    r = build({});
    put32(r.bytes, r.eocd + eCdSize, 1);
    check(r.bytes, ZipVerdict::CentralDirectoryBounds);
    // A header whose variable part runs past the directory is the same failure.
    r = basic();
    put16(r.bytes, r.central[2] + cNameLen, 0x00FF);
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd);
  }
  SECTION("a directory start that moves without the walk landing on the end") {
    Built r = basic();
    put32(r.bytes, r.eocd + eCdOffset, r.cd_off - 1);
    put32(r.bytes, r.eocd + eCdSize, r.cd_size + 1);
    check(r.bytes, ZipVerdict::CentralDirectoryBounds);  // the walk no longer lands on the end
  }
}

TEST_CASE("zip audit checks central header fields and limits", "[zip_audit]") {
  SECTION("signature") {
    Built r = basic();
    r.bytes[r.central[1]] ^= 1;
    check_at(r.bytes, ZipVerdict::BadCentralHeader, r.central[1], 1);
  }
  SECTION("empty name") {
    // Remove the name byte and the length together so the directory still adds up.
    Built q = build({file("a")});
    q.bytes.erase(q.bytes.begin() + static_cast<long>(q.central[0] + cName));
    put16(q.bytes, q.central[0] + cNameLen, 0);
    put32(q.bytes, q.eocd - 1 + eCdSize, q.cd_size - 1);
    check_at(q.bytes, ZipVerdict::BadCentralHeader, q.central[0]);
  }
  SECTION("name length at and over the limit") {
    check(build({file(std::string(255, 'n'))}).bytes, ZipVerdict::Ok);
    const Built over = build({file(std::string(256, 'n'))});
    check_at(over.bytes, ZipVerdict::BadCentralHeader, over.central[0]);
    ZipAuditLimits lim;
    lim.max_name_bytes = 1;
    check(build({file("a")}).bytes, ZipVerdict::Ok, lim);
    check(build({file("ab")}).bytes, ZipVerdict::BadCentralHeader, lim);
    lim.max_name_bytes = kMax;
    check(over.bytes, ZipVerdict::Ok, lim);
  }
  SECTION("extra length at and over the limit") {
    Item ok = file("a");
    ok.cextra = block(0xCAFE, 1020);
    ok.lextra = block(0xCAFE, 1020);
    check(build({ok}).bytes, ZipVerdict::Ok);
    Item over = file("a");
    over.cextra = block(0xCAFE, 1021);
    over.lextra = block(0xCAFE, 1021);
    const Built r = build({over});
    check_at(r.bytes, ZipVerdict::BadCentralHeader, r.central[0]);
    ZipAuditLimits lim;
    lim.max_extra_bytes = 1025;
    check(r.bytes, ZipVerdict::Ok, lim);
    lim.max_extra_bytes = 0;
    check(build({file("a")}).bytes, ZipVerdict::Ok, lim);
    check(build({ok}).bytes, ZipVerdict::BadCentralHeader, lim);
  }
  SECTION("entry comment at and over the limit") {
    Item ok = file("a");
    ok.comment.assign(1024, 'c');
    check(build({ok}).bytes, ZipVerdict::Ok);
    ok.comment.assign(1025, 'c');
    const Built r = build({ok});
    check_at(r.bytes, ZipVerdict::BadCentralHeader, r.central[0]);
    ZipAuditLimits lim;
    lim.max_comment_bytes = 1025;
    check(r.bytes, ZipVerdict::Ok, lim);
    lim.max_comment_bytes = kMax;
    check(r.bytes, ZipVerdict::Ok, lim);
  }
  SECTION("broken extra block") {
    Item it = file("a");
    it.cextra = {0xFE, 0xCA, 0x10, 0x00, 1, 2};  // block claims 16 bytes, has 2
    it.lextra = block(0xCAFE, 2);
    const Built r = build({it});
    check_at(r.bytes, ZipVerdict::BadCentralHeader, r.central[0]);
    it.cextra = {0xFE, 0xCA, 0x00};  // a header that is cut short
    check(build({it}).bytes, ZipVerdict::BadCentralHeader);
  }
}

TEST_CASE("zip audit flag bits", "[zip_audit]") {
  struct Row {
    int bit;
    ZipVerdict stored;   // method 0
    ZipVerdict deflate;  // method 8
  };
  const Row rows[] = {
      {0, ZipVerdict::Encrypted, ZipVerdict::Encrypted},
      {1, ZipVerdict::UnsupportedFeature, ZipVerdict::Ok},
      {2, ZipVerdict::UnsupportedFeature, ZipVerdict::Ok},
      {3, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {4, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {5, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {6, ZipVerdict::Encrypted, ZipVerdict::Encrypted},
      {7, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {8, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {9, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {10, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {11, ZipVerdict::Ok, ZipVerdict::Ok},
      {12, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {13, ZipVerdict::Encrypted, ZipVerdict::Encrypted},
      {14, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
      {15, ZipVerdict::UnsupportedFeature, ZipVerdict::UnsupportedFeature},
  };
  for (const Row& row : rows) {
    CAPTURE(row.bit);
    Item s = file("a", 4, 0);
    s.flags = static_cast<std::uint16_t>(1u << row.bit);
    check(build({s}).bytes, row.stored);
    Item d = file("a", 4, 8);
    d.flags = s.flags;
    check(build({d}).bytes, row.deflate);
  }
  SECTION("encryption wins over the other flag problems, and the failing header is reported") {
    Item it = file("a", 4, 99);
    it.flags = 0x0001 | 0x0008;
    const Built r = build({file("x"), it});
    check_at(r.bytes, ZipVerdict::Encrypted, r.central[1], 1);
    it.flags = 0x0008;
    check(build({it}).bytes, ZipVerdict::UnsupportedFeature);
    it.flags = 0;
    check(build({it}).bytes, ZipVerdict::UnsupportedMethod);
  }
}

TEST_CASE("zip audit methods", "[zip_audit]") {
  for (std::uint16_t m : {1, 2, 3, 4, 5, 6, 7, 9, 12, 14, 93, 98, 99, 0xFFFF}) {
    CAPTURE(m);
    check(build({file("a", 4, m)}).bytes, ZipVerdict::UnsupportedMethod);
  }
  check(build({file("a", 4, 0)}).bytes, ZipVerdict::Ok);
  check(build({file("a", 4, 8)}).bytes, ZipVerdict::Ok);
}

TEST_CASE("zip audit special files and directory rules", "[zip_audit]") {
  auto with_mode = [](std::uint32_t mode, std::uint16_t made_by = 0x031E,
                      const std::string& name = "a") {
    Item it = file(name, 0);
    it.custom_external = true;
    it.external = mode << 16;
    it.made_by = made_by;
    return build({it}).bytes;
  };
  check(with_mode(0100644), ZipVerdict::Ok);
  check(with_mode(0), ZipVerdict::Ok);  // no type given
  for (std::uint32_t type : {0010000u, 0020000u, 0060000u, 0120000u, 0140000u, 0030000u}) {
    CAPTURE(type);
    check(with_mode(type | 0644), ZipVerdict::SpecialFile);
  }
  // Not made on Unix: the attribute word carries no mode, so it is not read.
  check(with_mode(0120777, 0x001E), ZipVerdict::Ok);
  check(with_mode(0120777, 0x0A1E), ZipVerdict::Ok);
  check(with_mode(0040755, 0x001E), ZipVerdict::Ok);
  // A special file is reported with the entry that holds it.
  Item link = file("l", 0);
  link.custom_external = true;
  link.external = 0120777u << 16;
  const Built r = build({file("a"), link});
  check_at(r.bytes, ZipVerdict::SpecialFile, r.central[1], 1);

  SECTION("directory name and mode must agree") {
    check(with_mode(0100644, 0x031E, "d/"), ZipVerdict::BadCentralHeader);  // regular, slash
    check(with_mode(0040755, 0x031E, "d"), ZipVerdict::BadCentralHeader);   // dir mode, no slash
    check(with_mode(0040755, 0x031E, "d/"), ZipVerdict::Ok);
    check(with_mode(0, 0x031E, "d/"), ZipVerdict::Ok);
    check(with_mode(0100644, 0x001E, "d/"), ZipVerdict::Ok);  // DOS made, mode ignored
  }
  SECTION("a directory is stored with both sizes zero") {
    Item d = dir("d/");
    d.data.assign(1, 0);
    check(build({d}).bytes, ZipVerdict::BadSizes);
    Item m = dir("d/");
    m.method = 8;
    check(build({m}).bytes, ZipVerdict::BadSizes);
    Built r = build({dir("d/")});
    put32(r.bytes, r.central[0] + cUncompressed, 1);
    check_at(r.bytes, ZipVerdict::BadSizes, r.central[0]);
  }
}

TEST_CASE("zip audit local header problems", "[zip_audit]") {
  SECTION("signature and position") {
    Built r = basic();
    r.bytes[r.local[1]] ^= 1;
    check_at(r.bytes, ZipVerdict::BadLocalHeader, r.local[1], 1);
    r = basic();
    put32(r.bytes, r.central[1] + cOffset, r.bytes.size() - 29);
    check_at(r.bytes, ZipVerdict::BadLocalHeader, r.bytes.size() - 29, 1);
    r = basic();
    put32(r.bytes, r.central[1] + cOffset, r.bytes.size() - 30);
    check(r.bytes, ZipVerdict::BadLocalHeader);  // reads fine, but there is no signature
    r = basic();
    put32(r.bytes, r.central[1] + cOffset, 0xFFFFFFFEu);
    check(r.bytes, ZipVerdict::BadLocalHeader);
  }
  SECTION("name and extra limits apply to the local header too") {
    Built r = basic();
    put16(r.bytes, r.local[0] + lNameLen, 256);
    check_at(r.bytes, ZipVerdict::BadLocalHeader, r.local[0]);
    r = basic();
    put16(r.bytes, r.local[0] + lExtraLen, 1025);
    check_at(r.bytes, ZipVerdict::BadLocalHeader, r.local[0]);
    r = basic();
    put16(r.bytes, r.local[2] + lExtraLen, 0xFFFF);
    check_at(r.bytes, ZipVerdict::BadLocalHeader, r.local[2], 2);
    ZipAuditLimits lim;
    lim.max_extra_bytes = kMax;
    check(r.bytes, ZipVerdict::BadLocalHeader, lim);  // longer than the buffer
  }
  SECTION("broken local extra") {
    Item it = file("a");
    it.cextra = block(0xCAFE, 2);
    it.lextra = {0xFE, 0xCA, 0x10, 0x00, 1, 2};
    check(build({it}).bytes, ZipVerdict::BadLocalHeader);
  }
  SECTION("local extra may differ from the central one") {
    Item it = file("a");
    it.cextra = block(0xCAFE, 4);
    it.lextra = block(0xBEEF, 9);
    check(build({it}).bytes, ZipVerdict::Ok);
    it.lextra.clear();
    check(build({it}).bytes, ZipVerdict::Ok);
  }
}

TEST_CASE("zip audit central and local header mismatch", "[zip_audit]") {
  struct Field {
    std::size_t local;
    int width;
    std::uint64_t value;
    const char* name;
  };
  const Field fields[] = {
      {lFlags, 2, 0x0800, "flags"},        {lMethod, 2, 8, "method"},
      {lTime, 2, 0x1234, "time"},          {lDate, 2, 0x1234, "date"},
      {lCrc, 4, 0x11111111, "crc"},        {lCompressed, 4, 5, "compressed size"},
      {lUncompressed, 4, 5, "uncompressed size"},
  };
  for (const Field& f : fields) {
    CAPTURE(f.name);
    Built r = basic();
    if (f.width == 2) put16(r.bytes, r.local[0] + f.local, f.value);
    else put32(r.bytes, r.local[0] + f.local, f.value);
    check_at(r.bytes, ZipVerdict::HeaderMismatch, r.local[0], 0);
  }
  SECTION("name content") {
    Built r = basic();
    r.bytes[r.local[0] + lName] = 'b';
    check_at(r.bytes, ZipVerdict::HeaderMismatch, r.local[0]);
    r = basic();
    r.bytes[r.central[2] + cName + 6] = 'X';
    check_at(r.bytes, ZipVerdict::HeaderMismatch, r.local[2], 2);
  }
  SECTION("name length") {
    Built r = build({file("ab")});
    put16(r.bytes, r.local[0] + lNameLen, 1);  // local says "a", then data starts one byte early
    check(r.bytes, ZipVerdict::HeaderMismatch);
  }
  SECTION("fields that are free") {
    Built r = basic();
    put16(r.bytes, r.central[0] + cNeeded, 63);
    put16(r.bytes, r.local[0] + lNeeded, 10);
    put16(r.bytes, r.central[0] + cInternal, 1);
    put32(r.bytes, r.central[0] + cExternal, 0x81FF0000u | 0x20);
    check(r.bytes, ZipVerdict::Ok);
    // Time, date and CRC are free only when both headers agree.
    both16(r.bytes, r, 0, cTime, lTime, 0xFFFF);
    both16(r.bytes, r, 0, cDate, lDate, 0);
    both32(r.bytes, r, 0, cCrc, lCrc, 0xFFFFFFFFu);
    check(r.bytes, ZipVerdict::Ok);
  }
}

TEST_CASE("zip audit ZIP64 fields in central entries", "[zip_audit]") {
  auto entry_with = [](std::uint32_t usize, std::uint32_t csize, std::uint32_t offset,
                       Bytes cextra) {
    Built r = build({file("a")});
    put32(r.bytes, r.central[0] + cUncompressed, usize);
    put32(r.bytes, r.central[0] + cCompressed, csize);
    put32(r.bytes, r.central[0] + cOffset, offset);
    // Replace the extra field: central header length grows, directory size follows.
    Bytes b(r.bytes.begin(), r.bytes.begin() + static_cast<long>(r.central[0] + cName + 1));
    b.insert(b.end(), cextra.begin(), cextra.end());
    b.insert(b.end(), r.bytes.begin() + static_cast<long>(r.eocd), r.bytes.end());
    const std::size_t eocd = b.size() - 22;
    put16(b, r.central[0] + cExtraLen, cextra.size());
    put32(b, eocd + eCdSize, r.cd_size + cextra.size());
    return b;
  };
  const std::uint32_t S = 0xFFFFFFFFu;
  SECTION("sentinel without an extra block") {
    check_at(entry_with(S, 4, 0, {}), ZipVerdict::Zip64Mismatch, 35, 0);  // the central header

  }
  SECTION("extra block without a sentinel") {
    check(entry_with(4, 4, 0, zip64_extra({1ull << 32})), ZipVerdict::Zip64Mismatch);
  }
  SECTION("wrong payload length") {
    check(entry_with(S, 4, 0, zip64_extra({1ull << 32, 1ull << 32})), ZipVerdict::Zip64Mismatch);
    check(entry_with(S, S, 0, zip64_extra({1ull << 32})), ZipVerdict::Zip64Mismatch);
  }
  SECTION("value that would fit the ordinary field") {
    check(entry_with(S, 4, 0, zip64_extra({5})), ZipVerdict::Zip64Mismatch);
    check(entry_with(S, 4, 0, zip64_extra({0xFFFFFFFEull})), ZipVerdict::Zip64Mismatch);
    check(entry_with(4, 4, S, zip64_extra({0})), ZipVerdict::Zip64Mismatch);
  }
  SECTION("two ZIP64 blocks") {
    Bytes two = zip64_extra({1ull << 32});
    const Bytes again = zip64_extra({1ull << 32});
    two.insert(two.end(), again.begin(), again.end());
    check(entry_with(S, 4, 0, two), ZipVerdict::Zip64Mismatch);
  }
  SECTION("canonical value that points outside the buffer") {
    // Offset exactly at the sentinel, and at the uint64 maximum: the local header is not there.
    check(entry_with(4, 4, S, zip64_extra({0xFFFFFFFFull})), ZipVerdict::BadLocalHeader);
    check(entry_with(4, 4, S, zip64_extra({kMax})), ZipVerdict::BadLocalHeader);
  }
}

TEST_CASE("zip audit ZIP64 fields in local headers", "[zip_audit]") {
  const std::uint32_t S = 0xFFFFFFFFu;
  auto patch_local = [&](std::uint32_t usize, std::uint32_t csize, const Bytes& lextra) {
    Item it = file("a");
    it.lextra = lextra;
    Built r = build({it});
    put32(r.bytes, r.local[0] + lUncompressed, usize);
    put32(r.bytes, r.local[0] + lCompressed, csize);
    return r.bytes;
  };
  check(patch_local(4, 4, {}), ZipVerdict::Ok);
  check(patch_local(S, 4, zip64_extra({1ull << 32, 1ull << 32})), ZipVerdict::BadLocalHeader);
  check(patch_local(4, S, zip64_extra({1ull << 32, 1ull << 32})), ZipVerdict::BadLocalHeader);
  check(patch_local(S, S, {}), ZipVerdict::BadLocalHeader);
  check(patch_local(S, S, zip64_extra({1ull << 32})), ZipVerdict::BadLocalHeader);
  check(patch_local(S, S, zip64_extra({5, 5})), ZipVerdict::BadLocalHeader);
  check(patch_local(4, 4, zip64_extra({1ull << 32, 1ull << 32})), ZipVerdict::BadLocalHeader);
}

TEST_CASE("zip audit ZIP64 sizes that cannot fit the buffer", "[zip_audit]") {
  // Both headers carry the same ZIP64 sizes, so the headers agree and the sizes are the failure.
  auto huge = [](std::uint64_t usize, std::uint64_t csize, std::uint16_t method) {
    Item it = file("a", 0, method);
    it.cextra = zip64_extra({usize, csize});
    it.lextra = zip64_extra({usize, csize});
    Built r = build({it});
    for (std::size_t base : {r.central[0]}) {
      put32(r.bytes, base + cUncompressed, 0xFFFFFFFFu);
      put32(r.bytes, base + cCompressed, 0xFFFFFFFFu);
    }
    put32(r.bytes, r.local[0] + lUncompressed, 0xFFFFFFFFu);
    put32(r.bytes, r.local[0] + lCompressed, 0xFFFFFFFFu);
    return std::make_pair(r.bytes, r.local[0]);
  };
  for (std::uint16_t method : {0, 8}) {
    for (std::uint64_t v : std::initializer_list<std::uint64_t>{0xFFFFFFFFull, 1ull << 32, 1ull << 63, kMax - 1, kMax}) {
      CAPTURE(method);
      CAPTURE(v);
      const auto [bytes, at] = huge(v, v, method);
      check_at(bytes, ZipVerdict::BadSizes, at, 0);
    }
  }
  // A deflate entry may declare a larger uncompressed size only through ZIP64 and a matching
  // local header; the compressed size then also has to be a ZIP64 value, which cannot fit.
  const auto [bytes, at] = huge(kMax, 0xFFFFFFFFull, 8);
  check_at(bytes, ZipVerdict::BadSizes, at, 0);
}

TEST_CASE("zip audit sizes", "[zip_audit]") {
  SECTION("stored sizes must agree") {
    Built r = build({file("a", 4)});
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 5);
    check_at(r.bytes, ZipVerdict::BadSizes, r.local[0], 0);
    r = build({file("a", 4)});
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 3);
    check(r.bytes, ZipVerdict::BadSizes);
  }
  SECTION("deflate with no compressed bytes") {
    const Built r = build({file("a", 0), file("b", 0, 8)});
    check_at(r.bytes, ZipVerdict::BadSizes, r.local[1], 1);
    Built q = build({file("b", 4, 8)});
    both32(q.bytes, q, 0, cCompressed, lCompressed, 0);
    check(q.bytes, ZipVerdict::BadSizes);
    // Nonzero uncompressed size does not rescue it.
    both32(q.bytes, q, 0, cUncompressed, lUncompressed, 100);
    check(q.bytes, ZipVerdict::BadSizes);
  }
  SECTION("deflate may expand and may claim a larger uncompressed size") {
    Built r = build({file("b", 4, 8)});
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 0xFFFFFFFEu);
    check(r.bytes, ZipVerdict::Ok);
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 0);
    check(r.bytes, ZipVerdict::Ok);
  }
  SECTION("data that runs past the buffer") {
    Built r = build({file("a", 4)});
    const std::uint64_t room = r.bytes.size() - r.data[0];  // data, directory, end record
    both32(r.bytes, r, 0, cCompressed, lCompressed, room);
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, room);
    check(r.bytes, ZipVerdict::Overlap);  // fits the buffer, so it runs into the directory
    both32(r.bytes, r, 0, cCompressed, lCompressed, room + 1);
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, room + 1);
    check_at(r.bytes, ZipVerdict::BadSizes, r.local[0], 0);
    both32(r.bytes, r, 0, cCompressed, lCompressed, 0xFFFFFFFEu);
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 0xFFFFFFFEu);
    check_at(r.bytes, ZipVerdict::BadSizes, r.local[0], 0);
  }
}

TEST_CASE("zip audit layout", "[zip_audit]") {
  SECTION("data in front of the first entry") {
    Item first = file("a");
    first.gap_before = 5;
    const Built r = build({first, file("b")});
    check_at(r.bytes, ZipVerdict::PrependedData, 5, 0);
    Opts o;
    Built junk = build({file("a")}, o);
    Bytes shifted(7, 0);
    shifted.insert(shifted.end(), junk.bytes.begin(), junk.bytes.end());
    check(shifted, ZipVerdict::CentralDirectoryBounds);  // offsets not adjusted: no longer line up
  }
  SECTION("an empty archive with a directory offset above zero") {
    Bytes b(5, 0);
    const Built e = build({});
    b.insert(b.end(), e.bytes.begin(), e.bytes.end());
    put32(b, 5 + eCdOffset, 5);
    check_at(b, ZipVerdict::PrependedData, 5, 0);
  }
  SECTION("gap between entries and before the directory") {
    Item second = file("b");
    second.gap_before = 7;
    Built r = build({file("a"), second});
    check_at(r.bytes, ZipVerdict::Gap, r.data[0] + 4, 1);
    Opts o;
    o.pad_before_cd = 1;
    r = build({file("a"), file("b")}, o);
    check_at(r.bytes, ZipVerdict::Gap, r.data[1] + 4, 1);
    r = build({dir("d/")}, o);
    check_at(r.bytes, ZipVerdict::Gap, r.data[0], 0);
  }
  SECTION("entry data into the next header, or into the directory") {
    Built r = build({file("a", 4), file("b", 4)});
    both32(r.bytes, r, 0, cCompressed, lCompressed, 5);  // one byte into entry b's header
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 5);
    check_at(r.bytes, ZipVerdict::Overlap, r.local[1], 1);
    r = build({file("a", 4)});
    both32(r.bytes, r, 0, cCompressed, lCompressed, 5);
    both32(r.bytes, r, 0, cUncompressed, lUncompressed, 5);
    check_at(r.bytes, ZipVerdict::Overlap, r.local[0], 0);
  }
  SECTION("offsets that do not ascend") {
    Built r = build({file("a"), file("b")});
    put32(r.bytes, r.central[0] + cOffset, r.local[1]);
    put32(r.bytes, r.central[1] + cOffset, r.local[0]);
    // Swapped: the first central entry now points at b's header, whose name differs.
    check(r.bytes, ZipVerdict::HeaderMismatch);
    // Same header twice: the names agree, the offsets do not ascend.
    r = build({file("a"), file("a")});
    put32(r.bytes, r.central[1] + cOffset, r.local[0]);
    check_at(r.bytes, ZipVerdict::OutOfOrder, 0, 1);
    // Central order reversed: the first central entry no longer sits at offset 0.
    Built q = build({file("a"), file("b")});
    Bytes c0(q.bytes.begin() + static_cast<long>(q.central[0]),
             q.bytes.begin() + static_cast<long>(q.central[1]));
    Bytes c1(q.bytes.begin() + static_cast<long>(q.central[1]),
             q.bytes.begin() + static_cast<long>(q.eocd));
    Bytes swapped(q.bytes.begin(), q.bytes.begin() + static_cast<long>(q.cd_off));
    swapped.insert(swapped.end(), c1.begin(), c1.end());
    swapped.insert(swapped.end(), c0.begin(), c0.end());
    swapped.insert(swapped.end(), q.bytes.begin() + static_cast<long>(q.eocd), q.bytes.end());
    check_at(swapped, ZipVerdict::PrependedData, q.local[1], 0);
    // Three entries listed a, c, b: c skips b's bytes (a gap is found before the order of b).
    Built t = build({file("a"), file("b"), file("c")});
    Bytes ca(t.bytes.begin() + static_cast<long>(t.central[0]), t.bytes.begin() + static_cast<long>(t.central[1]));
    Bytes cb(t.bytes.begin() + static_cast<long>(t.central[1]), t.bytes.begin() + static_cast<long>(t.central[2]));
    Bytes cc(t.bytes.begin() + static_cast<long>(t.central[2]), t.bytes.begin() + static_cast<long>(t.eocd));
    Bytes acb(t.bytes.begin(), t.bytes.begin() + static_cast<long>(t.cd_off));
    acb.insert(acb.end(), ca.begin(), ca.end());
    acb.insert(acb.end(), cc.begin(), cc.end());
    acb.insert(acb.end(), cb.begin(), cb.end());
    acb.insert(acb.end(), t.bytes.begin() + static_cast<long>(t.eocd), t.bytes.end());
    check_at(acb, ZipVerdict::Gap, t.data[0] + 4, 1);
  }
  SECTION("overlap bombs: shared and nested data") {
    // Two central entries that share one local header and data.
    Built r = build({file("a", 8), file("a", 8)});
    put32(r.bytes, r.central[1] + cOffset, r.local[0]);
    check_at(r.bytes, ZipVerdict::OutOfOrder, 0, 1);
    // The second local header placed inside the first entry's data.
    Built nest = build({file("a", 100), file("b", 4)});
    const std::size_t inner_off = 40;
    Built target = build({file("a", 100), file("b", 4)});
    std::memcpy(target.bytes.data() + inner_off, nest.bytes.data() + nest.local[1], 35);
    put32(target.bytes, target.central[1] + cOffset, inner_off);
    check_at(target.bytes, ZipVerdict::Overlap, inner_off, 1);
  }
}

TEST_CASE("zip audit ZIP64 end record", "[zip_audit]") {
  SECTION("optional and consistent: record and locator without a sentinel are refused") {
    Opts o;
    o.zip64 = 2;
    const Built r = build(basic_items(), o);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_locator);
  }
  SECTION("sentinel without a locator") {
    Built r = basic();
    put16(r.bytes, r.eocd + eEntriesDisk, 0xFFFF);
    put16(r.bytes, r.eocd + eEntriesTotal, 0xFFFF);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.eocd);
    r = basic();
    put32(r.bytes, r.eocd + eCdSize, 0xFFFFFFFFu);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.eocd);
    r = basic();
    put32(r.bytes, r.eocd + eCdOffset, 0xFFFFFFFFu);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.eocd);
  }
  SECTION("sentinel with a record that holds a value that fits the ordinary field") {
    Opts o;
    o.zip64 = 1;  // count field is 0xFFFF, the record says 3
    const Built r = build(basic_items(), o);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
  }
  SECTION("sentinel fields in the other two places") {
    Opts o;
    o.zip64 = 2;
    Built r = build(basic_items(), o);
    put32(r.bytes, r.eocd + eCdSize, 0xFFFFFFFFu);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = build(basic_items(), o);
    put32(r.bytes, r.eocd + eCdOffset, 0xFFFFFFFFu);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
  }
  SECTION("record and locator fields") {
    Opts o;
    o.zip64 = 1;
    auto fresh = [&] {
      Built r = build(basic_items(), o);
      put64(r.bytes, r.z64_record + 24, kMax);  // entries on this disk
      put64(r.bytes, r.z64_record + 32, kMax);  // entries in total, canonical for the sentinel
      return r;
    };
    // Canonical count that is far above max_entries.
    Built r = fresh();
    check_at(r.bytes, ZipVerdict::TooManyEntries, r.eocd, 0);
    ZipAuditLimits lim;
    lim.max_entries = kMax;
    check_at(r.bytes, ZipVerdict::CentralDirectoryBounds, r.eocd, 0, lim);
    // Disagreeing counts, disks, size and offset.
    r = fresh();
    put64(r.bytes, r.z64_record + 24, 1);
    check_at(r.bytes, ZipVerdict::MultiDisk, r.z64_record);
    r = fresh();
    put32(r.bytes, r.z64_record + 16, 1);
    check_at(r.bytes, ZipVerdict::MultiDisk, r.z64_record);
    r = fresh();
    put32(r.bytes, r.z64_record + 20, 1);
    check_at(r.bytes, ZipVerdict::MultiDisk, r.z64_record);
    r = fresh();
    put64(r.bytes, r.z64_record + 40, r.cd_size + 1);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = fresh();
    put64(r.bytes, r.z64_record + 40, kMax);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = fresh();
    put64(r.bytes, r.z64_record + 48, r.cd_off + 1);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = fresh();
    put64(r.bytes, r.z64_record + 4, 45);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = fresh();
    put64(r.bytes, r.z64_record + 4, kMax);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = fresh();
    r.bytes[r.z64_record] ^= 1;
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_record);
    r = fresh();
    put32(r.bytes, r.z64_locator + 4, 1);
    check_at(r.bytes, ZipVerdict::MultiDisk, r.z64_locator);
    r = fresh();
    put32(r.bytes, r.z64_locator + 16, 2);
    check_at(r.bytes, ZipVerdict::MultiDisk, r.z64_locator);
    r = fresh();
    put64(r.bytes, r.z64_locator + 8, r.z64_record + 1);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_locator);
    r = fresh();
    put64(r.bytes, r.z64_locator + 8, kMax);
    check_at(r.bytes, ZipVerdict::Zip64Mismatch, r.z64_locator);
  }
  SECTION("record missing in front of the locator") {
    Opts o;
    o.zip64 = 1;
    const Built r = build({file("a")}, o);
    // Drop the record but keep the locator and the end record; the locator now sits too close.
    Bytes b(r.bytes.begin(), r.bytes.begin() + static_cast<long>(r.z64_record));
    b.insert(b.end(), r.bytes.begin() + static_cast<long>(r.z64_locator), r.bytes.end());
    check(b, ZipVerdict::Zip64Mismatch);
  }
}

TEST_CASE("zip audit reaches the 65535 entry ZIP64 boundary", "[zip_audit]") {
  ZipAuditLimits lim;
  lim.max_entries = 70000;
  auto many = [](std::size_t n) {
    std::vector<Item> items;
    items.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      Item it;
      it.name = "n" + std::to_string(100000 + i);
      items.push_back(std::move(it));
    }
    return items;
  };
  SECTION("65534 entries need no ZIP64") {
    const Built r = build(many(65534));
    check(r.bytes, ZipVerdict::Ok, lim);
    REQUIRE(audit_zip(r.bytes.data(), r.bytes.size(), lim).entries.size() == 65534);
  }
  SECTION("65535 entries need it, and with it are accepted") {
    Opts o;
    o.zip64 = 1;
    const Built r = build(many(65535), o);
    const ZipAuditResult a = audit_zip(r.bytes.data(), r.bytes.size(), lim);
    REQUIRE(a.verdict == ZipVerdict::Ok);
    REQUIRE(a.entries.size() == 65535);
    REQUIRE(a.entries.back().name == "n165534");
    ZipAuditLimits small;
    check(r.bytes, ZipVerdict::TooManyEntries, small);
    // Without the record the sentinel count has nothing behind it.
    const Built bare = build(many(65535));
    Bytes b = bare.bytes;
    put16(b, bare.eocd + eEntriesDisk, 0xFFFF);
    put16(b, bare.eocd + eEntriesTotal, 0xFFFF);
    check(b, ZipVerdict::Zip64Mismatch, lim);
    // The record with a count that is one off.
    Bytes c = r.bytes;
    put64(c, r.z64_record + 24, 65534);
    put64(c, r.z64_record + 32, 65534);
    check(c, ZipVerdict::Zip64Mismatch, lim);
  }
}

TEST_CASE("zip audit reference archives from tools/zip_audit_ref.py", "[zip_audit]") {
  for (const RefCase& c : kRefCases) {
    CAPTURE(c.name);
    const Bytes b = from_hex(c.hex);
    const ZipAuditResult r = audit_zip(b.data(), b.size());
    INFO("got " << to_string(r.verdict) << " at " << r.offset << " index " << r.entry_index);
    REQUIRE(std::string(to_string(r.verdict)) == c.verdict);
    REQUIRE(r.offset == c.offset);
    REQUIRE(r.entry_index == c.index);
    if (r.verdict == ZipVerdict::Ok) {
      // The Python builder's valid archives hold: a.txt (stored), d/ (directory), d/b.bin (deflate).
      const bool basicish = std::string(c.name) != "valid_empty";
      REQUIRE(r.entries.size() == (basicish ? 2u : 0u));
      REQUIRE(r.directory_count == (basicish ? 1u : 0u));
      if (basicish) {
        REQUIRE(r.entries[0].name == "a.txt");
        REQUIRE(r.entries[0].compressed_bytes == 6);
        REQUIRE(r.entries[1].name == "dir/b.bin");
        REQUIRE(r.entries[1].method == 8);
        REQUIRE(r.entries[1].uncompressed_bytes == 200);
        REQUIRE(r.entries[0].crc32 == 0x363A3020u);
      }
    }
  }
}

namespace {

// Bytes whose single-bit changes may still give an Ok archive; see docs/zip-audit.md.
std::vector<bool> exempt_bytes(const Built& r, const std::vector<Item>& items) {
  std::vector<bool> ex(r.bytes.size(), false);
  auto range = [&](std::size_t from, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) ex[from + i] = true;
  };
  for (std::size_t i = 0; i < items.size(); ++i) {
    range(r.data[i], items[i].data.size());                      // file data is not read
    range(r.local[i] + lNeeded, 2);                              // version needed
    range(r.local[i] + lName + items[i].name.size(), items[i].lextra.size());
    range(r.central[i] + cMadeBy, 4);                            // version made by, needed
    range(r.central[i] + cInternal, 2);                          // internal attributes
    range(r.central[i] + cExternal, 4);                          // DOS bits, permissions, mode
    range(r.central[i] + cName + items[i].name.size(), items[i].cextra.size());
    range(r.central[i] + cName + items[i].name.size() + items[i].cextra.size(),
          items[i].comment.size());
  }
  return ex;
}

}  // namespace

TEST_CASE("zip audit mutation: every structural byte matters", "[zip_audit]") {
  Item a = file("a.txt", 6);
  a.cextra = block(0xCAFE, 6);
  a.lextra = block(0xBEEF, 3);
  a.comment = "note";
  Item d = dir("d/");
  Item b = file("d/b.bin", 20, 8);
  b.flags = 0x0802;
  const std::vector<Item> items = {a, d, b};
  Opts o;
  o.comment = "archive comment";
  const Built r = build(items, o);
  REQUIRE(audit_zip(r.bytes.data(), r.bytes.size()).verdict == ZipVerdict::Ok);
  std::vector<bool> ex = exempt_bytes(r, items);
  for (std::size_t i = 0; i < 15; ++i) ex[r.eocd + 22 + i] = true;  // archive comment text

  std::size_t checked = 0;
  for (std::size_t at = 0; at < r.bytes.size(); ++at) {
    for (int bit = 0; bit < 8; ++bit) {
      Bytes m = r.bytes;
      m[at] ^= static_cast<std::uint8_t>(1u << bit);
      const ZipVerdict v = audit_zip(m.data(), m.size()).verdict;  // must not crash either way
      if (ex[at]) continue;
      ++checked;
      INFO("byte " << at << " bit " << bit << " gave " << to_string(v));
      REQUIRE(v != ZipVerdict::Ok);
    }
  }
  REQUIRE(checked > 200 * 8);

  // Cutting the archive anywhere, or adding any byte after it, is never accepted.
  for (std::size_t n = 0; n < r.bytes.size(); ++n) {
    REQUIRE(audit_zip(r.bytes.data(), n).verdict != ZipVerdict::Ok);
  }
  for (int extra : {0, 0x50, 0xFF}) {
    Bytes m = r.bytes;
    m.push_back(static_cast<std::uint8_t>(extra));
    REQUIRE(audit_zip(m.data(), m.size()).verdict != ZipVerdict::Ok);
  }
}

TEST_CASE("zip audit mutation: the reference archive from Python", "[zip_audit]") {
  const Bytes ok = from_hex(kRefCases[0].hex);
  REQUIRE(audit_zip(ok.data(), ok.size()).verdict == ZipVerdict::Ok);
  // The Python archive has no extras, comments or odd attributes, so only data and the
  // permission and version fields are free. Everything else must reject a single bit flip.
  const std::size_t data_a = 30 + 5;
  const std::size_t eocd = ok.size() - 22;
  REQUIRE(get32(ok, eocd) == 0x06054B50u);
  const std::size_t cd = get32(ok, eocd + eCdOffset);
  std::vector<bool> ex(ok.size(), false);
  for (std::size_t i = 0; i < 6; ++i) ex[data_a + i] = true;
  // Walk the three local entries and three central entries.
  std::size_t pos = 0;
  for (int e = 0; e < 3; ++e) {
    const std::size_t nl = get16(ok, pos + lNameLen);
    const std::size_t cs = get32(ok, pos + lCompressed);
    ex[pos + lNeeded] = ex[pos + lNeeded + 1] = true;
    for (std::size_t i = 0; i < cs; ++i) ex[pos + lName + nl + i] = true;
    pos += lName + nl + cs;
  }
  REQUIRE(pos == cd);
  for (int e = 0; e < 3; ++e) {
    const std::size_t nl = get16(ok, pos + cNameLen);
    for (std::size_t i : {std::size_t{cMadeBy}, std::size_t{cMadeBy + 1}, std::size_t{cNeeded},
                          std::size_t{cNeeded + 1}, std::size_t{cInternal},
                          std::size_t{cInternal + 1}, std::size_t{cExternal},
                          std::size_t{cExternal + 1}, std::size_t{cExternal + 2},
                          std::size_t{cExternal + 3}}) {
      ex[pos + i] = true;
    }
    pos += cName + nl;
  }
  REQUIRE(pos == eocd);
  for (std::size_t at = 0; at < ok.size(); ++at) {
    for (int bit = 0; bit < 8; ++bit) {
      Bytes m = ok;
      m[at] ^= static_cast<std::uint8_t>(1u << bit);
      const ZipVerdict v = audit_zip(m.data(), m.size()).verdict;
      if (ex[at]) continue;
      INFO("byte " << at << " bit " << bit << " gave " << to_string(v));
      REQUIRE(v != ZipVerdict::Ok);
    }
  }
}
