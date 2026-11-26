// scene.json (schema version 1, first draft) and the package loader over an in-memory file map.
// The loader verifies every file against checksums.txt before it parses or uses any content.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "manifest.h"

namespace lumora::scene {

struct Transform {
  float position[3] = {0, 0, 0};
  float scale[3] = {1, 1, 1};
  float rotation_z_degrees = 0;
};

struct Node {
  std::string id;      // stable, unique inside the scene
  std::string name;    // display only
  Transform transform;
  std::string layer_asset;     // asset id of a texture drawn by this node, empty when none
  float parallax_depth = 0;    // 0 = moves with the device, 1 = far background (layer nodes)
  std::vector<Node> children;  // strict tree
};

struct Asset {
  std::string id;    // stable asset id
  std::string kind;  // "texture" in this draft
  std::string path;  // package-relative, must be present and checksummed
};

struct Scene {
  std::vector<Asset> assets;
  Node root;
};

enum class SceneError { None, NotJson, WrongType, MissingField, BadValue, DuplicateId, UnknownAsset, TooDeep };

struct SceneResult {
  SceneError error = SceneError::None;
  std::string message;
  Scene scene;
  bool ok() const { return error == SceneError::None; }
};

SceneResult parse_scene(std::string_view json_text, unsigned max_node_depth = 32);

// Reserved entry name for the future package signature block (docs/security-model.md, Authenticity).
inline constexpr const char* kSignatureEntry = "signature.lumora";

// Largest script file the loader accepts (the script host applies the same cap).
inline constexpr std::size_t kMaxScriptBytes = 64 * 1024;

using FileMap = std::map<std::string, std::vector<std::uint8_t>>;

enum class LoadError {
  None,
  MissingChecksums,
  BadChecksums,
  ChecksumMismatch,   // a listed file differs from its digest
  UnlistedFile,       // a file is present but not covered by checksums.txt
  MissingFile,        // listed or referenced file absent
  SignatureUnsupported,  // reserved signature entry present; this loader cannot verify signatures
  BadManifest,
  BadScene,
  ResourceBudget      // a load-time budget (texture memory, scripts, nodes) is exceeded; see core/package/load_budget.h
};

struct LoadResult {
  LoadError error = LoadError::None;
  std::string message;
  Manifest manifest;
  Scene scene;
  bool ok() const { return error == LoadError::None; }
};

LoadResult load_package(const FileMap& files, const EngineSupport& support = {});

}  // namespace lumora::scene
