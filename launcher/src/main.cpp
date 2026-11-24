#include "launcher/app.hpp"

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
#include <switch.h>
#endif

int main(int argc, char* argv[]) {
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  romfsInit();
#endif

  nxrpg::launcher::BootContext ctx =
      nxrpg::launcher::LauncherApp::parse_boot_context(argc, argv);

  nxrpg::launcher::LauncherApp app;
  int rc = app.run(ctx);

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  romfsExit();
#endif
  return rc;
}
