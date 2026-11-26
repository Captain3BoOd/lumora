// Load-time resource budgets. See load_budget.h for the contract and docs/load-budget.md for the bounds.
#include "load_budget.h"

#include <algorithm>
#include <limits>

namespace lumora::package {

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

bool mul_fits(std::uint64_t a, std::uint64_t b, std::uint64_t* out) {
  if (a != 0 && b > kMax / a) return false;
  *out = a * b;
  return true;
}

bool add_fits(std::uint64_t a, std::uint64_t b, std::uint64_t* out) {
  if (b > kMax - a) return false;
  *out = a + b;
  return true;
}

bool format_known(TextureFormat f) {
  switch (f) {
    case TextureFormat::R8:
    case TextureFormat::Rg8:
    case TextureFormat::Rgba8:
    case TextureFormat::Bc1:
    case TextureFormat::Bc7:
      return true;
  }
  return false;
}

bool format_is_block(TextureFormat f) { return f == TextureFormat::Bc1 || f == TextureFormat::Bc7; }

std::uint64_t unit_bytes(TextureFormat f) {
  switch (f) {
    case TextureFormat::R8: return 1;
    case TextureFormat::Rg8: return 2;
    case TextureFormat::Rgba8: return 4;
    case TextureFormat::Bc1: return 8;
    case TextureFormat::Bc7: return 16;
  }
  return 0;
}

// Number of levels in the full chain: floor(log2(max(w, h))) + 1. Both are above 0.
std::uint32_t full_chain(std::uint32_t w, std::uint32_t h) {
  std::uint32_t m = std::max(w, h);
  std::uint32_t n = 0;
  while (m != 0) {
    ++n;
    m >>= 1;
  }
  return n;
}

bool texture_valid(const TextureInfo& t) {
  if (t.width == 0 || t.height == 0 || t.layers == 0 || t.mip_levels == 0) return false;
  if (!format_known(t.format)) return false;
  return t.mip_levels <= full_chain(t.width, t.height);
}

// Bytes of a valid texture, false on 64-bit overflow.
bool bytes_of_valid(const TextureInfo& t, std::uint64_t* out) {
  const bool block = format_is_block(t.format);
  const std::uint64_t unit = unit_bytes(t.format);
  std::uint64_t sum = 0;
  for (std::uint32_t l = 0; l < t.mip_levels; ++l) {
    std::uint64_t w = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(t.width) >> l);
    std::uint64_t h = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(t.height) >> l);
    if (block) {
      w = (w + 3) / 4;
      h = (h + 3) / 4;
    }
    std::uint64_t cells = 0;
    std::uint64_t level = 0;
    if (!mul_fits(w, h, &cells) || !mul_fits(cells, unit, &level)) return false;
    if (!add_fits(sum, level, &sum)) return false;
  }
  return mul_fits(sum, t.layers, out);
}

BudgetResult fail(BudgetVerdict v, std::size_t index, std::uint64_t tex, std::uint64_t scr,
                  std::uint64_t aud) {
  BudgetResult r;
  r.verdict = v;
  r.item_index = index;
  r.texture_bytes = tex;
  r.script_bytes = scr;
  r.audio_bytes = aud;
  return r;
}

}  // namespace

bool texture_bytes(const TextureInfo& t, std::uint64_t* out) {
  if (!texture_valid(t)) return false;
  std::uint64_t v = 0;
  if (!bytes_of_valid(t, &v)) return false;
  *out = v;
  return true;
}

