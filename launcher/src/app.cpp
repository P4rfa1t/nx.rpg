#include "launcher/app.hpp"

#include <filesystem>
#include <memory>

#include "launcher/services/config_service.hpp"
#include "launcher/services/environment_doctor.hpp"
#include "launcher/services/i18n_service.hpp"
#include "launcher/views/root_frame.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher {

int LauncherApp::run(const BootContext& ctx) {
#ifdef NXRPG_HAS_BOREALIS
  if (!brls::Application::init()) {
    return 1;
  }

  GlobalConfig global_cfg = ConfigService::load_global(ctx.sd_root);
  auto i18n = std::make_shared<I18nService>();
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  std::filesystem::path i18n_dir = "romfs:/nxrpg_i18n";
#else
  std::filesystem::path i18n_dir = std::filesystem::path(NXRPG_RESOURCES_DIR) / "i18n";
#endif
  (void)i18n->load_from_directory(i18n_dir, global_cfg.ui_language);

  // Clean up any orphaned `.tmp` files from an earlier crash before rendering UI
  (void)EnvironmentDoctor::cleanup_orphaned_tmp_files(ctx.sd_root);

  if (ctx.returned_from_core) {
    (void)finalize_active_session(ctx.sd_root);
  }

  brls::Application::createWindow(i18n->translate("app.title", "nx.rpg"));
  brls::Application::setGlobalQuit(false);

  auto* root = new views::RootFrame(ctx, i18n);
  brls::Application::pushActivity(new brls::Activity(root));

  if (ctx.memory_mode == MemoryExecutionMode::AppletRestrictedRam &&
      global_cfg.warn_on_applet_mode) {
    brls::Application::notify(i18n->translate("app.applet_warning"));
  }

  if (ctx.returned_from_core) {
    auto log_sum = EnvironmentDoctor::inspect_last_run_log(
        std::filesystem::path(ctx.sd_root) / "runtime/last_run.log");
    if (log_sum.has_fatal) {
      brls::Application::notify(i18n->translate("app.last_run_fatal"));
    }
  }

  while (brls::Application::mainLoop()) {
    // Borealis event loop
  }
  return 0;
#else
  (void)ctx;
  return 0;
#endif
}

}  // namespace nxrpg::launcher
