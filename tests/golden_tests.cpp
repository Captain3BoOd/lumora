// Unit tests for the golden-image helper in tests/golden.h. They use only that header and Catch2,
// need no GPU, and write files only under a scratch folder inside the build directory. They never
// read or write the real baselines under tests/golden/. One behavior per test case.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

#include "golden.h"

namespace fs = std::filesystem;

namespace {

// Fresh, empty scratch folder under the build directory (LUMORA_GOLDEN_OUT_DIR is a build-dir path).
fs::path scratch(const std::string& name) {
  fs::path dir = fs::path(LUMORA_GOLDEN_OUT_DIR) / "unit-tests" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

void write_bytes(const fs::path& p, const std::string& bytes) {
  std::ofstream f(p, std::ios::binary);
  f << bytes;
}

bool bless_mode() {
  const char* b = std::getenv("LUMORA_BLESS_GOLDEN");
  return b && *b == '1';
}

golden::Image pixels(std::initializer_list<std::uint8_t> rgb, unsigned w, unsigned h) {
  return golden::Image{w, h, std::vector<std::uint8_t>(rgb)};
}

}  // namespace

TEST_CASE("golden from_rgba drops the alpha channel") {
  const std::uint8_t rgba[] = {1, 2, 3, 255, 4, 5, 6, 0};
  golden::Image im = golden::from_rgba(rgba, 2, 1);
  CHECK(im.w == 2);
  CHECK(im.h == 1);
  REQUIRE(im.rgb.size() == 6);
  CHECK(im.rgb == std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("golden compare accepts identical images at tolerance zero") {
  golden::Image a = pixels({10, 20, 30, 40, 50, 60}, 2, 1);
  golden::Result r = golden::compare(a, a, 0);
  CHECK(r.ok);
  CHECK(r.max_delta == 0);
  CHECK(r.bad_pixels == 0);
  CHECK(r.message == "match");
}

TEST_CASE("golden compare rejects a width or height mismatch") {
  golden::Image a = pixels({0, 0, 0, 0, 0, 0}, 2, 1);
  golden::Image wide = pixels({0, 0, 0}, 1, 1);
  golden::Image tall = pixels({0, 0, 0, 0, 0, 0}, 1, 2);
  golden::Result rw = golden::compare(a, wide, 255);
  CHECK_FALSE(rw.ok);
  CHECK(rw.message == "size mismatch");
  CHECK_FALSE(golden::compare(a, tall, 255).ok);
}

TEST_CASE("golden compare leaves the diff image untouched on a size mismatch") {
  golden::Image a = pixels({0, 0, 0, 0, 0, 0}, 2, 1);
  golden::Image b = pixels({0, 0, 0}, 1, 1);
  golden::Image diff = pixels({9, 9, 9}, 1, 1);
  golden::compare(a, b, 0, &diff);
  CHECK(diff.w == 1);
  CHECK(diff.h == 1);
  CHECK(diff.rgb == std::vector<std::uint8_t>{9, 9, 9});
}

TEST_CASE("golden compare passes when the delta equals the tolerance") {
  golden::Image a = pixels({100, 100, 100}, 1, 1);
  golden::Image b = pixels({103, 100, 100}, 1, 1);
  CHECK(golden::compare(a, b, 3).ok);
}

TEST_CASE("golden compare fails when the delta exceeds the tolerance by one") {
  golden::Image a = pixels({100, 100, 100}, 1, 1);
  golden::Image b = pixels({104, 100, 100}, 1, 1);
  golden::Result r = golden::compare(a, b, 3);
  CHECK_FALSE(r.ok);
  CHECK(r.message == "pixels differ beyond tolerance");
}

TEST_CASE("golden compare uses the worst channel of a pixel") {
  golden::Image a = pixels({0, 0, 0}, 1, 1);
  golden::Image b = pixels({1, 2, 5}, 1, 1);
  golden::Result r = golden::compare(a, b, 4);
  CHECK_FALSE(r.ok);
  CHECK(r.max_delta == 5);
  CHECK(r.bad_pixels == 1);
}

TEST_CASE("golden compare is symmetric in its two images") {
  golden::Image a = pixels({10, 10, 10}, 1, 1);
  golden::Image b = pixels({20, 10, 10}, 1, 1);
  CHECK(golden::compare(a, b, 9).ok == golden::compare(b, a, 9).ok);
  CHECK(golden::compare(a, b, 9).max_delta == golden::compare(b, a, 9).max_delta);
}

TEST_CASE("golden compare counts each bad pixel once and reports the largest delta") {
  golden::Image a = pixels({0, 0, 0, 0, 0, 0, 0, 0, 0}, 3, 1);
  golden::Image b = pixels({9, 9, 9, 0, 0, 0, 7, 0, 0}, 3, 1);
  golden::Result r = golden::compare(a, b, 2);
  CHECK(r.bad_pixels == 2);
  CHECK(r.max_delta == 9);
}

TEST_CASE("golden compare marks bad pixels 255 in the red channel only") {
  golden::Image a = pixels({0, 0, 0, 0, 0, 0}, 2, 1);
  golden::Image b = pixels({0, 0, 0, 50, 0, 0}, 2, 1);
  golden::Image diff;
  golden::compare(a, b, 10, &diff);
  CHECK(diff.w == 2);
  CHECK(diff.h == 1);
  CHECK(diff.rgb == std::vector<std::uint8_t>{0, 0, 0, 255, 0, 0});
}

TEST_CASE("golden compare leaves the diff image all zero when images match") {
  golden::Image a = pixels({5, 6, 7}, 1, 1);
  golden::Image diff;
  golden::compare(a, a, 0, &diff);
  CHECK(diff.rgb == std::vector<std::uint8_t>{0, 0, 0});
}

TEST_CASE("golden ppm round trip keeps size and pixels") {
  fs::path dir = scratch("ppm-round-trip");
  golden::Image a = pixels({1, 2, 3, 4, 5, 6}, 2, 1);
  REQUIRE(golden::write_ppm(dir / "a.ppm", a));
  golden::Image b;
  REQUIRE(golden::read_ppm(dir / "a.ppm", b));
  CHECK(b.w == 2);
  CHECK(b.h == 1);
  CHECK(b.rgb == a.rgb);
}

TEST_CASE("golden write_ppm creates missing parent folders") {
  fs::path dir = scratch("write-creates-folders");
  CHECK(golden::write_ppm(dir / "x" / "y" / "a.ppm", pixels({0, 0, 0}, 1, 1)));
  CHECK(fs::exists(dir / "x" / "y" / "a.ppm"));
}

TEST_CASE("golden read_ppm fails for a missing file") {
  fs::path dir = scratch("read-missing");
  golden::Image im;
  CHECK_FALSE(golden::read_ppm(dir / "nope.ppm", im));
}

TEST_CASE("golden read_ppm rejects a wrong magic") {
  fs::path dir = scratch("read-bad-magic");
  write_bytes(dir / "a.ppm", std::string("P5\n1 1\n255\n") + "abc");
  golden::Image im;
  CHECK_FALSE(golden::read_ppm(dir / "a.ppm", im));
}

TEST_CASE("golden read_ppm rejects a maxval other than 255") {
  fs::path dir = scratch("read-bad-maxval");
  write_bytes(dir / "a.ppm", std::string("P6\n1 1\n65535\n") + "abc");
  golden::Image im;
  CHECK_FALSE(golden::read_ppm(dir / "a.ppm", im));
}

TEST_CASE("golden read_ppm rejects a zero width or height") {
  fs::path dir = scratch("read-zero-size");
  write_bytes(dir / "w0.ppm", "P6\n0 1\n255\n");
  write_bytes(dir / "h0.ppm", "P6\n1 0\n255\n");
  golden::Image im;
  CHECK_FALSE(golden::read_ppm(dir / "w0.ppm", im));
  CHECK_FALSE(golden::read_ppm(dir / "h0.ppm", im));
}

TEST_CASE("golden read_ppm rejects truncated pixel data") {
  fs::path dir = scratch("read-truncated");
  write_bytes(dir / "a.ppm", std::string("P6\n2 1\n255\n") + "abcde");  // needs 6 bytes
  golden::Image im;
  CHECK_FALSE(golden::read_ppm(dir / "a.ppm", im));
}

TEST_CASE("golden read_ppm does not handle comments in the header") {
  fs::path dir = scratch("read-comment");
  write_bytes(dir / "a.ppm", std::string("P6\n# comment\n1 1\n255\n") + "abc");
  golden::Image im;
  CHECK_FALSE(golden::read_ppm(dir / "a.ppm", im));
}

TEST_CASE("golden check reports a missing baseline and still writes the actual image") {
  if (bless_mode()) SKIP("LUMORA_BLESS_GOLDEN=1 writes the baseline instead");
  fs::path dir = scratch("check-missing");
  golden::Image a = pixels({1, 2, 3}, 1, 1);
  golden::Result r = golden::check("shot", a, (dir / "golden").string(), (dir / "out").string(), 0);
  CHECK_FALSE(r.ok);
  CHECK(r.message.rfind("baseline missing", 0) == 0);
  CHECK(fs::exists(dir / "out" / "shot.actual.ppm"));
  CHECK_FALSE(fs::exists(dir / "out" / "shot.diff.ppm"));
}

TEST_CASE("golden check passes against a matching baseline without a diff image") {
  if (bless_mode()) SKIP("LUMORA_BLESS_GOLDEN=1 writes the baseline instead");
  fs::path dir = scratch("check-match");
  golden::Image a = pixels({1, 2, 3, 4, 5, 6}, 2, 1);
  REQUIRE(golden::write_ppm(dir / "golden" / "shot.ppm", a));
  golden::Result r = golden::check("shot", a, (dir / "golden").string(), (dir / "out").string(), 0);
  CHECK(r.ok);
  CHECK(fs::exists(dir / "out" / "shot.actual.ppm"));
  CHECK_FALSE(fs::exists(dir / "out" / "shot.diff.ppm"));
}

TEST_CASE("golden check writes a diff image when the pixels differ beyond tolerance") {
  if (bless_mode()) SKIP("LUMORA_BLESS_GOLDEN=1 writes the baseline instead");
  fs::path dir = scratch("check-mismatch");
  golden::Image base = pixels({0, 0, 0}, 1, 1);
  golden::Image actual = pixels({40, 0, 0}, 1, 1);
  REQUIRE(golden::write_ppm(dir / "golden" / "shot.ppm", base));
  golden::Result r = golden::check("shot", actual, (dir / "golden").string(), (dir / "out").string(), 5);
  CHECK_FALSE(r.ok);
  CHECK(fs::exists(dir / "out" / "shot.diff.ppm"));
}

TEST_CASE("golden check accepts a difference within tolerance") {
  if (bless_mode()) SKIP("LUMORA_BLESS_GOLDEN=1 writes the baseline instead");
  fs::path dir = scratch("check-within-tolerance");
  REQUIRE(golden::write_ppm(dir / "golden" / "shot.ppm", pixels({0, 0, 0}, 1, 1)));
  golden::Result r =
      golden::check("shot", pixels({2, 0, 0}, 1, 1), (dir / "golden").string(), (dir / "out").string(), 2);
  CHECK(r.ok);
}