BudgetResult check_load_budget(const SceneStats& stats, const LoadBudgetLimits& limits) {
  std::uint64_t tex_total = 0;
  std::uint64_t script_total = 0;
  std::uint64_t audio_total = 0;

  // Textures.
  if (static_cast<std::uint64_t>(stats.textures.size()) > limits.max_textures) {
    return fail(BudgetVerdict::TooManyTextures, 0, 0, 0, 0);
  }
  for (std::size_t i = 0; i < stats.textures.size(); ++i) {
    const TextureInfo& t = stats.textures[i];
    if (!texture_valid(t)) return fail(BudgetVerdict::BadTexture, i, tex_total, 0, 0);
    if (t.width > limits.max_texture_dimension || t.height > limits.max_texture_dimension) {
      return fail(BudgetVerdict::TextureDimension, i, tex_total, 0, 0);
    }
    std::uint64_t bytes = 0;
    if (!bytes_of_valid(t, &bytes)) return fail(BudgetVerdict::SizeOverflow, i, tex_total, 0, 0);
    if (bytes > limits.max_texture_bytes_each) {
      return fail(BudgetVerdict::TextureTooLarge, i, tex_total, 0, 0);
    }
    std::uint64_t next = 0;
    if (!add_fits(tex_total, bytes, &next)) return fail(BudgetVerdict::SizeOverflow, i, tex_total, 0, 0);
    if (next > limits.max_texture_bytes_total) {
      return fail(BudgetVerdict::TextureBudget, i, tex_total, 0, 0);
    }
    tex_total = next;
  }

  // Scripts.
  for (std::size_t i = 0; i < stats.script_bytes.size(); ++i) {
    const std::uint64_t bytes = stats.script_bytes[i];
    if (bytes > limits.max_script_bytes_each) {
      return fail(BudgetVerdict::ScriptTooLarge, i, tex_total, script_total, 0);
    }
    std::uint64_t next = 0;
    if (!add_fits(script_total, bytes, &next)) {
      return fail(BudgetVerdict::SizeOverflow, i, tex_total, script_total, 0);
    }
    if (next > limits.max_script_bytes_total) {
      return fail(BudgetVerdict::ScriptBudget, i, tex_total, script_total, 0);
    }
    script_total = next;
  }

  // Nodes.
  if (stats.node_count > limits.max_nodes) {
    return fail(BudgetVerdict::TooManyNodes, 0, tex_total, script_total, 0);
  }

  // Audio.
  for (std::size_t i = 0; i < stats.audio.size(); ++i) {
    const AudioInfo& a = stats.audio[i];
    if (a.encoded_bytes > limits.max_audio_bytes_each) {
      return fail(BudgetVerdict::AudioTooLarge, i, tex_total, script_total, audio_total);
    }
    if (a.duration_ms > limits.max_audio_ms_each) {
      return fail(BudgetVerdict::AudioTooLong, i, tex_total, script_total, audio_total);
    }
    std::uint64_t next = 0;
    if (!add_fits(audio_total, a.encoded_bytes, &next)) {
      return fail(BudgetVerdict::SizeOverflow, i, tex_total, script_total, audio_total);
    }
    if (next > limits.max_audio_bytes_total) {
      return fail(BudgetVerdict::AudioBudget, i, tex_total, script_total, audio_total);
    }
    audio_total = next;
  }

  BudgetResult ok;
  ok.texture_bytes = tex_total;
  ok.script_bytes = script_total;
  ok.audio_bytes = audio_total;
  return ok;
}

const char* to_string(BudgetVerdict v) {
  switch (v) {
    case BudgetVerdict::Ok: return "Ok";
    case BudgetVerdict::TooManyTextures: return "TooManyTextures";
    case BudgetVerdict::BadTexture: return "BadTexture";
    case BudgetVerdict::TextureDimension: return "TextureDimension";
    case BudgetVerdict::TextureTooLarge: return "TextureTooLarge";
    case BudgetVerdict::TextureBudget: return "TextureBudget";
    case BudgetVerdict::ScriptTooLarge: return "ScriptTooLarge";
    case BudgetVerdict::ScriptBudget: return "ScriptBudget";
    case BudgetVerdict::TooManyNodes: return "TooManyNodes";
    case BudgetVerdict::AudioTooLarge: return "AudioTooLarge";
    case BudgetVerdict::AudioTooLong: return "AudioTooLong";
    case BudgetVerdict::AudioBudget: return "AudioBudget";
    case BudgetVerdict::SizeOverflow: return "SizeOverflow";
  }
  return "Unknown";
}

const char* to_string(TextureFormat f) {
  switch (f) {
    case TextureFormat::R8: return "R8";
    case TextureFormat::Rg8: return "Rg8";
    case TextureFormat::Rgba8: return "Rgba8";
    case TextureFormat::Bc1: return "Bc1";
    case TextureFormat::Bc7: return "Bc7";
  }
  return "Unknown";
}

}  // namespace lumora::package
