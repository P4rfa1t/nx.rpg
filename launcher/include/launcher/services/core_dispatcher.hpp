#pragma once

#include <filesystem>
#include <string>

#include "nxrpg/engine_detector.hpp"
#include "nxrpg/paths.hpp"
#include "nxrpg/types.hpp"

namespace nxrpg::launcher {

enum class LaunchPrecheckStatus {
  Ready = 0,
  WarnAppletModeHighRamCore,  // Applet Mode (~440 MB) + nxjs-mvz or mkxp-z (WASM/JIT/RAM risk)
  WarnMissingSharedRtp,       // Game likely requires RTP, and rtp/<engine>/ is empty
  ErrorCoreNroMissing,        // Target cores/<core>.nro does not exist on SD card
  ErrorGamePathMissing,       // Target game directory disappeared
  ErrorManifestWriteFailed,   // Failed atomic write to runtime/launch.json
};

struct LaunchPrecheckResult {
  LaunchPrecheckStatus status = LaunchPrecheckStatus::Ready;
  std::string detail_message;
};

/// Validates pre-conditions, writes `runtime/launch.json` atomically, and arms `envSetNextLoad`.
class CoreDispatcher {
 public:
  [[nodiscard]] static LaunchPrecheckResult precheck(
      const DetectedGame& game,
      MemoryExecutionMode memory_mode);

  [[nodiscard]] static LaunchPrecheckResult precheck(
      const DetectedGame& game,
      MemoryExecutionMode memory_mode,
      const std::filesystem::path& sd_root);

  /// Merges configs, writes `runtime/launch.json`, records `lastSelectedGameId`,
  /// arms `Chainloader::schedule_core_launch`, and returns whether chainload was armed.
  [[nodiscard]] static bool dispatch(const DetectedGame& game);

  [[nodiscard]] static bool dispatch(
      const DetectedGame& game,
      const std::filesystem::path& sd_root,
      bool arm_chainload = true);
};

}  // namespace nxrpg::launcher
