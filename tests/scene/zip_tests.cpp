// ZIP (stored) writer/reader round trip and hostile input rejection.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "zip_store.h"

using namespace lumora::scene;

namespace {
FileMap sample() {
  FileMap f;
  f["a.txt"] = {'h', 'i'};
  f["dir/b.bin"] = std::vector<std::uint8_t>(5000, 7);
  f["empty"] = {};
  return f;
}
}  // namespace

TEST_CASE("zip round trip is exact and reproducible", "[zip]") {
  const auto z = write_zip_store(sample());
  CHECK(z == write_zip_store(sample()));
  const auto r = read_zip_store(z.data(), z.size());
  REQUIRE(r.ok);
  CHECK(r.files == sample());
}

TEST_CASE("zip reader rejects hostile input", "[zip]") {
  const auto z = write_zip_store(sample());
  CHECK_FALSE(read_zip_store(z.data(), 10).ok);
  // Every truncation must fail cleanly, never crash.
  for (std::size_t n = 0; n < z.size(); n += 7) (void)read_zip_store(z.data(), n);
  // Single byte flips anywhere must never crash; flips in data or crc must be caught.
  for (std::size_t i = 0; i < z.size(); i += 3) {
    auto c = z;
    c[i] ^= 0x55;
    (void)read_zip_store(c.data(), c.size());
  }
  auto c = z;
  c[30 + 5] ^= 0xFF;  // first entry name or data byte region
  CHECK_FALSE(read_zip_store(c.data(), c.size()).ok);
  CHECK_FALSE(read_zip_store(z.data(), z.size(), 2).ok);          // entry-count cap
  CHECK_FALSE(read_zip_store(z.data(), z.size(), 100, 100).ok);   // size cap
}

TEST_CASE("zip reader rejects unsupported features", "[zip]") {
  auto z = write_zip_store(sample());
  // Central directory entry: method at +10 from its signature. Find the first central header.
  std::size_t cd = 0;
  for (std::size_t i = 0; i + 4 < z.size(); ++i)
    if (z[i] == 0x50 && z[i + 1] == 0x4b && z[i + 2] == 0x01 && z[i + 3] == 0x02) { cd = i; break; }
  REQUIRE(cd != 0);
  auto m = z; m[cd + 10] = 8;
  CHECK_FALSE(read_zip_store(m.data(), m.size()).ok);  // method 8 with stored bytes is not valid deflate data
  auto e = z; e[cd + 8] |= 1;
  CHECK_FALSE(read_zip_store(e.data(), e.size()).ok);
}

TEST_CASE("deflated zip round trip, bomb guard and hostile input", "[zip]") {
  FileMap f = sample();
  std::string text;
  for (int i = 0; i < 400; ++i) text += "parallax layer sky hills aurora ";
  f["big.txt"] = std::vector<std::uint8_t>(text.begin(), text.end());
  const auto stored = write_zip(f, 0);
  for (int level : {1, 6}) {
    const auto z = write_zip(f, level);
    CHECK(z.size() < stored.size());
    CHECK(z == write_zip(f, level));
    const auto r = read_zip_store(z.data(), z.size());
    REQUIRE(r.ok);
    CHECK(r.files == f);
    for (std::size_t n = 0; n < z.size(); n += 11) (void)read_zip_store(z.data(), n);
    for (std::size_t i = 0; i < z.size(); i += 5) {
      auto c = z;
      c[i] ^= 0xA5;
      (void)read_zip_store(c.data(), c.size());
    }
  }
  // A highly compressible entry is stored rather than deflated past the ratio guard, and still loads.
  FileMap zeros;
  zeros["z.bin"] = std::vector<std::uint8_t>(1 << 20, 0);
  const auto zz = write_zip(zeros, 6);
  const auto zr = read_zip_store(zz.data(), zz.size());
  REQUIRE(zr.ok);
  CHECK(zr.files == zeros);
  // A forged deflate entry that declares a huge size is refused by the ratio guard.
  auto z = write_zip(f, 6);
  std::size_t cd = 0;
  for (std::size_t i = 0; i + 4 < z.size(); ++i)
    if (z[i] == 0x50 && z[i + 1] == 0x4b && z[i + 2] == 0x01 && z[i + 3] == 0x02) { cd = i; break; }
  REQUIRE(cd != 0);
  z[cd + 27] = 0x40;  // uncompressed size field high byte: about 1 GB
  CHECK_FALSE(read_zip_store(z.data(), z.size()).ok);
}

TEST_CASE("deflated entries with short distances survive the zip round trip", "[zip]") {
  // Regression: 13 zero bytes deflate to a length/distance pair with distance 1 (no distance extra bits).
  for (std::size_t n : {13u, 14u, 40u, 300u}) {
    FileMap f;
    f["z.bin"] = std::vector<std::uint8_t>(n, 0);
    f["pat.bin"] = {};
    for (std::size_t i = 0; i < n; ++i) f["pat.bin"].push_back(static_cast<std::uint8_t>("ab"[i % 2]));
    for (int level : {1, 6}) {
      const auto z = write_zip(f, level);
      const auto r = read_zip_store(z.data(), z.size());
      REQUIRE(r.ok);
      CHECK(r.files == f);
    }
  }
}

TEST_CASE("zip reader applies the container audit and the entry policy", "[zip]") {
  const auto good = write_zip_store(sample());
  CHECK(read_zip_store(good.data(), good.size()).ok);

  auto appended = good;
  appended.push_back(0);
  const auto ra = read_zip_store(appended.data(), appended.size());
  CHECK_FALSE(ra.ok);
  CHECK(ra.audit_verdict == "TrailingData");

  auto prepended = good;
  prepended.insert(prepended.begin(), 4, 0x41);
  CHECK_FALSE(read_zip_store(prepended.data(), prepended.size()).ok);

  FileMap trav;
  trav["../evil.txt"] = {'x'};
  const auto zt = write_zip_store(trav);
  const auto rt = read_zip_store(zt.data(), zt.size());
  CHECK_FALSE(rt.ok);
  CHECK(rt.audit_verdict == "BadName");

  FileMap reserved;
  reserved["NUL.txt"] = {'x'};
  const auto zr = write_zip_store(reserved);
  CHECK_FALSE(read_zip_store(zr.data(), zr.size()).ok);

  FileMap fold;
  fold["Readme.txt"] = {'a'};
  fold["readme.txt"] = {'b'};
  const auto zf = write_zip_store(fold);
  const auto rf = read_zip_store(zf.data(), zf.size());
  CHECK_FALSE(rf.ok);
  CHECK(rf.audit_verdict == "CaseFoldCollision");

  CHECK_FALSE(read_zip_store(good.data(), good.size(), 2).ok);
}
