#include "launcher/app.hpp"

#include <filesystem>
#include <memory>

#include "launcher/services/config_service.hpp"
#include "launcher/services/environment_doctor.hpp"
#include "launcher/services/i18n_service.hpp"
#include "launcher/views/root_frame.hpp"
#include "nxrpg/logger.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher {

int LauncherApp::run(const BootContext& ctx) {
  NXLOG_INFO("app", "run() memory_mode=%d returned_from_core=%d sd_root=%s",
             static_cast<int>(ctx.memory_mode), ctx.returned_from_core ? 1 : 0,
             ctx.sd_root.c_str());
#ifdef NXRPG_HAS_BOREALIS
  if (!brls::Application::init()) {
    NXLOG_ERROR("app", "brls::Application::init failed");
    return 1;
  }

  GlobalConfig global_cfg = ConfigService::load_global(ctx.sd_root);
  NXLOG_INFO("app", "global config loaded: ui_language=%s last_game=%s",
             global_cfg.ui_language.c_str(),
             global_cfg.library.last_selected_game_id.c_str());
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
    if (finalize_active_session(ctx.sd_root)) {
      NXLOG_INFO("app", "finalized play session from runtime/active_session.json");
    } else {
      NXLOG_DEBUG("app", "no active session to finalize");
    }
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
      NXLOG_ERROR("app", "last core run reported FATAL [%s/%s]: %s",
                  log_sum.fatal_core.c_str(), log_sum.fatal_code.c_str(),
                  log_sum.fatal_message.c_str());
      brls::Application::notify(i18n->translate("app.last_run_fatal"));
    } else {
      NXLOG_INFO("app", "last core run ended cleanly");
    }
  }

  while (brls::Application::mainLoop()) {
    // Borealis event loop
  }
  NXLOG_INFO("app", "main loop exited");
  return 0;
#else
  (void)ctx;
  return 0;
#endif
}
}  // namespace nxrpg::launcher
