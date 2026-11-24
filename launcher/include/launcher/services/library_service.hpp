#pragma once

#include <filesystem>
#include <vector>

#include "nxrpg/engine_detector.hpp"

namespace nxrpg::launcher {

/// Manages incremental SD library scanning and `cache/library.json` persistence.
class LibraryService {
 public:
  explicit LibraryService(std::filesystem::path sd_root);

  /// Fast path used on `--from-core`: loads `cache/library.json` and only
  /// re-inspects game folders whose directory `mtime` changed.
  [[nodiscard]] std::vector<DetectedGame> load_or_refresh(bool force_full_rescan = false);

  /// Generates or retrieves cached 256x256 / 16:9 thumbnail under `cache/covers/<id>.jpg`.
  [[nodiscard]] std::filesystem::path ensure_cover_thumbnail(const DetectedGame& game);

 private:
  std::filesystem::path sd_root_;
};

}  // namespace nxrpg::launcher
