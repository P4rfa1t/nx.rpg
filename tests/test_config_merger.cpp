#include "nxrpg/config_merger.hpp"
#include "nxrpg/launch_manifest.hpp"
#include "nxrpg/paths.hpp"
#include "test_common.hpp"

using namespace nxrpg;

int main() {
  nxrpg::test::TempDir root("config_merger");

  const auto global_cfg = root.path() / "global.json";
  const auto game_cfg = root.path() / "games/touhou-xp.json";

  nxrpg::test::write_bytes(global_cfg, R"({
    "schemaVersion": 1,
    "uiLanguage": "zh-CN",
    "defaultEncoding": "shift_jis",
    "fallbackFontPath": "sdmc:/switch/nx.rpg/fonts/CustomCJK.ttc",
    "warnOnAppletMode": true,
    "performance": { "defaultCpuProfile": "boost_1581" },
    "video": { "scalingMode": "stretch", "integerScale": false, "vsync": true, "showFps": false },
    "input": { "profile": "default", "swapConfirmCancel": true, "enableTouch": true, "exitCombo": ["L", "R", "MINUS"] },
    "library": { "maxScanDepth": 2, "sortOrder": "recent", "lastSelectedGameId": "" }
  })");

  nxrpg::test::write_bytes(game_cfg, R"({
    "schemaVersion": 1,
    "displayTitle": "Override Title",
    "execNameOverride": "CustomExec",
    "encoding": "gbk",
    "rtpOverridePath": "sdmc:/switch/nx.rpg/rtp/custom",
    "performance": { "cpuProfile": "boost_1785" },
    "video": { "scalingMode": "fit", "integerScale": true, "showFps": true },
    "input": { "swapConfirmCancel": false },
    "mkxpOptions": {
      "enableWin32ApiStubs": true,
      "solidFonts": true,
      "smoothScaling": 2,
      "frameSkip": true,
      "subImageFix": true
    }
  })");

  DetectedGame game;
  game.id = "touhou-xp-123456";
  game.title = "Raw Title";
  game.engine = EngineKind::Rmxp;
  game.core = CoreKind::MkxpZ;
  game.game_path = "sdmc:/switch/nx.rpg/games/TouhouXP";
  game.content_root = "sdmc:/switch/nx.rpg/games/TouhouXP";
  game.exec_name = "Game";
  game.detected_encoding = TextEncoding::ShiftJis;

  LaunchManifest m = ConfigMerger::build_launch_manifest(game, global_cfg, game_cfg, "sess-42");

  // Verify 3-layer merge priority
  NXRPG_CHECK(m.schema_version == 1);
  NXRPG_CHECK(m.session_id == "sess-42");
  NXRPG_CHECK(m.game.id == "touhou-xp-123456");
  NXRPG_CHECK(m.game.title == "Override Title");
  NXRPG_CHECK(m.game.exec_name == "CustomExec");
  NXRPG_CHECK(m.game.save_path == "sdmc:/switch/nx.rpg/saves/touhou-xp-123456");
  NXRPG_CHECK(m.assets.rtp_path == "sdmc:/switch/nx.rpg/rtp/custom");
  NXRPG_CHECK(m.assets.fallback_font_path == "sdmc:/switch/nx.rpg/fonts/CustomCJK.ttc");
  NXRPG_CHECK(m.runtime.encoding == TextEncoding::Gbk);
  NXRPG_CHECK(m.runtime.rgss_version == 1);
  NXRPG_CHECK(m.runtime.cpu_profile == CpuProfile::Boost1785);
  NXRPG_CHECK(m.video.scaling_mode == ScalingMode::Fit);
  NXRPG_CHECK(m.video.integer_scale == true);
  NXRPG_CHECK(m.video.vsync == true);       // inherited from global
  NXRPG_CHECK(m.video.show_fps == true);    // overridden by per-game
  NXRPG_CHECK(m.input.swap_confirm_cancel == false);  // per-game overrides global's true
  NXRPG_CHECK(m.input.exit_combo.size() == 3 && m.input.exit_combo[2] == "MINUS");
  NXRPG_CHECK(m.core_options.mkxp.solid_fonts == true);
  NXRPG_CHECK(m.core_options.mkxp.smooth_scaling == 2);
  NXRPG_CHECK(m.core_options.mkxp.frame_skip == true);
  NXRPG_CHECK(m.core_options.mkxp.sub_image_fix == true);

  // Round-trip atomic save & load
  const auto manifest_path = root.path() / "runtime/launch.json";
  NXRPG_CHECK(m.save_to_file_atomic(manifest_path));
  auto loaded = LaunchManifest::load_from_file(manifest_path);
  NXRPG_CHECK(loaded.has_value());
  if (loaded) {
    NXRPG_CHECK(loaded->game.title == m.game.title);
    NXRPG_CHECK(loaded->runtime.encoding == TextEncoding::Gbk);
    NXRPG_CHECK(loaded->runtime.cpu_profile == CpuProfile::Boost1785);
    NXRPG_CHECK(loaded->core_options.mkxp.smooth_scaling == 2);
    NXRPG_CHECK(loaded->input.exit_combo == m.input.exit_combo);
  }

  // Verify cpuProfile="inherit" keeps global.json's boost_1581, and engineOverride updates core + rgssVersion
  const auto mv_cfg = root.path() / "games/inherit.json";
  nxrpg::test::write_bytes(mv_cfg, R"({
    "schemaVersion": 1,
    "engineOverride": "rmmz",
    "performance": { "cpuProfile": "inherit" }
  })");
  LaunchManifest m2 = ConfigMerger::build_launch_manifest(game, global_cfg, mv_cfg, "sess-43");
  NXRPG_CHECK(m2.game.engine == EngineKind::Rmmz);
  NXRPG_CHECK(m2.game.core == CoreKind::NxjsMvz);
  NXRPG_CHECK(m2.runtime.rgss_version == 0);
  NXRPG_CHECK(m2.runtime.cpu_profile == CpuProfile::Boost1581);

  return nxrpg::test::finish();
}
