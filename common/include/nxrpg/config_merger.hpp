#pragma once

#include <filesystem>
#include <string_view>

#include "nxrpg/engine_detector.hpp"
#include "nxrpg/launch_manifest.hpp"

namespace nxrpg {

/// Merges built-in defaults -> `config/global.json` -> `config/games/<id>.json`
/// together with `DetectedGame` metadata to produce a validated `LaunchManifest`.
class ConfigMerger {
 public:
  [[nodiscard]] static LaunchManifest build_launch_manifest(
      const DetectedGame& game,
      const std::filesystem::path& global_config_path,
      const std::filesystem::path& per_game_config_path,
      std::string_view session_id);
};

}  // namespace nxrpg
