#include "nxrpg/chainload.hpp"

#include "nxrpg/paths.hpp"

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
#include <switch.h>
#endif

namespace nxrpg {

std::string Chainloader::format_core_argv(
    CoreKind core,
    std::string_view core_nro_path,
    std::string_view manifest_path) {
  if (core_nro_path.empty()) {
    return "";
  }
  switch (core) {
    case CoreKind::NxjsMvz:
      // CRITICAL: TooTallNate/nx.js treats Switch.argv[1] as a JS entrypoint override
      // replacing romfs:/main.js. Never pass positional argv[1] to nxjs-mvz.nro!
      return std::string(core_nro_path);

    case CoreKind::EasyRpg:
    case CoreKind::MkxpZ:
      if (manifest_path.empty()) {
        return std::string(core_nro_path);
      }
      return std::string(core_nro_path) + " --nxrpg-manifest " + std::string(manifest_path);

    case CoreKind::Unknown:
    default:
      return "";
  }
}

bool Chainloader::schedule_core_launch(const LaunchManifest& manifest) {
  std::string core_nro = paths::core_nro_path(manifest.game.core);
  if (core_nro.empty()) {
    return false;
  }
  std::string argv = format_core_argv(manifest.game.core, core_nro, paths::kLaunchManifest);
  if (argv.empty()) {
    return false;
  }

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  if (!envHasNextLoad()) {
    return false;
  }
  Result rc = envSetNextLoad(core_nro.c_str(), argv.c_str());
  return R_SUCCEEDED(rc);
#else
  (void)argv;
  return false;
#endif
}

bool Chainloader::schedule_return_to_launcher(std::string_view launcher_nro_path) {
  if (launcher_nro_path.empty()) {
    return false;
  }
  std::string target(launcher_nro_path);
  std::string argv = target + " --from-core";

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  if (!envHasNextLoad()) {
    return false;
  }
  Result rc = envSetNextLoad(target.c_str(), argv.c_str());
  return R_SUCCEEDED(rc);
#else
  (void)argv;
  return false;
#endif
}

}  // namespace nxrpg
