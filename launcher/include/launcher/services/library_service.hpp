#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "nxrpg/engine_detector.hpp"

namespace nxrpg::launcher {

struct LibraryEntry {
  DetectedGame game;
  std::string cover_cache_path;
  bool favorite = false;
  std::uint64_t last_played_at = 0;
  std::uint64_t play_time_seconds = 0;
};

/// Manages incremental SD library scanning and `cache/library.json` persistence.
class LibraryService {
 public:
  explicit LibraryService(std::filesystem::path sd_root);

  /// Fast path used on `--from-core`: loads `cache/library.json` and only
  /// re-inspects game folders whose directory `mtime` changed.
  [[nodiscard]] std::vector<DetectedGame> load_or_refresh(bool force_full_rescan = false);

  /// Full cached entry view including favorites, play time, and last-played timestamps.
  [[nodiscard]] std::vector<LibraryEntry> load_or_refresh_entries(bool force_full_rescan = false);

  /// Reads `cache/library.json` without hitting `games/` at all (instant `--from-core` load).
  [[nodiscard]] std::vector<LibraryEntry> load_cached_entries() const;

  /// Persists entries atomically to `cache/library.json` (schema v1).
  [[nodiscard]] bool save_entries(const std::vector<LibraryEntry>& entries) const;

  /// Updates `lastPlayedAt` (and optional elapsed seconds) for `game_id` in `cache/library.json`.
  [[nodiscard]] bool record_play_session(
      std::string_view game_id,
      std::uint64_t played_at_epoch,
      std::uint64_t added_seconds = 0);

  /// Toggles favorite status for `game_id` in `cache/library.json`.
  [[nodiscard]] bool toggle_favorite(std::string_view game_id);

  /// Generates or retrieves cached thumbnail under `cache/covers/<id>.jpg` (or source image path).
  [[nodiscard]] std::filesystem::path ensure_cover_thumbnail(const DetectedGame& game);

  [[nodiscard]] const std::filesystem::path& sd_root() const noexcept { return sd_root_; }

 private:
  std::filesystem::path sd_root_;
};

}  // namespace nxrpg::launcher
