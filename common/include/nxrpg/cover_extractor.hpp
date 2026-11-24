#pragma once

#include <filesystem>

#include "nxrpg/engine_detector.hpp"

namespace nxrpg {

/// Extracts and caches unencrypted or archive-encrypted title/cover artwork
/// into `cache/covers/<game.id>.<ext>` using atomic writes.
///
/// Supported sources:
/// 1. Direct unencrypted title/icon (`game.cover_source_path`)
/// 2. RPG Maker MV/MZ encrypted title images (`.rpgmvp`, `.png_`) using `System.json -> encryptionKey`
/// 3. RPG Maker XP/VX (`RGSSAD\0\x01`) and VX Ace (`RGSSAD\0\x03`) encrypted archives
class CoverExtractor {
 public:
  /// Returns the cached cover path if already present or newly extracted,
  /// or an empty path if no suitable cover image exists.
  [[nodiscard]] static std::filesystem::path extract_to_cache(
      const DetectedGame& game,
      const std::filesystem::path& cover_cache_dir);
};

}  // namespace nxrpg
