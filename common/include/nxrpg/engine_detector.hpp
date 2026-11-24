#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "nxrpg/types.hpp"

namespace nxrpg {

struct DetectedGame {
  std::string id;                // Deterministic slug + short hash of normalized relative path
  std::string title;             // Decoded UTF-8 title from System.json / Game.ini / RPG_RT.ini
  EngineKind engine = EngineKind::Unknown;
  CoreKind core = CoreKind::Unknown;
  std::string game_path;         // Outer folder under sdmc:/switch/nx.rpg/games/
  std::string content_root;      // Actual engine asset root (e.g. <game_path>/www for MV)
  std::string exec_name = "Game";// Stem of renamed <Stem>.ini / <Stem>.rgssad
  TextEncoding detected_encoding = TextEncoding::Auto;
  std::string cover_source_path; // Candidate unencrypted title/icon image path if available
  std::uint64_t dir_mtime = 0;
  bool has_encrypted_archive = false;
  bool needs_rtp = false;
};

struct ScanOptions {
  std::uint32_t max_depth = 2;
  TextEncoding fallback_encoding = TextEncoding::Auto;
};

/// Stateless, host-testable 7-generation RPG Maker directory detector.
/// Precedence rules are specified in docs/ENGINE_DETECTION.md.
class EngineDetector {
 public:
  /// Inspects a single candidate game folder (allowing 1-level wrapper unwrap).
  [[nodiscard]] static std::optional<DetectedGame> inspect_directory(
      const std::filesystem::path& candidate_dir,
      const ScanOptions& options = {});

  /// Recursively scans `games_root` up to `options.max_depth`.
  [[nodiscard]] static std::vector<DetectedGame> scan_library(
      const std::filesystem::path& games_root,
      const ScanOptions& options = {});

  /// Computes deterministic `game.id` from a path relative to `games_root`.
  [[nodiscard]] static std::string compute_game_id(std::string_view relative_game_path);
};

}  // namespace nxrpg
