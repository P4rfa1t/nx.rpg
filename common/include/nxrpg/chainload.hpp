#pragma once

#include <string>
#include <string_view>

#include "nxrpg/launch_manifest.hpp"
#include "nxrpg/types.hpp"

namespace nxrpg {

/// Abstracts libnx `envSetNextLoad()` handoff between `nx.rpg.nro` and `cores/*.nro`.
class Chainloader {
 public:
  /// Builds the exact `argv` string required by each core's entrypoint rules:
  /// - `easyrpg` & `mkxp-z`: `<core.nro> --nxrpg-manifest <manifest_path>`
  /// - `nxjs-mvz`: `<nxjs-mvz.nro>` (NO positional `argv[1]`, because nx.js interprets
  ///   `Switch.argv[1]` as the JS entrypoint override instead of `romfs:/main.js`!).
  [[nodiscard]] static std::string format_core_argv(
      CoreKind core,
      std::string_view core_nro_path,
      std::string_view manifest_path);

  /// Schedules next NRO load via `envSetNextLoad` (no-op returning false if unavailable).
  [[nodiscard]] static bool schedule_core_launch(const LaunchManifest& manifest);

  /// Called by cores during clean shutdown to return to `manifest.return_to` (`--from-core`).
  [[nodiscard]] static bool schedule_return_to_launcher(std::string_view launcher_nro_path);
};

}  // namespace nxrpg
