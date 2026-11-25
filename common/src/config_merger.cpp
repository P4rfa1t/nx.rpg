#include "nxrpg/config_merger.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>

#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg {

namespace {

std::optional<JsonValue> load_json_file(const std::filesystem::path& p) {
  if (p.empty()) return std::nullopt;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(p, ec)) return std::nullopt;
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) return std::nullopt;
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  auto parsed = JsonValue::parse(content);
  if (!parsed || !parsed->is_object()) return std::nullopt;
  return parsed;
}

}  // namespace

LaunchManifest ConfigMerger::build_launch_manifest(
    const DetectedGame& game,
    const std::filesystem::path& global_config_path,
    const std::filesystem::path& per_game_config_path,
    std::string_view session_id) {
  // -------------------------------------------------------------------------
  // Layer 1: Built-in engine defaults + DetectedGame metadata
  // -------------------------------------------------------------------------
  LaunchManifest m;
  m.schema_version = 1;
  m.session_id = session_id.empty() ? "session-0" : std::string(session_id);
  m.return_to = std::string(paths::kLauncherNro);
  m.log_path = std::string(paths::kLastRunLog);

  m.game.id = game.id.empty() ? "unknown-game" : game.id;
  m.game.title = game.title.empty() ? m.game.id : game.title;
  m.game.engine = game.engine;
  m.game.core = (game.core != CoreKind::Unknown) ? game.core : core_for_engine(game.engine);
  m.game.game_path = game.game_path;
  m.game.content_root = game.content_root.empty() ? game.game_path : game.content_root;
  m.game.save_path = std::string(paths::kSavesDir) + "/" + m.game.id;
  m.game.exec_name = game.exec_name.empty() ? "Game" : game.exec_name;

  m.assets.rtp_path = paths::default_rtp_path(m.game.engine);
  m.assets.fallback_font_path = std::string(paths::kFontsDir) + "/NotoSansCJK-Regular.ttc";

  if (m.game.engine == EngineKind::Rmmv || m.game.engine == EngineKind::Rmmz) {
    m.runtime.encoding = TextEncoding::Utf8;
  } else if (game.detected_encoding != TextEncoding::Auto) {
    m.runtime.encoding = game.detected_encoding;
  } else {
    m.runtime.encoding = TextEncoding::Auto;
  }
  m.runtime.rgss_version = rgss_version_for_engine(m.game.engine);
  m.runtime.cpu_profile = CpuProfile::SystemDefault;
  m.runtime.enable_touch = true;
  m.runtime.enable_software_keyboard = true;

  m.video.scaling_mode = ScalingMode::Fit;
  m.video.integer_scale = false;
  m.video.vsync = true;
  m.video.show_fps = false;

  m.input.profile = "default";
  m.input.swap_confirm_cancel = false;
  m.input.exit_combo = {"L", "R", "PLUS"};

  // -------------------------------------------------------------------------
  // Layer 2: Global config (`config/global.json`)
  // -------------------------------------------------------------------------
  if (auto global_opt = load_json_file(global_config_path)) {
    const JsonValue& g = *global_opt;

    if (const auto* enc_v = g.find("defaultEncoding"); enc_v && enc_v->is_string()) {
      if (auto enc = parse_text_encoding(*enc_v->as_string()); enc && *enc != TextEncoding::Auto) {
        m.runtime.encoding = *enc;
      }
    }
    if (const auto* font_v = g.find("fallbackFontPath"); font_v && font_v->is_string()) {
      m.assets.fallback_font_path = *font_v->as_string();
    }
    if (const auto* perf = g.find("performance"); perf && perf->is_object()) {
      if (const auto* cpu_v = perf->find("defaultCpuProfile"); cpu_v && cpu_v->is_string()) {
        if (auto cpu = parse_cpu_profile(*cpu_v->as_string())) {
          m.runtime.cpu_profile = *cpu;
        }
      }
    }
    if (const auto* vid = g.find("video"); vid && vid->is_object()) {
      if (const auto* sm_v = vid->find("scalingMode"); sm_v && sm_v->is_string()) {
        if (auto sm = parse_scaling_mode(*sm_v->as_string())) {
          m.video.scaling_mode = *sm;
        }
      }
      if (const auto* b = vid->find("integerScale"); b && b->as_bool()) {
        m.video.integer_scale = *b->as_bool();
      }
      if (const auto* b = vid->find("vsync"); b && b->as_bool()) {
        m.video.vsync = *b->as_bool();
      }
      if (const auto* b = vid->find("showFps"); b && b->as_bool()) {
        m.video.show_fps = *b->as_bool();
      }
    }
    if (const auto* inp = g.find("input"); inp && inp->is_object()) {
      if (const auto* prof = inp->find("profile"); prof && prof->is_string()) {
        m.input.profile = *prof->as_string();
      }
      if (const auto* b = inp->find("swapConfirmCancel"); b && b->as_bool()) {
        m.input.swap_confirm_cancel = *b->as_bool();
      }
      if (const auto* b = inp->find("enableTouch"); b && b->as_bool()) {
        m.runtime.enable_touch = *b->as_bool();
      }
      if (const auto* combo = inp->find("exitCombo"); combo && combo->as_array()) {
        std::vector<std::string> btns;
        for (const auto& item : *combo->as_array()) {
          if (const auto* s = item.as_string()) {
            btns.push_back(*s);
          }
        }
        if (btns.size() >= 2) {
          m.input.exit_combo = std::move(btns);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Layer 3: Per-game config (`config/games/<id>.json`)
  // -------------------------------------------------------------------------
  if (auto game_opt = load_json_file(per_game_config_path)) {
    const JsonValue& pg = *game_opt;

    if (const auto* title_v = pg.find("displayTitle"); title_v && title_v->is_string()) {
      if (!title_v->as_string()->empty()) {
        m.game.title = *title_v->as_string();
      }
    }
    if (const auto* eng_v = pg.find("engineOverride"); eng_v && eng_v->is_string()) {
      const std::string& es = *eng_v->as_string();
      if (es != "auto") {
        if (auto eng = parse_engine_kind(es); eng && *eng != EngineKind::Unknown) {
          m.game.engine = *eng;
          m.game.core = core_for_engine(*eng);
          m.runtime.rgss_version = rgss_version_for_engine(*eng);
          m.assets.rtp_path = paths::default_rtp_path(*eng);
        }
      }
    }
    if (const auto* exec_v = pg.find("execNameOverride"); exec_v && exec_v->is_string()) {
      if (!exec_v->as_string()->empty()) {
        m.game.exec_name = *exec_v->as_string();
      }
    }
    if (const auto* enc_v = pg.find("encoding"); enc_v && enc_v->is_string()) {
      if (auto enc = parse_text_encoding(*enc_v->as_string()); enc && *enc != TextEncoding::Auto) {
        m.runtime.encoding = *enc;
      }
    }
    if (const auto* rtp_v = pg.find("rtpOverridePath"); rtp_v && rtp_v->is_string()) {
      m.assets.rtp_path = *rtp_v->as_string();
    }
    if (const auto* font_v = pg.find("fallbackFontPath"); font_v && font_v->is_string()) {
      m.assets.fallback_font_path = *font_v->as_string();
    }
    if (const auto* perf = pg.find("performance"); perf && perf->is_object()) {
      if (const auto* cpu_v = perf->find("cpuProfile"); cpu_v && cpu_v->is_string()) {
        if (*cpu_v->as_string() != "inherit") {
          if (auto cpu = parse_cpu_profile(*cpu_v->as_string())) {
            m.runtime.cpu_profile = *cpu;
          }
        }
      }
    }
    if (const auto* vid = pg.find("video"); vid && vid->is_object()) {
      if (const auto* sm_v = vid->find("scalingMode"); sm_v && sm_v->is_string()) {
        if (auto sm = parse_scaling_mode(*sm_v->as_string())) {
          m.video.scaling_mode = *sm;
        }
      }
      if (const auto* b = vid->find("integerScale"); b && b->as_bool()) {
        m.video.integer_scale = *b->as_bool();
      }
      if (const auto* b = vid->find("vsync"); b && b->as_bool()) {
        m.video.vsync = *b->as_bool();
      }
      if (const auto* b = vid->find("showFps"); b && b->as_bool()) {
        m.video.show_fps = *b->as_bool();
      }
    }
    if (const auto* inp = pg.find("input"); inp && inp->is_object()) {
      if (const auto* prof = inp->find("profile"); prof && prof->is_string()) {
        m.input.profile = *prof->as_string();
      }
      if (const auto* b = inp->find("swapConfirmCancel"); b && b->as_bool()) {
        m.input.swap_confirm_cancel = *b->as_bool();
      }
      if (const auto* b = inp->find("enableTouch"); b && b->as_bool()) {
        m.runtime.enable_touch = *b->as_bool();
      }
    }
    if (const auto* er = pg.find("easyrpgOptions"); er && er->is_object()) {
      if (const auto* s = er->find("soundfontPath"); s && s->is_string()) {
        m.core_options.easyrpg.soundfont_path = *s->as_string();
      }
      if (const auto* b = er->find("disableRtp"); b && b->as_bool()) {
        m.core_options.easyrpg.disable_rtp = *b->as_bool();
      }
    }
    if (const auto* mvz = pg.find("mvzOptions"); mvz && mvz->is_object()) {
      if (const auto* s = mvz->find("renderer"); s && s->is_string()) {
        m.core_options.mvz.renderer = *s->as_string();
      }
      if (const auto* b = mvz->find("pretendNwjs"); b && b->as_bool()) {
        m.core_options.mvz.pretend_nwjs = *b->as_bool();
      }
      if (const auto* i = mvz->find("maxTextureSize"); i && i->as_int64()) {
        m.core_options.mvz.max_texture_size = static_cast<std::uint32_t>(*i->as_int64());
      }
    }
    if (const auto* mkxp = pg.find("mkxpOptions"); mkxp && mkxp->is_object()) {
      if (const auto* b = mkxp->find("enableWin32ApiStubs"); b && b->as_bool()) {
        m.core_options.mkxp.enable_win32_api_stubs = *b->as_bool();
      }
      if (const auto* b = mkxp->find("solidFonts"); b && b->as_bool()) {
        m.core_options.mkxp.solid_fonts = *b->as_bool();
      }
      if (const auto* i = mkxp->find("smoothScaling"); i && i->as_int64()) {
        m.core_options.mkxp.smooth_scaling = static_cast<std::uint8_t>(*i->as_int64());
      }
      if (const auto* b = mkxp->find("frameSkip"); b && b->as_bool()) {
        m.core_options.mkxp.frame_skip = *b->as_bool();
      }
      if (const auto* b = mkxp->find("subImageFix"); b && b->as_bool()) {
        m.core_options.mkxp.sub_image_fix = *b->as_bool();
      }
    }
  }

  return m;
}

}  // namespace nxrpg
