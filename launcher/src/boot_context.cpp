#include "launcher/app.hpp"

#include <string_view>

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
#include <switch.h>
#endif

namespace nxrpg::launcher {

BootContext LauncherApp::parse_boot_context(int argc, const char* const* argv) {
  BootContext ctx;

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  AppletType at = appletGetAppletType();
  if (at == AppletType_Application || at == AppletType_SystemApplication) {
    ctx.memory_mode = MemoryExecutionMode::ApplicationFullRam;
  } else {
    ctx.memory_mode = MemoryExecutionMode::AppletRestrictedRam;
  }
#else
  ctx.memory_mode = MemoryExecutionMode::HostDesktop;
#endif

  if (!argv || argc <= 1) {
    return ctx;
  }

  for (int i = 1; i < argc; ++i) {
    if (!argv[i]) continue;
    std::string_view arg(argv[i]);
    if (arg == "--from-core") {
      ctx.returned_from_core = true;
    } else if (arg == "--simulate-applet") {
      ctx.memory_mode = MemoryExecutionMode::AppletRestrictedRam;
    } else if (arg == "--sd-root" && i + 1 < argc && argv[i + 1]) {
      ctx.sd_root = argv[++i];
    } else if (arg.rfind("--sd-root=", 0) == 0 && arg.size() > 10) {
      ctx.sd_root = std::string(arg.substr(10));
    }
  }

  return ctx;
}

}  // namespace nxrpg::launcher
