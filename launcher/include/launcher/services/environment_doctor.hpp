#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nxrpg/paths.hpp"
#include "nxrpg/types.hpp"

namespace nxrpg::launcher {

struct DiagnosticItem {
  enum class Severity { Ok, Warning, Error } severity = Severity::Ok;
  std::string component;  // e.g. "Memory Mode", "Core: nxjs-mvz", "RTP: rm2003", "Font: CJK", "SD Filesystem"
  std::string summary;
  std::string remediation_hint;
};

struct LastRunLogSummary {
  bool exists = false;
  bool has_fatal = false;
  std::string fatal_line;
  std::string fatal_core;
  std::string fatal_game;
  std::string fatal_code;
  std::string fatal_message;
  std::vector<std::string> tail_lines;
};

/// Inspects Switch runtime health: Applet vs Full RAM, core NRO presence,
/// 5 shared RTP pools, CJK fallback fonts, orphaned `.tmp` cleanup, and `last_run.log`.
class EnvironmentDoctor {
 public:
  [[nodiscard]] static std::vector<DiagnosticItem> run_all_checks(
      MemoryExecutionMode memory_mode);

  [[nodiscard]] static std::vector<DiagnosticItem> run_all_checks(
      MemoryExecutionMode memory_mode,
      const std::filesystem::path& sd_root);

  /// Counts (without deleting) orphaned `*.tmp` files in config/cache/runtime/saves.
  static std::size_t count_orphaned_tmp_files(const std::filesystem::path& sd_root);

  /// Removes orphaned `*.tmp` files left by crashed writes in config/cache/runtime/saves.
  static std::size_t cleanup_orphaned_tmp_files(const std::filesystem::path& sd_root);

  /// Parses a core run log for `[nx.rpg][FATAL]` diagnostics and last N lines.
  [[nodiscard]] static LastRunLogSummary inspect_last_run_log(
      const std::filesystem::path& log_path,
      std::size_t max_tail_lines = 40);

  /// Returns the most recently modified `logs/games/<game_id>.log`, or
  /// nullopt when the per-game logs directory does not exist or is empty.
  [[nodiscard]] static std::optional<std::filesystem::path> most_recent_game_log(
      const std::filesystem::path& game_logs_dir);

  /// Lists every per-game run log under `logs/games/` sorted by last write
  /// time (newest first), paired with the game id parsed from the filename.
  [[nodiscard]] static std::vector<std::pair<std::string, std::filesystem::path>> list_game_logs(
      const std::filesystem::path& game_logs_dir);
};

}  // namespace nxrpg::launcher
