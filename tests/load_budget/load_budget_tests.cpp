// Tests for core/package/load_budget. Evidence label: unit tests only.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/load_budget_ref.py, an independent
// Python implementation. Regenerate it with: python3 tools/load_budget_ref.py
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "load_budget.h"

using namespace lumora::package;

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint32_t kMax32 = std::numeric_limits<std::uint32_t>::max();

// BEGIN GENERATED REFERENCE (tools/load_budget_ref.py)
struct RefLimits {
  std::uint64_t max_textures;
  std::uint32_t dim;
  std::uint64_t tex_each, tex_total, scr_each, scr_total, nodes, aud_each, aud_total, aud_ms;
};
struct RefTex {
  std::uint32_t w, h;
  TextureFormat fmt;
  std::uint32_t mips, layers;
};
struct RefAudio {
  std::uint64_t bytes, ms;
};
struct RefScene {
  const char* name;
  std::vector<RefTex> tex;
  std::vector<std::uint64_t> scripts;
  std::uint64_t nodes;
  std::vector<RefAudio> audio;
  RefLimits lim;
  BudgetVerdict verdict;
  std::size_t index;
  std::uint64_t tex_bytes, script_bytes, audio_bytes;
};
struct RefTexSize {
  RefTex t;
  int status;  // 0 ok, 1 invalid, 2 overflow
  std::uint64_t bytes;
};
const RefLimits kRefDefaults = {1024ull, 8192, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull};
const RefScene kRefScenes[] = {
    {"textures 0 limit 0", {}, {}, 0ull, {}, {0ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"textures 1 limit 0", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {0ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyTextures, 0, 0ull, 0ull, 0ull},
    {"textures 0 limit 1", {}, {}, 0ull, {}, {1ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"textures 1 limit 1", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"textures 2 limit 1", {{1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyTextures, 0, 0ull, 0ull, 0ull},
    {"textures 2 limit 3", {{1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {3ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 2ull, 0ull, 0ull},
    {"textures 3 limit 3", {{1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {3ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 3ull, 0ull, 0ull},
    {"textures 4 limit 3", {{1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {3ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyTextures, 0, 0ull, 0ull, 0ull},
    {"nodes 0 limit 0", {}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 0ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"nodes 1 limit 0", {}, {}, 1ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 0ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 0ull, 0ull, 0ull},
    {"nodes 18446744073709551615 limit 0", {}, {}, 18446744073709551615ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 0ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 0ull, 0ull, 0ull},
    {"nodes 0 limit 1", {}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 1ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"nodes 1 limit 1", {}, {}, 1ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 1ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"nodes 2 limit 1", {}, {}, 2ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 1ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 0ull, 0ull, 0ull},
    {"nodes 18446744073709551615 limit 1", {}, {}, 18446744073709551615ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 1ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 0ull, 0ull, 0ull},
    {"nodes 99999 limit 100000", {}, {}, 99999ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"nodes 100000 limit 100000", {}, {}, 100000ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"nodes 100001 limit 100000", {}, {}, 100001ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 0ull, 0ull, 0ull},
    {"nodes 18446744073709551615 limit 100000", {}, {}, 18446744073709551615ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 0ull, 0ull, 0ull},
    {"nodes 18446744073709551614 limit 18446744073709551615", {}, {}, 18446744073709551614ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 18446744073709551615ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"nodes 18446744073709551615 limit 18446744073709551615", {}, {}, 18446744073709551615ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 18446744073709551615ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"dim width 0 limit 0", {{0u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"dim height 0 limit 0", {{1u, 0u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"dim width 1 limit 0", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim height 1 limit 0", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim width 4294967295 limit 0", {{4294967295u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim height 4294967295 limit 0", {{1u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim width 0 limit 1", {{0u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"dim height 0 limit 1", {{1u, 0u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"dim width 1 limit 1", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"dim height 1 limit 1", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"dim width 2 limit 1", {{2u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim height 2 limit 1", {{1u, 2u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim width 4294967295 limit 1", {{4294967295u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim height 4294967295 limit 1", {{1u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 1u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim width 8191 limit 8192", {{8191u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8191ull, 0ull, 0ull},
    {"dim height 8191 limit 8192", {{1u, 8191u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8191ull, 0ull, 0ull},
    {"dim width 8192 limit 8192", {{8192u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8192ull, 0ull, 0ull},
    {"dim height 8192 limit 8192", {{1u, 8192u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8192ull, 0ull, 0ull},
    {"dim width 8193 limit 8192", {{8193u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim height 8193 limit 8192", {{1u, 8193u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim width 4294967295 limit 8192", {{4294967295u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim height 4294967295 limit 8192", {{1u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dim width 4294967294 limit 4294967295", {{4294967294u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 4294967294ull, 0ull, 0ull},
    {"dim height 4294967294 limit 4294967295", {{1u, 4294967294u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 4294967294ull, 0ull, 0ull},
    {"dim width 4294967295 limit 4294967295", {{4294967295u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 4294967295ull, 0ull, 0ull},
    {"dim height 4294967295 limit 4294967295", {{1u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 4294967295ull, 0ull, 0ull},
    {"tex each 1 limit 0", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 0ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 63 limit 0", {{63u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 0ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 64 limit 0", {{64u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 0ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 65 limit 0", {{65u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 0ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 1 limit 63", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 63ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"tex each 63 limit 63", {{63u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 63ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 63ull, 0ull, 0ull},
    {"tex each 64 limit 63", {{64u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 63ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 65 limit 63", {{65u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 63ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 1 limit 64", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 64ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"tex each 63 limit 64", {{63u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 64ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 63ull, 0ull, 0ull},
    {"tex each 64 limit 64", {{64u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 64ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 64ull, 0ull, 0ull},
    {"tex each 65 limit 64", {{65u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 64ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex each 1 limit 65", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 65ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"tex each 63 limit 65", {{63u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 65ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 63ull, 0ull, 0ull},
    {"tex each 64 limit 65", {{64u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 65ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 64ull, 0ull, 0ull},
    {"tex each 65 limit 65", {{65u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 65ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 65ull, 0ull, 0ull},
    {"tex each 1 limit 18446744073709551615", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"tex each 63 limit 18446744073709551615", {{63u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 63ull, 0ull, 0ull},
    {"tex each 64 limit 18446744073709551615", {{64u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 64ull, 0ull, 0ull},
    {"tex each 65 limit 18446744073709551615", {{65u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 65ull, 0ull, 0ull},
    {"tex each at max", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 18446744065119617025ull, 0ull, 0ull},
    {"tex each max minus", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744065119617023ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"tex total 0 limit 0", {}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 0ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"tex total 1 limit 0", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 0ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureBudget, 0, 0ull, 0ull, 0ull},
    {"tex total 30 limit 0", {{15u, 1u, TextureFormat::R8, 1u, 1u}, {15u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 0ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureBudget, 0, 0ull, 0ull, 0ull},
    {"tex total 9 limit 10", {{5u, 1u, TextureFormat::R8, 1u, 1u}, {4u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 10ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 9ull, 0ull, 0ull},
    {"tex total 10 limit 10", {{5u, 1u, TextureFormat::R8, 1u, 1u}, {5u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 10ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 10ull, 0ull, 0ull},
    {"tex total 11 limit 10", {{6u, 1u, TextureFormat::R8, 1u, 1u}, {5u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 10ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureBudget, 1, 6ull, 0ull, 0ull},
    {"tex total 30 limit 10", {{15u, 1u, TextureFormat::R8, 1u, 1u}, {15u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 10ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureBudget, 0, 0ull, 0ull, 0ull},
    {"tex total 30 limit 18446744073709551615", {{15u, 1u, TextureFormat::R8, 1u, 1u}, {15u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 30ull, 0ull, 0ull},
    {"tex total overflow second", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}, {4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 1, 18446744065119617025ull, 0ull, 0ull},
    {"tex total overflow third", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}, {4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}, {4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 1, 18446744065119617025ull, 0ull, 0ull},
    {"tex total exact max", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}, {4294967295u, 1u, TextureFormat::R8, 1u, 2u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 18446744073709551615ull, 0ull, 0ull},
    {"tex total max plus one", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}, {4294967295u, 1u, TextureFormat::R8, 1u, 2u}, {1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 2, 18446744073709551615ull, 0ull, 0ull},
    {"script each 0 limit 0", {}, {0ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 0ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"script each 1 limit 0", {}, {1ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 0ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptTooLarge, 0, 0ull, 0ull, 0ull},
    {"script each 18446744073709551615 limit 0", {}, {18446744073709551615ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 0ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptTooLarge, 0, 0ull, 0ull, 0ull},
    {"script each 99 limit 100", {}, {99ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 100ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 99ull, 0ull},
    {"script each 100 limit 100", {}, {100ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 100ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 100ull, 0ull},
    {"script each 101 limit 100", {}, {101ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 100ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptTooLarge, 0, 0ull, 0ull, 0ull},
    {"script each 18446744073709551615 limit 100", {}, {18446744073709551615ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 100ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptTooLarge, 0, 0ull, 0ull, 0ull},
    {"script each 18446744073709551614 limit 18446744073709551615", {}, {18446744073709551614ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 18446744073709551614ull, 0ull},
    {"script each 18446744073709551615 limit 18446744073709551615", {}, {18446744073709551615ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 18446744073709551615ull, 0ull},
    {"script total 0 limit 0", {}, {0ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 0ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"script total 1 limit 0", {}, {1ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 0ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptBudget, 0, 0ull, 0ull, 0ull},
    {"script total 30 limit 0", {}, {15ull, 15ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 0ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptBudget, 0, 0ull, 0ull, 0ull},
    {"script total 9 limit 10", {}, {5ull, 4ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 10ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 9ull, 0ull},
    {"script total 10 limit 10", {}, {5ull, 5ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 10ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 10ull, 0ull},
    {"script total 11 limit 10", {}, {6ull, 5ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 10ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptBudget, 1, 0ull, 6ull, 0ull},
    {"script total 30 limit 10", {}, {15ull, 15ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 10ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptBudget, 0, 0ull, 0ull, 0ull},
    {"script total 30 limit 18446744073709551615", {}, {15ull, 15ull}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 18446744073709551615ull, 18446744073709551615ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 30ull, 0ull},
    {"script total overflow", {}, {18446744073709551615ull, 1ull}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 1, 0ull, 18446744073709551615ull, 0ull},
    {"script total overflow late", {}, {5ull, 18446744073709551610ull, 1ull, 0ull}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 2, 0ull, 18446744073709551615ull, 0ull},
    {"script total exact max", {}, {18446744073709551610ull, 5ull}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 0ull, 18446744073709551615ull, 0ull},
    {"audio each 0 limit 0", {}, {}, 0ull, {{0ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 0ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"audio each 1 limit 0", {}, {}, 0ull, {{1ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 0ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::AudioTooLarge, 0, 0ull, 0ull, 0ull},
    {"audio each 18446744073709551615 limit 0", {}, {}, 0ull, {{18446744073709551615ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 0ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::AudioTooLarge, 0, 0ull, 0ull, 0ull},
    {"audio each 99 limit 100", {}, {}, 0ull, {{99ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 100ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 99ull},
    {"audio each 100 limit 100", {}, {}, 0ull, {{100ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 100ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 100ull},
    {"audio each 101 limit 100", {}, {}, 0ull, {{101ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 100ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::AudioTooLarge, 0, 0ull, 0ull, 0ull},
    {"audio each 18446744073709551615 limit 100", {}, {}, 0ull, {{18446744073709551615ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 100ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::AudioTooLarge, 0, 0ull, 0ull, 0ull},
    {"audio each 18446744073709551614 limit 18446744073709551615", {}, {}, 0ull, {{18446744073709551614ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 18446744073709551614ull},
    {"audio each 18446744073709551615 limit 18446744073709551615", {}, {}, 0ull, {{18446744073709551615ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 18446744073709551615ull},
    {"audio ms 0 limit 0", {}, {}, 0ull, {{1ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 0ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio ms 1 limit 0", {}, {}, 0ull, {{1ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 0ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"audio ms 18446744073709551615 limit 0", {}, {}, 0ull, {{1ull, 18446744073709551615ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 0ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"audio ms 999 limit 1000", {}, {}, 0ull, {{1ull, 999ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 1000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio ms 1000 limit 1000", {}, {}, 0ull, {{1ull, 1000ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 1000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio ms 1001 limit 1000", {}, {}, 0ull, {{1ull, 1001ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 1000ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"audio ms 18446744073709551615 limit 1000", {}, {}, 0ull, {{1ull, 18446744073709551615ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 1000ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"audio ms 599999 limit 600000", {}, {}, 0ull, {{1ull, 599999ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio ms 600000 limit 600000", {}, {}, 0ull, {{1ull, 600000ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio ms 600001 limit 600000", {}, {}, 0ull, {{1ull, 600001ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"audio ms 18446744073709551615 limit 600000", {}, {}, 0ull, {{1ull, 18446744073709551615ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"audio ms 18446744073709551614 limit 18446744073709551615", {}, {}, 0ull, {{1ull, 18446744073709551614ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio ms 18446744073709551615 limit 18446744073709551615", {}, {}, 0ull, {{1ull, 18446744073709551615ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 1ull},
    {"audio total 0 limit 0", {}, {}, 0ull, {{0ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 0ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"audio total 1 limit 0", {}, {}, 0ull, {{1ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 0ull, 600000ull}, BudgetVerdict::AudioBudget, 0, 0ull, 0ull, 0ull},
    {"audio total 30 limit 0", {}, {}, 0ull, {{15ull, 1ull}, {15ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 0ull, 600000ull}, BudgetVerdict::AudioBudget, 0, 0ull, 0ull, 0ull},
    {"audio total 9 limit 10", {}, {}, 0ull, {{5ull, 1ull}, {4ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 10ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 9ull},
    {"audio total 10 limit 10", {}, {}, 0ull, {{5ull, 1ull}, {5ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 10ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 10ull},
    {"audio total 11 limit 10", {}, {}, 0ull, {{6ull, 1ull}, {5ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 10ull, 600000ull}, BudgetVerdict::AudioBudget, 1, 0ull, 0ull, 6ull},
    {"audio total 30 limit 10", {}, {}, 0ull, {{15ull, 1ull}, {15ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 10ull, 600000ull}, BudgetVerdict::AudioBudget, 0, 0ull, 0ull, 0ull},
    {"audio total 30 limit 18446744073709551615", {}, {}, 0ull, {{15ull, 1ull}, {15ull, 1ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 18446744073709551615ull, 18446744073709551615ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 30ull},
    {"audio total overflow", {}, {}, 0ull, {{18446744073709551615ull, 0ull}, {1ull, 0ull}}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 1, 0ull, 0ull, 18446744073709551615ull},
    {"audio total exact max", {}, {}, 0ull, {{18446744073709551610ull, 0ull}, {5ull, 0ull}}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 18446744073709551615ull},
    {"audio too large before too long", {}, {}, 0ull, {{101ull, 18446744073709551615ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 100ull, 134217728ull, 5ull}, BudgetVerdict::AudioTooLarge, 0, 0ull, 0ull, 0ull},
    {"audio too long before budget", {}, {}, 0ull, {{10ull, 6ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 100ull, 1ull, 5ull}, BudgetVerdict::AudioTooLong, 0, 0ull, 0ull, 0ull},
    {"texture failure hides script failure", {{0u, 1u, TextureFormat::R8, 1u, 1u}}, {18446744073709551615ull}, 18446744073709551615ull, {{18446744073709551615ull, 18446744073709551615ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"script failure keeps texture total", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}}, {7ull, 2097152ull}, 18446744073709551615ull, {{18446744073709551615ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::ScriptTooLarge, 1, 64ull, 7ull, 0ull},
    {"node failure keeps both totals", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}}, {7ull, 8ull}, 100001ull, {{18446744073709551615ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TooManyNodes, 0, 64ull, 15ull, 0ull},
    {"audio failure keeps all totals", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}}, {7ull, 8ull}, 5ull, {{3ull, 4ull}, {34603008ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::AudioTooLarge, 1, 64ull, 15ull, 3ull},
    {"ok carries totals", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}, {2u, 2u, TextureFormat::R8, 1u, 1u}}, {7ull, 8ull}, 5ull, {{3ull, 4ull}, {5ull, 6ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 68ull, 15ull, 8ull},
    {"empty scene", {}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"zero values pass", {}, {0ull}, 0ull, {{0ull, 0ull}}, {1024ull, 8192u, 134217728ull, 536870912ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}, BudgetVerdict::Ok, 0, 0ull, 0ull, 0ull},
    {"failed texture reports accepted total", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}, {4u, 4u, TextureFormat::Rgba8, 1u, 1u}, {4u, 4u, TextureFormat::Rgba8, 1u, 1u}, {1u, 1u, TextureFormat::R8, 2u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 3, 192ull, 0ull, 0ull},
    {"budget failure reports accepted total", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}, {4u, 4u, TextureFormat::Rgba8, 1u, 1u}, {4u, 4u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 150ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureBudget, 2, 128ull, 0ull, 0ull},
    {"bad texture (0, 4, 'R8', 1, 1)", {{0u, 4u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (4, 0, 'R8', 1, 1)", {{4u, 0u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (4, 4, 'R8', 0, 1)", {{4u, 4u, TextureFormat::R8, 0u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (4, 4, 'R8', 1, 0)", {{4u, 4u, TextureFormat::R8, 1u, 0u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (4, 4, 'Bad', 1, 1)", {{4u, 4u, static_cast<TextureFormat>(200), 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (4, 4, 'R8', 4, 1)", {{4u, 4u, TextureFormat::R8, 4u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (1, 1, 'R8', 2, 1)", {{1u, 1u, TextureFormat::R8, 2u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (8, 3, 'R8', 5, 1)", {{8u, 3u, TextureFormat::R8, 5u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (4294967295, 4294967295, 'R8', 33, 1)", {{4294967295u, 4294967295u, TextureFormat::R8, 33u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (0, 9000, 'R8', 1, 1)", {{0u, 9000u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (9000, 9000, 'R8', 15, 1)", {{9000u, 9000u, TextureFormat::R8, 15u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (9000, 1, 'R8', 0, 1)", {{9000u, 1u, TextureFormat::R8, 0u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"bad texture (9000, 9000, 'Bad', 1, 1)", {{9000u, 9000u, static_cast<TextureFormat>(200), 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"dimension after valid", {{9000u, 9000u, TextureFormat::R8, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"dimension limit 0", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 0u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"chain edge ok", {{4294967295u, 1u, TextureFormat::R8, 32u, 1u}}, {}, 0ull, {}, {1024ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8589934558ull, 0ull, 0ull},
    {"chain edge 4096", {{4096u, 4096u, TextureFormat::R8, 13u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 22369621ull, 0ull, 0ull},
    {"chain edge 4096 plus one", {{4096u, 4096u, TextureFormat::R8, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::BadTexture, 0, 0ull, 0ull, 0ull},
    {"size R8 1x1 m1 l1", {{1u, 1u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1ull, 0ull, 0ull},
    {"size R8 4x4 m1 l1", {{4u, 4u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 16ull, 0ull, 0ull},
    {"size R8 5x5 m3 l1", {{5u, 5u, TextureFormat::R8, 3u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 30ull, 0ull, 0ull},
    {"size R8 9x3 m4 l2", {{9u, 3u, TextureFormat::R8, 4u, 2u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 68ull, 0ull, 0ull},
    {"size R8 256x256 m9 l1", {{256u, 256u, TextureFormat::R8, 9u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 87381ull, 0ull, 0ull},
    {"size R8 256x256 m1 l6", {{256u, 256u, TextureFormat::R8, 1u, 6u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 393216ull, 0ull, 0ull},
    {"size R8 7x13 m4 l3", {{7u, 13u, TextureFormat::R8, 4u, 3u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 339ull, 0ull, 0ull},
    {"size R8 4096x2048 m13 l1", {{4096u, 2048u, TextureFormat::R8, 13u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 11184811ull, 0ull, 0ull},
    {"size R8 8192x8192 m14 l1", {{8192u, 8192u, TextureFormat::R8, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 89478485ull, 0ull, 0ull},
    {"size R8 3x100 m7 l1", {{3u, 100u, TextureFormat::R8, 7u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 397ull, 0ull, 0ull},
    {"size Rg8 1x1 m1 l1", {{1u, 1u, TextureFormat::Rg8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 2ull, 0ull, 0ull},
    {"size Rg8 4x4 m1 l1", {{4u, 4u, TextureFormat::Rg8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 32ull, 0ull, 0ull},
    {"size Rg8 5x5 m3 l1", {{5u, 5u, TextureFormat::Rg8, 3u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 60ull, 0ull, 0ull},
    {"size Rg8 9x3 m4 l2", {{9u, 3u, TextureFormat::Rg8, 4u, 2u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 136ull, 0ull, 0ull},
    {"size Rg8 256x256 m9 l1", {{256u, 256u, TextureFormat::Rg8, 9u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 174762ull, 0ull, 0ull},
    {"size Rg8 256x256 m1 l6", {{256u, 256u, TextureFormat::Rg8, 1u, 6u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 786432ull, 0ull, 0ull},
    {"size Rg8 7x13 m4 l3", {{7u, 13u, TextureFormat::Rg8, 4u, 3u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 678ull, 0ull, 0ull},
    {"size Rg8 4096x2048 m13 l1", {{4096u, 2048u, TextureFormat::Rg8, 13u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 22369622ull, 0ull, 0ull},
    {"size Rg8 8192x8192 m14 l1", {{8192u, 8192u, TextureFormat::Rg8, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 178956970ull, 0ull, 0ull},
    {"size Rg8 3x100 m7 l1", {{3u, 100u, TextureFormat::Rg8, 7u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 794ull, 0ull, 0ull},
    {"size Rgba8 1x1 m1 l1", {{1u, 1u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 4ull, 0ull, 0ull},
    {"size Rgba8 4x4 m1 l1", {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 64ull, 0ull, 0ull},
    {"size Rgba8 5x5 m3 l1", {{5u, 5u, TextureFormat::Rgba8, 3u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 120ull, 0ull, 0ull},
    {"size Rgba8 9x3 m4 l2", {{9u, 3u, TextureFormat::Rgba8, 4u, 2u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 272ull, 0ull, 0ull},
    {"size Rgba8 256x256 m9 l1", {{256u, 256u, TextureFormat::Rgba8, 9u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 349524ull, 0ull, 0ull},
    {"size Rgba8 256x256 m1 l6", {{256u, 256u, TextureFormat::Rgba8, 1u, 6u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1572864ull, 0ull, 0ull},
    {"size Rgba8 7x13 m4 l3", {{7u, 13u, TextureFormat::Rgba8, 4u, 3u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1356ull, 0ull, 0ull},
    {"size Rgba8 4096x2048 m13 l1", {{4096u, 2048u, TextureFormat::Rgba8, 13u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 44739244ull, 0ull, 0ull},
    {"size Rgba8 8192x8192 m14 l1", {{8192u, 8192u, TextureFormat::Rgba8, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 357913940ull, 0ull, 0ull},
    {"size Rgba8 3x100 m7 l1", {{3u, 100u, TextureFormat::Rgba8, 7u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 1588ull, 0ull, 0ull},
    {"size Bc1 1x1 m1 l1", {{1u, 1u, TextureFormat::Bc1, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8ull, 0ull, 0ull},
    {"size Bc1 4x4 m1 l1", {{4u, 4u, TextureFormat::Bc1, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 8ull, 0ull, 0ull},
    {"size Bc1 5x5 m3 l1", {{5u, 5u, TextureFormat::Bc1, 3u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 48ull, 0ull, 0ull},
    {"size Bc1 9x3 m4 l2", {{9u, 3u, TextureFormat::Bc1, 4u, 2u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 96ull, 0ull, 0ull},
    {"size Bc1 256x256 m9 l1", {{256u, 256u, TextureFormat::Bc1, 9u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 43704ull, 0ull, 0ull},
    {"size Bc1 256x256 m1 l6", {{256u, 256u, TextureFormat::Bc1, 1u, 6u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 196608ull, 0ull, 0ull},
    {"size Bc1 7x13 m4 l3", {{7u, 13u, TextureFormat::Bc1, 4u, 3u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 288ull, 0ull, 0ull},
    {"size Bc1 4096x2048 m13 l1", {{4096u, 2048u, TextureFormat::Bc1, 13u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 5592424ull, 0ull, 0ull},
    {"size Bc1 8192x8192 m14 l1", {{8192u, 8192u, TextureFormat::Bc1, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 44739256ull, 0ull, 0ull},
    {"size Bc1 3x100 m7 l1", {{3u, 100u, TextureFormat::Bc1, 7u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 416ull, 0ull, 0ull},
    {"size Bc7 1x1 m1 l1", {{1u, 1u, TextureFormat::Bc7, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 16ull, 0ull, 0ull},
    {"size Bc7 4x4 m1 l1", {{4u, 4u, TextureFormat::Bc7, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 16ull, 0ull, 0ull},
    {"size Bc7 5x5 m3 l1", {{5u, 5u, TextureFormat::Bc7, 3u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 96ull, 0ull, 0ull},
    {"size Bc7 9x3 m4 l2", {{9u, 3u, TextureFormat::Bc7, 4u, 2u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 192ull, 0ull, 0ull},
    {"size Bc7 256x256 m9 l1", {{256u, 256u, TextureFormat::Bc7, 9u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 87408ull, 0ull, 0ull},
    {"size Bc7 256x256 m1 l6", {{256u, 256u, TextureFormat::Bc7, 1u, 6u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 393216ull, 0ull, 0ull},
    {"size Bc7 7x13 m4 l3", {{7u, 13u, TextureFormat::Bc7, 4u, 3u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 576ull, 0ull, 0ull},
    {"size Bc7 4096x2048 m13 l1", {{4096u, 2048u, TextureFormat::Bc7, 13u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 11184848ull, 0ull, 0ull},
    {"size Bc7 8192x8192 m14 l1", {{8192u, 8192u, TextureFormat::Bc7, 14u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 89478512ull, 0ull, 0ull},
    {"size Bc7 3x100 m7 l1", {{3u, 100u, TextureFormat::Bc7, 7u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 18446744073709551615ull, 18446744073709551615ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 832ull, 0ull, 0ull},
    {"overflow level R8", {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 18446744065119617025ull, 0ull, 0ull},
    {"overflow chain R8", {{4294967295u, 4294967295u, TextureFormat::R8, 32u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow layers R8", {{4294967295u, 1u, TextureFormat::R8, 1u, 4294967295u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 18446744065119617025ull, 0ull, 0ull},
    {"overflow before each R8", {{4294967295u, 4294967295u, TextureFormat::R8, 32u, 4294967295u}}, {}, 0ull, {}, {1024ull, 4294967295u, 5ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow level Rg8", {{4294967295u, 4294967295u, TextureFormat::Rg8, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow chain Rg8", {{4294967295u, 4294967295u, TextureFormat::Rg8, 32u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow layers Rg8", {{4294967295u, 1u, TextureFormat::Rg8, 1u, 4294967295u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow before each Rg8", {{4294967295u, 4294967295u, TextureFormat::Rg8, 32u, 4294967295u}}, {}, 0ull, {}, {1024ull, 4294967295u, 5ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow level Rgba8", {{4294967295u, 4294967295u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow chain Rgba8", {{4294967295u, 4294967295u, TextureFormat::Rgba8, 32u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow layers Rgba8", {{4294967295u, 1u, TextureFormat::Rgba8, 1u, 4294967295u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow before each Rgba8", {{4294967295u, 4294967295u, TextureFormat::Rgba8, 32u, 4294967295u}}, {}, 0ull, {}, {1024ull, 4294967295u, 5ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow level Bc1", {{4294967295u, 4294967295u, TextureFormat::Bc1, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 9223372036854775808ull, 0ull, 0ull},
    {"overflow chain Bc1", {{4294967295u, 4294967295u, TextureFormat::Bc1, 32u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 12297829382473034416ull, 0ull, 0ull},
    {"overflow layers Bc1", {{4294967295u, 1u, TextureFormat::Bc1, 1u, 4294967295u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow before each Bc1", {{4294967295u, 4294967295u, TextureFormat::Bc1, 32u, 4294967295u}}, {}, 0ull, {}, {1024ull, 4294967295u, 5ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow level Bc7", {{4294967295u, 4294967295u, TextureFormat::Bc7, 1u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow chain Bc7", {{4294967295u, 4294967295u, TextureFormat::Bc7, 32u, 1u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow layers Bc7", {{4294967295u, 1u, TextureFormat::Bc7, 1u, 4294967295u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow before each Bc7", {{4294967295u, 4294967295u, TextureFormat::Bc7, 32u, 4294967295u}}, {}, 0ull, {}, {1024ull, 4294967295u, 5ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::SizeOverflow, 0, 0ull, 0ull, 0ull},
    {"overflow after dimension", {{4294967295u, 4294967295u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 4294967294u, 134217728ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureDimension, 0, 0ull, 0ull, 0ull},
    {"overflow fits 64 bits barely", {{4294967295u, 1u, TextureFormat::R8, 1u, 4294967295u}}, {}, 0ull, {}, {18446744073709551615ull, 4294967295u, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull, 18446744073709551615ull}, BudgetVerdict::Ok, 0, 18446744065119617025ull, 0ull, 0ull},
    {"texture too large with small limit", {{100u, 100u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 39999ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::TextureTooLarge, 0, 0ull, 0ull, 0ull},
    {"texture at each limit", {{100u, 100u, TextureFormat::Rgba8, 1u, 1u}}, {}, 0ull, {}, {1024ull, 8192u, 40000ull, 536870912ull, 1048576ull, 4194304ull, 100000ull, 33554432ull, 134217728ull, 600000ull}, BudgetVerdict::Ok, 0, 40000ull, 0ull, 0ull},
};
const RefTexSize kRefSizes[] = {
    {{1u, 1u, TextureFormat::R8, 1u, 1u}, 0, 1ull},
    {{0u, 1u, TextureFormat::R8, 1u, 1u}, 1, 0ull},
    {{1u, 0u, TextureFormat::R8, 1u, 1u}, 1, 0ull},
    {{4294967295u, 1u, TextureFormat::R8, 1u, 1u}, 0, 4294967295ull},
    {{1u, 4294967295u, TextureFormat::R8, 1u, 1u}, 0, 4294967295ull},
    {{2u, 1u, TextureFormat::R8, 1u, 1u}, 0, 2ull},
    {{1u, 2u, TextureFormat::R8, 1u, 1u}, 0, 2ull},
    {{8191u, 1u, TextureFormat::R8, 1u, 1u}, 0, 8191ull},
    {{1u, 8191u, TextureFormat::R8, 1u, 1u}, 0, 8191ull},
    {{8192u, 1u, TextureFormat::R8, 1u, 1u}, 0, 8192ull},
    {{1u, 8192u, TextureFormat::R8, 1u, 1u}, 0, 8192ull},
    {{8193u, 1u, TextureFormat::R8, 1u, 1u}, 0, 8193ull},
    {{1u, 8193u, TextureFormat::R8, 1u, 1u}, 0, 8193ull},
    {{4294967294u, 1u, TextureFormat::R8, 1u, 1u}, 0, 4294967294ull},
    {{1u, 4294967294u, TextureFormat::R8, 1u, 1u}, 0, 4294967294ull},
    {{63u, 1u, TextureFormat::R8, 1u, 1u}, 0, 63ull},
    {{64u, 1u, TextureFormat::R8, 1u, 1u}, 0, 64ull},
    {{65u, 1u, TextureFormat::R8, 1u, 1u}, 0, 65ull},
    {{4294967295u, 4294967295u, TextureFormat::R8, 1u, 1u}, 0, 18446744065119617025ull},
    {{4u, 4u, TextureFormat::Rgba8, 1u, 1u}, 0, 64ull},
    {{0u, 4u, TextureFormat::R8, 1u, 1u}, 1, 0ull},
    {{4u, 0u, TextureFormat::R8, 1u, 1u}, 1, 0ull},
    {{4u, 4u, TextureFormat::R8, 0u, 1u}, 1, 0ull},
    {{4u, 4u, TextureFormat::R8, 1u, 0u}, 1, 0ull},
    {{4u, 4u, static_cast<TextureFormat>(200), 1u, 1u}, 1, 0ull},
    {{4u, 4u, TextureFormat::R8, 4u, 1u}, 1, 0ull},
    {{1u, 1u, TextureFormat::R8, 2u, 1u}, 1, 0ull},
    {{8u, 3u, TextureFormat::R8, 5u, 1u}, 1, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::R8, 33u, 1u}, 1, 0ull},
    {{0u, 9000u, TextureFormat::R8, 1u, 1u}, 1, 0ull},
    {{9000u, 9000u, TextureFormat::R8, 15u, 1u}, 1, 0ull},
    {{9000u, 1u, TextureFormat::R8, 0u, 1u}, 1, 0ull},
    {{9000u, 9000u, static_cast<TextureFormat>(200), 1u, 1u}, 1, 0ull},
    {{9000u, 9000u, TextureFormat::R8, 14u, 1u}, 0, 107999029ull},
    {{4294967295u, 1u, TextureFormat::R8, 32u, 1u}, 0, 8589934558ull},
    {{4096u, 4096u, TextureFormat::R8, 13u, 1u}, 0, 22369621ull},
    {{4096u, 4096u, TextureFormat::R8, 14u, 1u}, 1, 0ull},
    {{4u, 4u, TextureFormat::R8, 1u, 1u}, 0, 16ull},
    {{5u, 5u, TextureFormat::R8, 3u, 1u}, 0, 30ull},
    {{9u, 3u, TextureFormat::R8, 4u, 2u}, 0, 68ull},
    {{256u, 256u, TextureFormat::R8, 9u, 1u}, 0, 87381ull},
    {{256u, 256u, TextureFormat::R8, 1u, 6u}, 0, 393216ull},
    {{7u, 13u, TextureFormat::R8, 4u, 3u}, 0, 339ull},
    {{4096u, 2048u, TextureFormat::R8, 13u, 1u}, 0, 11184811ull},
    {{8192u, 8192u, TextureFormat::R8, 14u, 1u}, 0, 89478485ull},
    {{3u, 100u, TextureFormat::R8, 7u, 1u}, 0, 397ull},
    {{1u, 1u, TextureFormat::Rg8, 1u, 1u}, 0, 2ull},
    {{4u, 4u, TextureFormat::Rg8, 1u, 1u}, 0, 32ull},
    {{5u, 5u, TextureFormat::Rg8, 3u, 1u}, 0, 60ull},
    {{9u, 3u, TextureFormat::Rg8, 4u, 2u}, 0, 136ull},
    {{256u, 256u, TextureFormat::Rg8, 9u, 1u}, 0, 174762ull},
    {{256u, 256u, TextureFormat::Rg8, 1u, 6u}, 0, 786432ull},
    {{7u, 13u, TextureFormat::Rg8, 4u, 3u}, 0, 678ull},
    {{4096u, 2048u, TextureFormat::Rg8, 13u, 1u}, 0, 22369622ull},
    {{8192u, 8192u, TextureFormat::Rg8, 14u, 1u}, 0, 178956970ull},
    {{3u, 100u, TextureFormat::Rg8, 7u, 1u}, 0, 794ull},
    {{1u, 1u, TextureFormat::Rgba8, 1u, 1u}, 0, 4ull},
    {{5u, 5u, TextureFormat::Rgba8, 3u, 1u}, 0, 120ull},
    {{9u, 3u, TextureFormat::Rgba8, 4u, 2u}, 0, 272ull},
    {{256u, 256u, TextureFormat::Rgba8, 9u, 1u}, 0, 349524ull},
    {{256u, 256u, TextureFormat::Rgba8, 1u, 6u}, 0, 1572864ull},
    {{7u, 13u, TextureFormat::Rgba8, 4u, 3u}, 0, 1356ull},
    {{4096u, 2048u, TextureFormat::Rgba8, 13u, 1u}, 0, 44739244ull},
    {{8192u, 8192u, TextureFormat::Rgba8, 14u, 1u}, 0, 357913940ull},
    {{3u, 100u, TextureFormat::Rgba8, 7u, 1u}, 0, 1588ull},
    {{1u, 1u, TextureFormat::Bc1, 1u, 1u}, 0, 8ull},
    {{4u, 4u, TextureFormat::Bc1, 1u, 1u}, 0, 8ull},
    {{5u, 5u, TextureFormat::Bc1, 3u, 1u}, 0, 48ull},
    {{9u, 3u, TextureFormat::Bc1, 4u, 2u}, 0, 96ull},
    {{256u, 256u, TextureFormat::Bc1, 9u, 1u}, 0, 43704ull},
    {{256u, 256u, TextureFormat::Bc1, 1u, 6u}, 0, 196608ull},
    {{7u, 13u, TextureFormat::Bc1, 4u, 3u}, 0, 288ull},
    {{4096u, 2048u, TextureFormat::Bc1, 13u, 1u}, 0, 5592424ull},
    {{8192u, 8192u, TextureFormat::Bc1, 14u, 1u}, 0, 44739256ull},
    {{3u, 100u, TextureFormat::Bc1, 7u, 1u}, 0, 416ull},
    {{1u, 1u, TextureFormat::Bc7, 1u, 1u}, 0, 16ull},
    {{4u, 4u, TextureFormat::Bc7, 1u, 1u}, 0, 16ull},
    {{5u, 5u, TextureFormat::Bc7, 3u, 1u}, 0, 96ull},
    {{9u, 3u, TextureFormat::Bc7, 4u, 2u}, 0, 192ull},
    {{256u, 256u, TextureFormat::Bc7, 9u, 1u}, 0, 87408ull},
    {{256u, 256u, TextureFormat::Bc7, 1u, 6u}, 0, 393216ull},
    {{7u, 13u, TextureFormat::Bc7, 4u, 3u}, 0, 576ull},
    {{4096u, 2048u, TextureFormat::Bc7, 13u, 1u}, 0, 11184848ull},
    {{8192u, 8192u, TextureFormat::Bc7, 14u, 1u}, 0, 89478512ull},
    {{3u, 100u, TextureFormat::Bc7, 7u, 1u}, 0, 832ull},
    {{4294967295u, 4294967295u, TextureFormat::R8, 32u, 1u}, 2, 0ull},
    {{4294967295u, 1u, TextureFormat::R8, 1u, 4294967295u}, 0, 18446744065119617025ull},
    {{4294967295u, 4294967295u, TextureFormat::R8, 32u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Rg8, 1u, 1u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Rg8, 32u, 1u}, 2, 0ull},
    {{4294967295u, 1u, TextureFormat::Rg8, 1u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Rg8, 32u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Rgba8, 1u, 1u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Rgba8, 32u, 1u}, 2, 0ull},
    {{4294967295u, 1u, TextureFormat::Rgba8, 1u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Rgba8, 32u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Bc1, 1u, 1u}, 0, 9223372036854775808ull},
    {{4294967295u, 4294967295u, TextureFormat::Bc1, 32u, 1u}, 0, 12297829382473034416ull},
    {{4294967295u, 1u, TextureFormat::Bc1, 1u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Bc1, 32u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Bc7, 1u, 1u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Bc7, 32u, 1u}, 2, 0ull},
    {{4294967295u, 1u, TextureFormat::Bc7, 1u, 4294967295u}, 2, 0ull},
    {{4294967295u, 4294967295u, TextureFormat::Bc7, 32u, 4294967295u}, 2, 0ull},
    {{100u, 100u, TextureFormat::Rgba8, 1u, 1u}, 0, 40000ull},
};
// END GENERATED REFERENCE

TextureInfo tex_of(const RefTex& r) {
  TextureInfo t;
  t.width = r.w;
  t.height = r.h;
  t.format = r.fmt;
  t.mip_levels = r.mips;
  t.layers = r.layers;
  return t;
}

TextureInfo tex(std::uint32_t w, std::uint32_t h, TextureFormat f = TextureFormat::Rgba8,
                std::uint32_t mips = 1, std::uint32_t layers = 1) {
  return tex_of(RefTex{w, h, f, mips, layers});
}

LoadBudgetLimits limits_of(const RefLimits& r) {
  LoadBudgetLimits l;
  l.max_textures = r.max_textures;
  l.max_texture_dimension = r.dim;
  l.max_texture_bytes_each = r.tex_each;
  l.max_texture_bytes_total = r.tex_total;
  l.max_script_bytes_each = r.scr_each;
  l.max_script_bytes_total = r.scr_total;
  l.max_nodes = r.nodes;
  l.max_audio_bytes_each = r.aud_each;
  l.max_audio_bytes_total = r.aud_total;
  l.max_audio_ms_each = r.aud_ms;
  return l;
}

// Everything at the maximum, so only the field under test can fail.
LoadBudgetLimits big() {
  LoadBudgetLimits l;
  l.max_textures = kMax;
  l.max_texture_dimension = kMax32;
  l.max_texture_bytes_each = kMax;
  l.max_texture_bytes_total = kMax;
  l.max_script_bytes_each = kMax;
  l.max_script_bytes_total = kMax;
  l.max_nodes = kMax;
  l.max_audio_bytes_each = kMax;
  l.max_audio_bytes_total = kMax;
  l.max_audio_ms_each = kMax;
  return l;
}

void expect(const BudgetResult& r, BudgetVerdict v, std::size_t index, std::uint64_t t, std::uint64_t s,
            std::uint64_t a) {
  INFO("got " << to_string(r.verdict) << " index " << r.item_index << " totals " << r.texture_bytes << " "
               << r.script_bytes << " " << r.audio_bytes);
  REQUIRE(r.verdict == v);
  REQUIRE(r.item_index == index);
  REQUIRE(r.texture_bytes == t);
  REQUIRE(r.script_bytes == s);
  REQUIRE(r.audio_bytes == a);
}

SceneStats scene_with_textures(std::vector<TextureInfo> t) {
  SceneStats s;
  s.textures = std::move(t);
  return s;
}

}  // namespace

TEST_CASE("load budget reference constants from tools/load_budget_ref.py", "[load_budget]") {
  REQUIRE(kRefDefaults.max_textures == 1024);
  REQUIRE(kRefDefaults.dim == 8192);
  const LoadBudgetLimits d;
  REQUIRE(d.max_textures == kRefDefaults.max_textures);
  REQUIRE(d.max_texture_dimension == kRefDefaults.dim);
  REQUIRE(d.max_texture_bytes_each == kRefDefaults.tex_each);
  REQUIRE(d.max_texture_bytes_total == kRefDefaults.tex_total);
  REQUIRE(d.max_script_bytes_each == kRefDefaults.scr_each);
  REQUIRE(d.max_script_bytes_total == kRefDefaults.scr_total);
  REQUIRE(d.max_nodes == kRefDefaults.nodes);
  REQUIRE(d.max_audio_bytes_each == kRefDefaults.aud_each);
  REQUIRE(d.max_audio_bytes_total == kRefDefaults.aud_total);
  REQUIRE(d.max_audio_ms_each == kRefDefaults.aud_ms);
  // Hand-written values for the same limits.
  REQUIRE(d.max_texture_bytes_each == 134217728ull);
  REQUIRE(d.max_texture_bytes_total == 536870912ull);
  REQUIRE(d.max_script_bytes_each == 1048576ull);
  REQUIRE(d.max_script_bytes_total == 4194304ull);
  REQUIRE(d.max_audio_bytes_each == 33554432ull);
  REQUIRE(d.max_audio_bytes_total == 134217728ull);
  REQUIRE(d.max_audio_ms_each == 600000ull);
}

TEST_CASE("load budget scenes match the Python reference", "[load_budget]") {
  REQUIRE(std::size(kRefScenes) > 200);
  for (const RefScene& r : kRefScenes) {
    INFO(r.name);
    SceneStats s;
    for (const RefTex& t : r.tex) s.textures.push_back(tex_of(t));
    s.script_bytes = r.scripts;
    s.node_count = r.nodes;
    for (const RefAudio& a : r.audio) {
      AudioInfo i;
      i.encoded_bytes = a.bytes;
      i.duration_ms = a.ms;
      s.audio.push_back(i);
    }
    expect(check_load_budget(s, limits_of(r.lim)), r.verdict, r.index, r.tex_bytes, r.script_bytes,
           r.audio_bytes);
  }
}

TEST_CASE("texture_bytes matches the Python reference and leaves out untouched on false", "[load_budget]") {
  REQUIRE(std::size(kRefSizes) > 50);
  for (const RefTexSize& r : kRefSizes) {
    INFO("w " << r.t.w << " h " << r.t.h << " format " << to_string(r.t.fmt) << " mips " << r.t.mips
              << " layers " << r.t.layers);
    std::uint64_t out = 0xDEADBEEFull;
    const bool ok = texture_bytes(tex_of(r.t), &out);
    if (r.status == 0) {
      REQUIRE(ok);
      REQUIRE(out == r.bytes);
    } else {
      REQUIRE_FALSE(ok);
      REQUIRE(out == 0xDEADBEEFull);
    }
  }
}

TEST_CASE("load budget texture sizes, written by hand", "[load_budget]") {
  std::uint64_t out = 0;
  auto bytes = [&](const TextureInfo& t) {
    REQUIRE(texture_bytes(t, &out));
    return out;
  };
  SECTION("uncompressed formats") {
    REQUIRE(bytes(tex(1, 1, TextureFormat::R8)) == 1);
    REQUIRE(bytes(tex(1, 1, TextureFormat::Rg8)) == 2);
    REQUIRE(bytes(tex(1, 1, TextureFormat::Rgba8)) == 4);
    REQUIRE(bytes(tex(4, 4, TextureFormat::Rgba8)) == 64);
    REQUIRE(bytes(tex(3, 5, TextureFormat::Rg8)) == 30);
  }
  SECTION("block formats round up to whole blocks") {
    REQUIRE(bytes(tex(1, 1, TextureFormat::Bc1)) == 8);
    REQUIRE(bytes(tex(4, 4, TextureFormat::Bc1)) == 8);
    REQUIRE(bytes(tex(5, 4, TextureFormat::Bc1)) == 16);
    REQUIRE(bytes(tex(5, 5, TextureFormat::Bc7)) == 64);
    REQUIRE(bytes(tex(8, 8, TextureFormat::Bc7)) == 64);
  }
  SECTION("mip chains") {
    REQUIRE(bytes(tex(256, 256, TextureFormat::Rgba8, 9)) == 349524);
    REQUIRE(bytes(tex(8, 8, TextureFormat::Bc7, 4)) == 112);
    REQUIRE(bytes(tex(8, 8, TextureFormat::Bc7, 1)) == 64);
    REQUIRE(bytes(tex(4, 2, TextureFormat::R8, 3)) == 8 + 2 + 1);  // 4x2, 2x1, 1x1
    REQUIRE(bytes(tex(1, 8, TextureFormat::R8, 4)) == 8 + 4 + 2 + 1);
  }
  SECTION("layers multiply the whole chain, tight") {
    REQUIRE(bytes(tex(256, 256, TextureFormat::Rgba8, 1, 6)) == 256ull * 256 * 4 * 6);
    REQUIRE(bytes(tex(4, 4, TextureFormat::Rgba8, 3, 3)) == (64 + 16 + 4) * 3);
  }
  SECTION("invalid textures") {
    out = 7;
    REQUIRE_FALSE(texture_bytes(tex(0, 4), &out));
    REQUIRE_FALSE(texture_bytes(tex(4, 0), &out));
    REQUIRE_FALSE(texture_bytes(tex(4, 4, TextureFormat::R8, 0), &out));
    REQUIRE_FALSE(texture_bytes(tex(4, 4, TextureFormat::R8, 1, 0), &out));
    REQUIRE_FALSE(texture_bytes(tex(4, 4, TextureFormat::R8, 4), &out));
    REQUIRE_FALSE(texture_bytes(tex(4, 4, static_cast<TextureFormat>(5)), &out));
    REQUIRE_FALSE(texture_bytes(tex(4, 4, static_cast<TextureFormat>(255)), &out));
    REQUIRE(out == 7);
  }
  SECTION("the chain length is set by the larger side") {
    REQUIRE(texture_bytes(tex(4, 4, TextureFormat::R8, 3), &out));
    REQUIRE(texture_bytes(tex(1, 4, TextureFormat::R8, 3), &out));
    REQUIRE_FALSE(texture_bytes(tex(1, 4, TextureFormat::R8, 4), &out));
    REQUIRE(texture_bytes(tex(5, 1, TextureFormat::R8, 3), &out));
    REQUIRE_FALSE(texture_bytes(tex(5, 1, TextureFormat::R8, 4), &out));
    REQUIRE(texture_bytes(tex(kMax32, 1, TextureFormat::R8, 32), &out));
    REQUIRE_FALSE(texture_bytes(tex(kMax32, 1, TextureFormat::R8, 33), &out));
    REQUIRE(texture_bytes(tex(2147483648u, 1, TextureFormat::R8, 32), &out));
    REQUIRE_FALSE(texture_bytes(tex(2147483648u, 1, TextureFormat::R8, 33), &out));
    REQUIRE(texture_bytes(tex(2147483647u, 1, TextureFormat::R8, 31), &out));
    REQUIRE_FALSE(texture_bytes(tex(2147483647u, 1, TextureFormat::R8, 32), &out));
  }
  SECTION("overflow is false, in the level, the chain and the layers") {
    out = 9;
    REQUIRE_FALSE(texture_bytes(tex(kMax32, kMax32, TextureFormat::Rgba8, 1), &out));
    REQUIRE_FALSE(texture_bytes(tex(kMax32, kMax32, TextureFormat::Bc7, 1), &out));
    REQUIRE_FALSE(texture_bytes(tex(kMax32, kMax32, TextureFormat::R8, 32), &out));
    REQUIRE_FALSE(texture_bytes(tex(kMax32, 1, TextureFormat::Rgba8, 1, kMax32), &out));
    REQUIRE(out == 9);
    REQUIRE(texture_bytes(tex(kMax32, kMax32, TextureFormat::R8, 1), &out));
    REQUIRE(out == 18446744065119617025ull);  // (2^32 - 1)^2
  }
  SECTION("the pointer is required only on success") {
    REQUIRE(texture_bytes(tex(1, 1, TextureFormat::R8), &out));
  }
}

TEST_CASE("load budget texture check order", "[load_budget]") {
  SECTION("bad texture before dimension") {
    expect(check_load_budget(scene_with_textures({tex(0, 9000)})), BudgetVerdict::BadTexture, 0, 0, 0, 0);
    expect(check_load_budget(scene_with_textures({tex(9000, 9000, TextureFormat::R8, 15)})),
           BudgetVerdict::BadTexture, 0, 0, 0, 0);
    expect(check_load_budget(scene_with_textures({tex(9000, 9000, TextureFormat::R8, 14)})),
           BudgetVerdict::TextureDimension, 0, 0, 0, 0);
  }
  SECTION("dimension before size overflow before too large before budget") {
    LoadBudgetLimits l = big();
    l.max_texture_dimension = kMax32 - 1;
    expect(check_load_budget(scene_with_textures({tex(kMax32, kMax32)}), l), BudgetVerdict::TextureDimension, 0,
           0, 0, 0);
    l.max_texture_dimension = kMax32;
    l.max_texture_bytes_each = 5;
    expect(check_load_budget(scene_with_textures({tex(kMax32, kMax32)}), l), BudgetVerdict::SizeOverflow, 0, 0,
           0, 0);
    expect(check_load_budget(scene_with_textures({tex(4, 4)}), l), BudgetVerdict::TextureTooLarge, 0, 0, 0, 0);
    l.max_texture_bytes_each = kMax;
    l.max_texture_bytes_total = 10;
    expect(check_load_budget(scene_with_textures({tex(4, 4)}), l), BudgetVerdict::TextureBudget, 0, 0, 0, 0);
  }
  SECTION("count before any texture") {
    LoadBudgetLimits l;
    l.max_textures = 1;
    expect(check_load_budget(scene_with_textures({tex(0, 0), tex(0, 0)}), l), BudgetVerdict::TooManyTextures, 0,
           0, 0, 0);
  }
  SECTION("the offending texture is not added to the total, later textures are not read") {
    LoadBudgetLimits l;
    l.max_texture_bytes_total = 100;
    expect(check_load_budget(scene_with_textures({tex(4, 4), tex(4, 4), tex(0, 0)}), l),
           BudgetVerdict::TextureBudget, 1, 64, 0, 0);
    expect(check_load_budget(scene_with_textures({tex(4, 4), tex(0, 0), tex(4, 4)}), l),
           BudgetVerdict::BadTexture, 1, 64, 0, 0);
  }
}

TEST_CASE("load budget to_string", "[load_budget]") {
  REQUIRE(std::string(to_string(BudgetVerdict::Ok)) == "Ok");
  REQUIRE(std::string(to_string(BudgetVerdict::TooManyTextures)) == "TooManyTextures");
  REQUIRE(std::string(to_string(BudgetVerdict::BadTexture)) == "BadTexture");
  REQUIRE(std::string(to_string(BudgetVerdict::TextureDimension)) == "TextureDimension");
  REQUIRE(std::string(to_string(BudgetVerdict::TextureTooLarge)) == "TextureTooLarge");
  REQUIRE(std::string(to_string(BudgetVerdict::TextureBudget)) == "TextureBudget");
  REQUIRE(std::string(to_string(BudgetVerdict::ScriptTooLarge)) == "ScriptTooLarge");
  REQUIRE(std::string(to_string(BudgetVerdict::ScriptBudget)) == "ScriptBudget");
  REQUIRE(std::string(to_string(BudgetVerdict::TooManyNodes)) == "TooManyNodes");
  REQUIRE(std::string(to_string(BudgetVerdict::AudioTooLarge)) == "AudioTooLarge");
  REQUIRE(std::string(to_string(BudgetVerdict::AudioTooLong)) == "AudioTooLong");
  REQUIRE(std::string(to_string(BudgetVerdict::AudioBudget)) == "AudioBudget");
  REQUIRE(std::string(to_string(BudgetVerdict::SizeOverflow)) == "SizeOverflow");
  REQUIRE(std::string(to_string(static_cast<BudgetVerdict>(99))) == "Unknown");
  REQUIRE(std::string(to_string(TextureFormat::R8)) == "R8");
  REQUIRE(std::string(to_string(TextureFormat::Rg8)) == "Rg8");
  REQUIRE(std::string(to_string(TextureFormat::Rgba8)) == "Rgba8");
  REQUIRE(std::string(to_string(TextureFormat::Bc1)) == "Bc1");
  REQUIRE(std::string(to_string(TextureFormat::Bc7)) == "Bc7");
  REQUIRE(std::string(to_string(static_cast<TextureFormat>(77))) == "Unknown");
}

// ---------------------------------------------------------------------------------------------------------
// Oracle: a naive second implementation inside the test. It keeps every number as a 128-bit value made of
// two uint64 halves, so nothing overflows silently, and shares no code with load_budget.cpp.
namespace {

struct Wide {
  std::uint64_t hi = 0;
  std::uint64_t lo = 0;
};

Wide wide_mul(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t a0 = a & 0xFFFFFFFFull, a1 = a >> 32, b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  Wide w;
  w.lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
  w.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  return w;
}

Wide wide_add(Wide a, Wide b) {
  Wide w;
  w.lo = a.lo + b.lo;
  w.hi = a.hi + b.hi + (w.lo < a.lo ? 1 : 0);
  return w;
}

Wide wide_scale(Wide a, std::uint64_t k) {  // a * k, k below 2^33 and a below 2^100: no further overflow
  const Wide l = wide_mul(a.lo, k);
  Wide w;
  w.lo = l.lo;
  w.hi = l.hi + a.hi * k;
  return w;
}

bool fits(Wide w) { return w.hi == 0; }
bool above(Wide w, std::uint64_t limit) { return w.hi != 0 || w.lo > limit; }

struct Oracle {
  BudgetVerdict verdict = BudgetVerdict::Ok;
  std::size_t index = 0;
  std::uint64_t t = 0, s = 0, a = 0;
};

Oracle oracle(const SceneStats& st, const LoadBudgetLimits& lim) {
  Oracle o;
  auto stop = [&](BudgetVerdict v, std::size_t i) {
    o.verdict = v;
    o.index = i;
    return o;
  };
  if (st.textures.size() > lim.max_textures) return stop(BudgetVerdict::TooManyTextures, 0);
  Wide tt;
  for (std::size_t i = 0; i < st.textures.size(); ++i) {
    const TextureInfo& x = st.textures[i];
    const int f = static_cast<int>(x.format);
    bool bad = x.width == 0 || x.height == 0 || x.layers == 0 || x.mip_levels == 0 || f < 0 || f > 4;
    if (!bad) {
      std::uint64_t big_side = x.width > x.height ? x.width : x.height;
      int levels = 0;
      while (big_side > 0) {
        ++levels;
        big_side /= 2;
      }
      if (x.mip_levels > static_cast<std::uint32_t>(levels)) bad = true;
    }
    if (bad) return stop(BudgetVerdict::BadTexture, i);
    if (x.width > lim.max_texture_dimension || x.height > lim.max_texture_dimension)
      return stop(BudgetVerdict::TextureDimension, i);
    static const std::uint64_t unit[] = {1, 2, 4, 8, 16};
    Wide chain;
    for (std::uint32_t l = 0; l < x.mip_levels; ++l) {
      std::uint64_t w = x.width, h = x.height;
      for (std::uint32_t k = 0; k < l; ++k) {
        if (w > 1) w /= 2;
        if (h > 1) h /= 2;
      }
      if (f >= 3) {
        w = (w + 3) / 4;
        h = (h + 3) / 4;
      }
      chain = wide_add(chain, wide_scale(wide_mul(w, h), unit[f]));
    }
    const Wide bytes = wide_scale(chain, x.layers);
    if (!fits(bytes)) return stop(BudgetVerdict::SizeOverflow, i);
    if (bytes.lo > lim.max_texture_bytes_each) return stop(BudgetVerdict::TextureTooLarge, i);
    const Wide next = wide_add(tt, bytes);
    if (!fits(next)) return stop(BudgetVerdict::SizeOverflow, i);
    if (above(next, lim.max_texture_bytes_total)) return stop(BudgetVerdict::TextureBudget, i);
    tt = next;
    o.t = tt.lo;
  }
  Wide ss;
  for (std::size_t i = 0; i < st.script_bytes.size(); ++i) {
    const std::uint64_t b = st.script_bytes[i];
    if (b > lim.max_script_bytes_each) return stop(BudgetVerdict::ScriptTooLarge, i);
    Wide next = wide_add(ss, Wide{0, b});
    if (!fits(next)) return stop(BudgetVerdict::SizeOverflow, i);
    if (above(next, lim.max_script_bytes_total)) return stop(BudgetVerdict::ScriptBudget, i);
    ss = next;
    o.s = ss.lo;
  }
  if (st.node_count > lim.max_nodes) return stop(BudgetVerdict::TooManyNodes, 0);
  Wide aa;
  for (std::size_t i = 0; i < st.audio.size(); ++i) {
    const AudioInfo& x = st.audio[i];
    if (x.encoded_bytes > lim.max_audio_bytes_each) return stop(BudgetVerdict::AudioTooLarge, i);
    if (x.duration_ms > lim.max_audio_ms_each) return stop(BudgetVerdict::AudioTooLong, i);
    Wide next = wide_add(aa, Wide{0, x.encoded_bytes});
    if (!fits(next)) return stop(BudgetVerdict::SizeOverflow, i);
    if (above(next, lim.max_audio_bytes_total)) return stop(BudgetVerdict::AudioBudget, i);
    aa = next;
    o.a = aa.lo;
  }
  return o;
}

}  // namespace

TEST_CASE("load budget against a naive oracle", "[load_budget]") {
  std::mt19937_64 rng(20260312);
  auto pick = [&](std::uint64_t n) { return n == 0 ? 0 : rng() % n; };
  const std::uint32_t dims[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 255, 256, 4097, 8192, 8193,
                                65536, 2147483647u, 2147483648u, kMax32 - 1, kMax32};
  const std::uint64_t edges[] = {0, 1, 2, 3, 10, 63, 64, 65, 100, 1000, 1ull << 20, 1ull << 32,
                                 (1ull << 32) + 1, 1ull << 62, 1ull << 63, kMax - 1, kMax};
  auto edge = [&]() { return edges[pick(std::size(edges))]; };
  auto near = [&](std::uint64_t v) -> std::uint64_t {
    switch (pick(3)) {
      case 0: return v;
      case 1: return v == 0 ? 0 : v - 1;
      default: return v == kMax ? kMax : v + 1;
    }
  };
  std::size_t counts[13] = {};
  for (int iter = 0; iter < 60000; ++iter) {
    SceneStats st;
    const std::size_t nt = pick(4);
    for (std::size_t i = 0; i < nt; ++i) {
      TextureInfo t;
      t.width = pick(4) == 0 ? static_cast<std::uint32_t>(pick(40)) : dims[pick(std::size(dims))];
      t.height = pick(4) == 0 ? static_cast<std::uint32_t>(pick(40)) : dims[pick(std::size(dims))];
      t.format = static_cast<TextureFormat>(pick(12) == 0 ? 5 + pick(3) : pick(5));
      t.mip_levels = pick(3) == 0 ? static_cast<std::uint32_t>(pick(35)) : 1 + static_cast<std::uint32_t>(pick(3));
      if (pick(2) == 0) {
        std::uint32_t m = t.width > t.height ? t.width : t.height;
        std::uint32_t n = 0;
        while (m != 0) {
          ++n;
          m >>= 1;
        }
        if (n != 0) t.mip_levels = 1 + static_cast<std::uint32_t>(pick(n + 1) % n) + (pick(8) == 0 ? 1 : 0);
      }
      t.layers = pick(4) == 0 ? static_cast<std::uint32_t>(edge() & 0xFFFFFFFFull) : 1 + static_cast<std::uint32_t>(pick(6));
      st.textures.push_back(t);
    }
    const std::size_t ns = pick(4);
    for (std::size_t i = 0; i < ns; ++i) st.script_bytes.push_back(edge());
    st.node_count = edge();
    const std::size_t na = pick(4);
    for (std::size_t i = 0; i < na; ++i) {
      AudioInfo a;
      a.encoded_bytes = edge();
      a.duration_ms = edge();
      st.audio.push_back(a);
    }
    LoadBudgetLimits lim;
    if (pick(3) != 0) {
      lim = big();
      // Tighten a few limits, often to exactly a value present in the scene.
      if (pick(2) == 0) lim.max_textures = near(st.textures.size());
      if (pick(2) == 0) lim.max_texture_dimension = static_cast<std::uint32_t>(near(dims[pick(std::size(dims))]));
      if (pick(2) == 0) lim.max_texture_bytes_each = near(edge());
      if (pick(2) == 0) lim.max_texture_bytes_total = near(edge());
      if (pick(2) == 0) lim.max_script_bytes_each = near(edge());
      if (pick(2) == 0) lim.max_script_bytes_total = near(edge());
      if (pick(2) == 0) lim.max_nodes = near(st.node_count);
      if (pick(2) == 0) lim.max_audio_bytes_each = near(edge());
      if (pick(2) == 0) lim.max_audio_bytes_total = near(edge());
      if (pick(2) == 0) lim.max_audio_ms_each = near(edge());
    }
    if (!st.textures.empty() && pick(2) == 0) {
      std::uint64_t b = 0;
      if (texture_bytes(st.textures[pick(st.textures.size())], &b)) {
        lim.max_texture_bytes_each = near(b);
        if (pick(2) == 0) lim.max_texture_bytes_total = near(b);
      }
    }
    if (!st.script_bytes.empty() && pick(2) == 0) {
      std::uint64_t sum = 0;
      for (std::uint64_t b : st.script_bytes) sum += b;
      lim.max_script_bytes_total = near(sum);
      lim.max_script_bytes_each = near(st.script_bytes[pick(st.script_bytes.size())]);
    }
    if (!st.audio.empty() && pick(2) == 0) {
      std::uint64_t sum = 0;
      for (const AudioInfo& a : st.audio) sum += a.encoded_bytes;
      lim.max_audio_bytes_total = near(sum);
      lim.max_audio_bytes_each = near(st.audio[pick(st.audio.size())].encoded_bytes);
      lim.max_audio_ms_each = near(st.audio[pick(st.audio.size())].duration_ms);
    }
    const BudgetResult r = check_load_budget(st, lim);
    const Oracle o = oracle(st, lim);
    ++counts[static_cast<int>(r.verdict)];
    INFO("iteration " << iter << " got " << to_string(r.verdict) << " want " << to_string(o.verdict));
    REQUIRE(r.verdict == o.verdict);
    REQUIRE(r.item_index == o.index);
    REQUIRE(r.texture_bytes == o.t);
    REQUIRE(r.script_bytes == o.s);
    REQUIRE(r.audio_bytes == o.a);
  }
  // Every verdict must actually have been exercised, or the oracle test proves little.
  for (int v = 0; v < 13; ++v) {
    INFO("verdict " << v);
    REQUIRE(counts[v] > 20);
  }
}
