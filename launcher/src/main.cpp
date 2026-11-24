#include "launcher/app.hpp"

#include <string>
#include <string_view>

#include "nxrpg/logger.hpp"
#include "nxrpg/paths.hpp"

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
#include <switch.h>
#endif

int main(int argc, char* argv[]) {
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  romfsInit();

  // Live debug mirror over nxlink when a host is listening; file logging below.
  if (R_SUCCEEDED(socketInitializeDefault())) {
    if (nxlinkStdio() >= 0) {
      // stdout/stderr now stream to the nxlink host; keep the mirror on.
    } else {
      socketExit();
      nxrpg::Logger::instance().set_stderr_mirror(false);
    }
  } else {
    nxrpg::Logger::instance().set_stderr_mirror(false);
  }
#endif

  const char* sd_root_arg = "sdmc:/switch/nx.rpg";
  for (int i = 1; i < argc; ++i) {
    if (argv[i] && std::string_view(argv[i]).rfind("--sd-root=", 0) == 0) {
      sd_root_arg = argv[i] + 10;
    } else if (argv[i] && i + 1 < argc && std::string_view(argv[i]) == "--sd-root") {
      sd_root_arg = argv[i + 1];
    }
  }

  nxrpg::Logger& logger = nxrpg::Logger::instance();
  if (!logger.open_file(std::filesystem::path(sd_root_arg) / "runtime/launcher.log")) {
    // No writable SD root (e.g. host smoke run) — stderr-only logging.
    logger.set_stderr_mirror(true);
  }
  NXLOG_INFO("boot", "nx.rpg launcher starting");

  nxrpg::launcher::BootContext ctx =
      nxrpg::launcher::LauncherApp::parse_boot_context(argc, argv);

  nxrpg::launcher::LauncherApp app;
  int rc = app.run(ctx);

  NXLOG_INFO("boot", std::string("nx.rpg launcher exiting with code ").append(std::to_string(rc)).c_str());
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  romfsExit();
#endif
  return rc;
}
