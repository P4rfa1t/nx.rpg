#include "launcher/app.hpp"
#include "launcher/services/config_service.hpp"
#include "launcher/services/core_dispatcher.hpp"
#include "launcher/services/environment_doctor.hpp"
#include "launcher/services/i18n_service.hpp"
#include "launcher/services/library_service.hpp"
#include "nxrpg/launch_manifest.hpp"
#include "nxrpg/paths.hpp"
#include "test_common.hpp"

#include <filesystem>

using namespace nxrpg;
using namespace nxrpg::launcher;

int main() {
  nxrpg::test::TempDir root("launcher_services");
  const auto sd = root.path();

  // Prepare SD skeleton
  std::filesystem::create_directories(sd / "games");
  std::filesystem::create_directories(sd / "cores");
  std::filesystem::create_directories(sd / "rtp/rm2000");
  std::filesystem::create_directories(sd / "rtp/rm2003");
  std::filesystem::create_directories(sd / "rtp/rmxp");
  std::filesystem::create_directories(sd / "rtp/rmvx");
  std::filesystem::create_directories(sd / "rtp/rmvxa");
  std::filesystem::create_directories(sd / "fonts");

  // 1. ConfigService global & per-game persistence
  {
    GlobalConfig g;
    g.ui_language = "en-US";
    g.default_encoding = TextEncoding::ShiftJis;
    g.performance.default_cpu_profile = CpuProfile::Boost1581;
    g.video.integer_scale = true;
    g.library.max_scan_depth = 3;
    g.library.sort_order = "title";
    NXRPG_CHECK(ConfigService::save_global(sd, g));

    GlobalConfig loaded = ConfigService::load_global(sd);
    NXRPG_CHECK(loaded.ui_language == "en-US");
    NXRPG_CHECK(loaded.default_encoding == TextEncoding::ShiftJis);
    NXRPG_CHECK(loaded.performance.default_cpu_profile == CpuProfile::Boost1581);
    NXRPG_CHECK(loaded.video.integer_scale == true);
    NXRPG_CHECK(loaded.library.max_scan_depth == 3);
    NXRPG_CHECK(loaded.library.sort_order == "title");

    PerGameConfig pg;
    pg.display_title = "Custom Title";
    pg.encoding = TextEncoding::Gbk;
    pg.cpu_profile = "boost_1785";
    pg.easyrpg_disable_rtp = true;
    NXRPG_CHECK(ConfigService::save_per_game(sd, "sample-rm2k-112233", pg));

    PerGameConfig loaded_pg = ConfigService::load_per_game(sd, "sample-rm2k-112233");
    NXRPG_CHECK(loaded_pg.display_title == "Custom Title");
    NXRPG_CHECK(loaded_pg.encoding == TextEncoding::Gbk);
    NXRPG_CHECK(loaded_pg.cpu_profile == "boost_1785");
    NXRPG_CHECK(loaded_pg.easyrpg_disable_rtp == true);
  }

  // 2. LibraryService full scan + cache persistence + incremental refresh
  {
    const auto g1 = sd / "games/SampleMV";
    nxrpg::test::write_bytes(g1 / "www/js/rpg_core.js", "// mv");
    nxrpg::test::write_bytes(g1 / "www/data/System.json", R"({"gameTitle":"Alpha MV"})");
    nxrpg::test::write_bytes(g1 / "www/img/titles1/Title.png", "PNG");

    const auto g2 = sd / "games/Sample2K";
    nxrpg::test::write_bytes(g2 / "RPG_RT.ldb", "\x0B" "LcfDataBase\x00");
    nxrpg::test::write_bytes(g2 / "RPG_RT.lmt", "\x0A" "LcfMapTree");
    nxrpg::test::write_bytes(g2 / "RPG_RT.ini", "[RPG_RT]\r\nGameTitle=Beta 2K\r\n");

    LibraryService svc(sd);
    auto first = svc.load_or_refresh_entries(false);
    NXRPG_CHECK(first.size() == 2);
    NXRPG_CHECK(std::filesystem::is_regular_file(sd / "cache/library.json"));

    std::string mv_id;
    std::string k2_id;
    for (const auto& e : first) {
      if (e.game.engine == EngineKind::Rmmv) mv_id = e.game.id;
      if (e.game.engine == EngineKind::Rm2k) k2_id = e.game.id;
    }
    NXRPG_CHECK(!mv_id.empty() && !k2_id.empty());

    NXRPG_CHECK(svc.toggle_favorite(mv_id));
    NXRPG_CHECK(svc.record_play_session(mv_id, 1732440000ULL, 120));

    // Incremental refresh should preserve favorite & play stats and keep both games
    auto second = svc.load_or_refresh_entries(false);
    NXRPG_CHECK(second.size() == 2);
    for (const auto& e : second) {
      if (e.game.id == mv_id) {
        NXRPG_CHECK(e.favorite == true);
        NXRPG_CHECK(e.last_played_at == 1732440000ULL);
        NXRPG_CHECK(e.play_time_seconds == 120);
        auto thumb = svc.ensure_cover_thumbnail(e.game);
        NXRPG_CHECK(!thumb.empty());
      }
    }
  }

  // 3. CoreDispatcher precheck & dispatch
  {
    LibraryService svc(sd);
    auto games = svc.load_or_refresh(false);
    DetectedGame mv_game;
    DetectedGame k2_game;
    for (const auto& g : games) {
      if (g.engine == EngineKind::Rmmv) mv_game = g;
      if (g.engine == EngineKind::Rm2k) k2_game = g;
    }

    // Missing core NRO
    auto r1 = CoreDispatcher::precheck(mv_game, MemoryExecutionMode::ApplicationFullRam, sd);
    NXRPG_CHECK(r1.status == LaunchPrecheckStatus::ErrorCoreNroMissing);

    // Create dummy core NROs
    nxrpg::test::write_bytes(sd / "cores/nxjs-mvz.nro", "NRO0");
    nxrpg::test::write_bytes(sd / "cores/easyrpg.nro", "NRO0");

    // Applet mode warning for nxjs-mvz
    auto r2 = CoreDispatcher::precheck(mv_game, MemoryExecutionMode::AppletRestrictedRam, sd);
    NXRPG_CHECK(r2.status == LaunchPrecheckStatus::WarnAppletModeHighRamCore);

    // Ready in Application mode
    auto r3 = CoreDispatcher::precheck(mv_game, MemoryExecutionMode::ApplicationFullRam, sd);
    NXRPG_CHECK(r3.status == LaunchPrecheckStatus::Ready);

    // RM2k missing shared RTP warning when needs_rtp == true and rtp/rm2000 only has .gitkeep
    nxrpg::test::write_bytes(sd / "rtp/rm2000/.gitkeep", "");
    auto r4 = CoreDispatcher::precheck(k2_game, MemoryExecutionMode::AppletRestrictedRam, sd);
    NXRPG_CHECK(r4.status == LaunchPrecheckStatus::WarnMissingSharedRtp);

    // Populated RTP clears warning
    nxrpg::test::write_bytes(sd / "rtp/rm2000/RPG_RT.exe", "dummy");
    auto r5 = CoreDispatcher::precheck(k2_game, MemoryExecutionMode::AppletRestrictedRam, sd);
    NXRPG_CHECK(r5.status == LaunchPrecheckStatus::Ready);

    // Dispatch writes runtime/launch.json, runtime/active_session.json, and updates lastSelectedGameId
    NXRPG_CHECK(CoreDispatcher::dispatch(mv_game, sd, false));
    auto manifest = LaunchManifest::load_from_file(sd / "runtime/launch.json");
    NXRPG_CHECK(manifest.has_value());
    if (manifest) {
      NXRPG_CHECK(manifest->game.id == mv_game.id);
      NXRPG_CHECK(manifest->game.core == CoreKind::NxjsMvz);
      NXRPG_CHECK(manifest->runtime.cpu_profile == CpuProfile::Boost1581);
      // Per-game run log contract: logPath points into logs/games/<id>.log
      NXRPG_CHECK(manifest->log_path == (sd / "logs/games" / (mv_game.id + ".log")).generic_string() ||
                  manifest->log_path ==
                      paths::game_log_path(mv_game.id));
    }
    NXRPG_CHECK(std::filesystem::is_regular_file(sd / "runtime/active_session.json"));

    // Simulate returning via `--from-core` after 45 seconds
    NXRPG_CHECK(LauncherApp::finalize_active_session(sd, 9999999999ULL));
    NXRPG_CHECK(!std::filesystem::exists(sd / "runtime/active_session.json"));

    GlobalConfig updated = ConfigService::load_global(sd);
    NXRPG_CHECK(updated.library.last_selected_game_id == mv_game.id);

    // Regression: dispatch must succeed on a REWRITE of launch.json too
    // (Horizon fsdev rename() does not clobber; write_file_atomic removes first).
    NXRPG_CHECK(CoreDispatcher::dispatch(mv_game, sd, false));
    NXRPG_CHECK(CoreDispatcher::dispatch(mv_game, sd, false));
    auto manifest2 = LaunchManifest::load_from_file(sd / "runtime/launch.json");
    NXRPG_CHECK(manifest2.has_value());
  }

  // 4b. Default cpu_profile is system_default (never touches clkrst)
  {
    GlobalConfig def;
    NXRPG_CHECK(def.performance.default_cpu_profile == CpuProfile::SystemDefault);
    NXRPG_CHECK(to_string(CpuProfile::SystemDefault) == "system_default");
    auto parsed = parse_cpu_profile("system_default");
    NXRPG_CHECK(parsed.has_value() && *parsed == CpuProfile::SystemDefault);
    LaunchManifest m;
    NXRPG_CHECK(m.runtime.cpu_profile == CpuProfile::SystemDefault);
  }

  // 4. EnvironmentDoctor checks, .tmp cleanup & [nx.rpg][FATAL] parser
  {
    nxrpg::test::write_bytes(sd / "runtime/stale.json.tmp", "partial");
    // Per-game run log: logs/games/<id>.log (manifest logPath target)
    nxrpg::test::write_bytes(
        sd / "logs/games/touhou-xp-91b2.log",
        "info line 1\n[nx.rpg][FATAL][core=mkxp-z][game=touhou-xp-91b2][code=SCRIPT_ERR] NoMethodError: foo\n");

    auto items = EnvironmentDoctor::run_all_checks(MemoryExecutionMode::AppletRestrictedRam, sd);
    NXRPG_CHECK(!items.empty());
    NXRPG_CHECK(!std::filesystem::exists(sd / "runtime/stale.json.tmp"));

    bool saw_applet_warn = false;
    bool saw_fatal_err = false;
    for (const auto& it : items) {
      if (it.component == "Memory Mode" && it.severity == DiagnosticItem::Severity::Warning) {
        saw_applet_warn = true;
      }
      if (it.component == "Game Log: touhou-xp-91b2" && it.severity == DiagnosticItem::Severity::Error) {
        saw_fatal_err = true;
      }
    }
    NXRPG_CHECK(saw_applet_warn);
    NXRPG_CHECK(saw_fatal_err);

    auto log_sum = EnvironmentDoctor::inspect_last_run_log(sd / "logs/games/touhou-xp-91b2.log");
    NXRPG_CHECK(log_sum.exists && log_sum.has_fatal);
    NXRPG_CHECK(log_sum.fatal_core == "mkxp-z");
    NXRPG_CHECK(log_sum.fatal_game == "touhou-xp-91b2");
    NXRPG_CHECK(log_sum.fatal_code == "SCRIPT_ERR");
    NXRPG_CHECK(log_sum.fatal_message == "NoMethodError: foo");

    // list_game_logs / most_recent_game_log on the per-game logs dir
    auto logs = EnvironmentDoctor::list_game_logs(sd / "logs/games");
    NXRPG_CHECK(logs.size() == 1);
    NXRPG_CHECK(!logs.empty() && logs[0].first == "touhou-xp-91b2");
    auto recent = EnvironmentDoctor::most_recent_game_log(sd / "logs/games");
    NXRPG_CHECK(recent.has_value());
    NXRPG_CHECK(!recent || recent->filename() == "touhou-xp-91b2.log");
  }

  // 5. I18nService & BootContext
  {
    I18nService i18n;
    NXRPG_CHECK(i18n.load_from_directory(NXRPG_I18N_DIR, "zh-CN"));
    NXRPG_CHECK(!i18n.translate("tabs.library").empty());
    NXRPG_CHECK(i18n.translate("missing.key", "fallback") == "fallback");

    const char* argv[] = {"nx.rpg.nro", "--from-core", "--simulate-applet", "--sd-root", "/tmp/nx"};
    BootContext ctx = LauncherApp::parse_boot_context(5, argv);
    NXRPG_CHECK(ctx.returned_from_core);
    NXRPG_CHECK(ctx.memory_mode == MemoryExecutionMode::AppletRestrictedRam);
    NXRPG_CHECK(ctx.sd_root == "/tmp/nx");
  }

  return nxrpg::test::finish();
}
