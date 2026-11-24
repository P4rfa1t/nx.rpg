#include "launcher/services/core_dispatcher.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <system_error>

#include "launcher/services/config_service.hpp"
#include "nxrpg/chainload.hpp"
#include "nxrpg/config_merger.hpp"
#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg::launcher {

namespace {

namespace fs = std::filesystem;

std::string core_filename_for(CoreKind core) {
  switch (core) {
    case CoreKind::EasyRpg:
      return "easyrpg.nro";
    case CoreKind::NxjsMvz:
      return "nxjs-mvz.nro";
    case CoreKind::MkxpZ:
      return "mkxp-z.nro";
    case CoreKind::Unknown:
    default:
      return "";
  }
}

std::string rtp_subdir_for(EngineKind engine) {
  switch (engine) {
    case EngineKind::Rm2k:
      return "rm2000";
    case EngineKind::Rm2k3:
      return "rm2003";
    case EngineKind::Rmxp:
      return "rmxp";
    case EngineKind::Rmvx:
      return "rmvx";
    case EngineKind::Rmvxa:
      return "rmvxa";
    default:
      return "";
  }
}

bool dir_has_real_files(const fs::path& dir) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return false;
  for (fs::directory_iterator it(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    std::string fname = it->path().filename().string();
    if (fname == ".gitkeep" || fname == "." || fname == "..") continue;
    return true;
  }
  return false;
}

std::string generate_session_id() {
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
  char buf[48];
  std::snprintf(buf, sizeof(buf), "nxrpg-%lld", static_cast<long long>(secs));
  return std::string(buf);
}

}  // namespace

LaunchPrecheckResult CoreDispatcher::precheck(
    const DetectedGame& game,
    MemoryExecutionMode memory_mode) {
  return precheck(game, memory_mode, fs::path(paths::kSdRoot));
}

LaunchPrecheckResult CoreDispatcher::precheck(
    const DetectedGame& game,
    MemoryExecutionMode memory_mode,
    const fs::path& sd_root) {
  std::error_code ec;
  if (game.game_path.empty() || !fs::exists(fs::path(game.game_path), ec)) {
    return {LaunchPrecheckStatus::ErrorGamePathMissing,
            "Game directory does not exist: " + game.game_path};
  }

  CoreKind target_core = (game.core != CoreKind::Unknown) ? game.core : core_for_engine(game.engine);
  std::string core_file = core_filename_for(target_core);
  if (core_file.empty() || !fs::is_regular_file(sd_root / "cores" / core_file, ec)) {
    return {LaunchPrecheckStatus::ErrorCoreNroMissing,
            "Missing core NRO: " + (sd_root / "cores" / core_file).generic_string()};
  }

  if (memory_mode == MemoryExecutionMode::AppletRestrictedRam &&
      (target_core == CoreKind::NxjsMvz || target_core == CoreKind::MkxpZ)) {
    return {LaunchPrecheckStatus::WarnAppletModeHighRamCore,
            "Applet mode (~440 MB) limits V8 JIT/WASM and large RGSS heaps. Launch via Title Takeover (hold R) for full RAM."};
  }

  if (game.needs_rtp) {
    PerGameConfig pg = ConfigService::load_per_game(sd_root, game.id);
    if (!pg.easyrpg_disable_rtp) {
      fs::path rtp_dir = pg.rtp_override_path.empty()
                             ? (sd_root / "rtp" / rtp_subdir_for(game.engine))
                             : fs::path(pg.rtp_override_path);
      if (!dir_has_real_files(rtp_dir)) {
        return {LaunchPrecheckStatus::WarnMissingSharedRtp,
                "Shared RTP directory is empty: " + rtp_dir.generic_string()};
      }
    }
  }

  return {LaunchPrecheckStatus::Ready, ""};
}

bool CoreDispatcher::dispatch(const DetectedGame& game) {
  return dispatch(game, fs::path(paths::kSdRoot), true);
}

bool CoreDispatcher::dispatch(
    const DetectedGame& game,
    const fs::path& sd_root,
    bool arm_chainload) {
  const fs::path global_cfg = sd_root / "config/global.json";
  const fs::path per_game_cfg = sd_root / "config/games" / (game.id + ".json");
  LaunchManifest manifest = ConfigMerger::build_launch_manifest(
      game, global_cfg, per_game_cfg, generate_session_id());

  // Keep relative sd_root paths coherent when running under a custom host/preview root
  if (sd_root.generic_string() != paths::kSdRoot) {
    manifest.return_to = (sd_root / "nx.rpg.nro").generic_string();
    manifest.log_path = (sd_root / "runtime/last_run.log").generic_string();
    manifest.game.save_path = (sd_root / "saves" / manifest.game.id).generic_string();
    if (!manifest.assets.rtp_path.empty()) {
      std::string rtp_sub = rtp_subdir_for(manifest.game.engine);
      if (!rtp_sub.empty() && manifest.assets.rtp_path == paths::default_rtp_path(manifest.game.engine)) {
        manifest.assets.rtp_path = (sd_root / "rtp" / rtp_sub).generic_string();
      }
    }
  }

  const fs::path manifest_path = sd_root / "runtime/launch.json";
  if (!manifest.save_to_file_atomic(manifest_path)) {
    return false;
  }

  (void)ConfigService::update_last_selected_game(sd_root, game.id);

  // Record launch start timestamp so `--from-core` updates playTimeSeconds accurately
  {
    auto now_secs = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    JsonValue sess = JsonValue::make_object();
    sess.set("gameId", manifest.game.id);
    sess.set("startedAt", static_cast<std::int64_t>(now_secs > 0 ? now_secs : 0));
    (void)paths::write_file_atomic(sd_root / "runtime/active_session.json", sess.dump(false));
  }

  if (!arm_chainload) {
    return true;
  }
  return Chainloader::schedule_core_launch(manifest);
}

}  // namespace nxrpg::launcher
