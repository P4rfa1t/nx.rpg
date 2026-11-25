#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "nxrpg/types.hpp"

namespace nxrpg::paths {

inline constexpr std::string_view kSdRoot          = "sdmc:/switch/nx.rpg";
inline constexpr std::string_view kLauncherNro     = "sdmc:/switch/nx.rpg/nx.rpg.nro";
inline constexpr std::string_view kCoresDir        = "sdmc:/switch/nx.rpg/cores";
inline constexpr std::string_view kGamesDir        = "sdmc:/switch/nx.rpg/games";
inline constexpr std::string_view kSavesDir        = "sdmc:/switch/nx.rpg/saves";
inline constexpr std::string_view kRtpDir          = "sdmc:/switch/nx.rpg/rtp";
inline constexpr std::string_view kFontsDir        = "sdmc:/switch/nx.rpg/fonts";
inline constexpr std::string_view kConfigDir       = "sdmc:/switch/nx.rpg/config";
inline constexpr std::string_view kGameConfigsDir  = "sdmc:/switch/nx.rpg/config/games";
inline constexpr std::string_view kGlobalConfig    = "sdmc:/switch/nx.rpg/config/global.json";
inline constexpr std::string_view kCacheDir        = "sdmc:/switch/nx.rpg/cache";
inline constexpr std::string_view kCoverCacheDir   = "sdmc:/switch/nx.rpg/cache/covers";
inline constexpr std::string_view kLibraryCache    = "sdmc:/switch/nx.rpg/cache/library.json";
inline constexpr std::string_view kRuntimeDir      = "sdmc:/switch/nx.rpg/runtime";
inline constexpr std::string_view kLaunchManifest  = "sdmc:/switch/nx.rpg/runtime/launch.json";
inline constexpr std::string_view kLastRunLog      = "sdmc:/switch/nx.rpg/runtime/last_run.log";
inline constexpr std::string_view kLogsDir         = "sdmc:/switch/nx.rpg/logs";
inline constexpr std::string_view kGameLogsDir     = "sdmc:/switch/nx.rpg/logs/games";
inline constexpr std::string_view kLauncherLog     = "sdmc:/switch/nx.rpg/logs/launcher.log";

/// Returns the per-game core run log path `logs/games/<game_id>.log`.
[[nodiscard]] inline std::string game_log_path(std::string_view game_id) {
  return std::string(kGameLogsDir) + "/" + std::string(game_id) + ".log";
}

/// Returns the canonical NRO path for a target core under `kCoresDir`.
[[nodiscard]] std::string core_nro_path(CoreKind core);

/// Returns the default shared RTP directory for an engine (empty for MV/MZ).
[[nodiscard]] std::string default_rtp_path(EngineKind engine);

/// Resolves `relative_path` inside `base_dir` case-insensitively, matching
/// Windows asset path behavior on FAT32/exFAT/POSIX filesystems.
[[nodiscard]] std::optional<std::filesystem::path> resolve_case_insensitive(
    const std::filesystem::path& base_dir,
    std::string_view relative_path);

/// Atomically writes `bytes` to `target_path` via `<target>.tmp` + fsync + rename
/// to prevent Horizon OS exFAT/FAT32 corruption on sudden power loss or crash.
[[nodiscard]] bool write_file_atomic(
    const std::filesystem::path& target_path,
    std::string_view bytes);

}  // namespace nxrpg::paths
