#include "launcher/services/config_service.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg::launcher {

namespace {

namespace fs = std::filesystem;

std::optional<JsonValue> read_json_object(const fs::path& p) {
  std::error_code ec;
  if (!fs::is_regular_file(p, ec)) return std::nullopt;
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) return std::nullopt;
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  auto parsed = JsonValue::parse(content);
  if (!parsed || !parsed->is_object()) return std::nullopt;
  return parsed;
}

}  // namespace

GlobalConfig ConfigService::load_global(const fs::path& sd_root) {
  GlobalConfig cfg;
  auto root_opt = read_json_object(sd_root / "config/global.json");
  if (!root_opt) return cfg;
  const JsonValue& g = *root_opt;

  if (const auto* v = g.find("uiLanguage"); v && v->as_string()) {
    const std::string& lang = *v->as_string();
    if (lang == "zh-CN" || lang == "zh-TW" || lang == "en-US" || lang == "ja-JP" || lang == "ko-KR") {
      cfg.ui_language = lang;
    }
  }
  if (const auto* v = g.find("defaultEncoding"); v && v->as_string()) {
    if (auto enc = parse_text_encoding(*v->as_string())) {
      cfg.default_encoding = *enc;
    }
  }
  if (const auto* v = g.find("fallbackFontPath"); v && v->as_string()) {
    cfg.fallback_font_path = *v->as_string();
  }
  if (const auto* v = g.find("warnOnAppletMode"); v && v->as_bool()) {
    cfg.warn_on_applet_mode = *v->as_bool();
  }

  if (const auto* perf = g.find("performance"); perf && perf->is_object()) {
    if (const auto* cpu = perf->find("defaultCpuProfile"); cpu && cpu->as_string()) {
      if (auto p = parse_cpu_profile(*cpu->as_string())) {
        cfg.performance.default_cpu_profile = *p;
      }
    }
  }

  if (const auto* vid = g.find("video"); vid && vid->is_object()) {
    if (const auto* sm = vid->find("scalingMode"); sm && sm->as_string()) {
      if (auto m = parse_scaling_mode(*sm->as_string())) {
        cfg.video.scaling_mode = *m;
      }
    }
    if (const auto* b = vid->find("integerScale"); b && b->as_bool()) {
      cfg.video.integer_scale = *b->as_bool();
    }
    if (const auto* b = vid->find("vsync"); b && b->as_bool()) {
      cfg.video.vsync = *b->as_bool();
    }
    if (const auto* b = vid->find("showFps"); b && b->as_bool()) {
      cfg.video.show_fps = *b->as_bool();
    }
  }

  if (const auto* inp = g.find("input"); inp && inp->is_object()) {
    if (const auto* p = inp->find("profile"); p && p->as_string()) {
      cfg.input.profile = *p->as_string();
    }
    if (const auto* b = inp->find("swapConfirmCancel"); b && b->as_bool()) {
      cfg.input.swap_confirm_cancel = *b->as_bool();
    }
    if (const auto* b = inp->find("enableTouch"); b && b->as_bool()) {
      cfg.input.enable_touch = *b->as_bool();
    }
    if (const auto* arr = inp->find("exitCombo"); arr && arr->as_array()) {
      std::vector<std::string> combo;
      for (const auto& item : *arr->as_array()) {
        if (const auto* s = item.as_string()) combo.push_back(*s);
      }
      if (combo.size() >= 2) {
        cfg.input.exit_combo = std::move(combo);
      }
    }
  }

  if (const auto* lib = g.find("library"); lib && lib->is_object()) {
    if (const auto* d = lib->find("maxScanDepth"); d && d->as_int64()) {
      auto depth = *d->as_int64();
      if (depth >= 1 && depth <= 4) {
        cfg.library.max_scan_depth = static_cast<std::uint32_t>(depth);
      }
    }
    if (const auto* so = lib->find("sortOrder"); so && so->as_string()) {
      const std::string& s = *so->as_string();
      if (s == "recent" || s == "title" || s == "engine") {
        cfg.library.sort_order = s;
      }
    }
    if (const auto* id = lib->find("lastSelectedGameId"); id && id->as_string()) {
      cfg.library.last_selected_game_id = *id->as_string();
    }
  }

  return cfg;
}

bool ConfigService::save_global(const fs::path& sd_root, const GlobalConfig& cfg) {
  JsonValue root = JsonValue::make_object();
  root.set("schemaVersion", static_cast<std::int64_t>(1));
  root.set("uiLanguage", cfg.ui_language);
  root.set("defaultEncoding", to_string(cfg.default_encoding));
  root.set("fallbackFontPath", cfg.fallback_font_path);
  root.set("warnOnAppletMode", cfg.warn_on_applet_mode);

  JsonValue perf = JsonValue::make_object();
  perf.set("defaultCpuProfile", to_string(cfg.performance.default_cpu_profile));
  root.set("performance", std::move(perf));

  JsonValue vid = JsonValue::make_object();
  vid.set("scalingMode", to_string(cfg.video.scaling_mode));
  vid.set("integerScale", cfg.video.integer_scale);
  vid.set("vsync", cfg.video.vsync);
  vid.set("showFps", cfg.video.show_fps);
  root.set("video", std::move(vid));

  JsonValue inp = JsonValue::make_object();
  inp.set("profile", cfg.input.profile);
  inp.set("swapConfirmCancel", cfg.input.swap_confirm_cancel);
  inp.set("enableTouch", cfg.input.enable_touch);
  JsonValue combo = JsonValue::make_array();
  if (cfg.input.exit_combo.size() >= 2) {
    for (const auto& b : cfg.input.exit_combo) combo.push_back(b);
  } else {
    combo.push_back("L");
    combo.push_back("R");
    combo.push_back("PLUS");
  }
  inp.set("exitCombo", std::move(combo));
  root.set("input", std::move(inp));

  JsonValue lib = JsonValue::make_object();
  lib.set("maxScanDepth", static_cast<std::int64_t>(cfg.library.max_scan_depth));
  lib.set("sortOrder", cfg.library.sort_order);
  lib.set("lastSelectedGameId", cfg.library.last_selected_game_id);
  root.set("library", std::move(lib));

  return paths::write_file_atomic(sd_root / "config/global.json", root.dump(true));
}

PerGameConfig ConfigService::load_per_game(const fs::path& sd_root, std::string_view game_id) {
  PerGameConfig cfg;
  if (game_id.empty()) return cfg;
  auto root_opt = read_json_object(sd_root / "config/games" / (std::string(game_id) + ".json"));
  if (!root_opt) return cfg;
  const JsonValue& pg = *root_opt;

  if (const auto* v = pg.find("displayTitle"); v && v->as_string()) cfg.display_title = *v->as_string();
  if (const auto* v = pg.find("engineOverride"); v && v->as_string()) cfg.engine_override = *v->as_string();
  if (const auto* v = pg.find("execNameOverride"); v && v->as_string()) cfg.exec_name_override = *v->as_string();
  if (const auto* v = pg.find("encoding"); v && v->as_string()) {
    if (auto enc = parse_text_encoding(*v->as_string())) cfg.encoding = *enc;
  }
  if (const auto* v = pg.find("rtpOverridePath"); v && v->as_string()) cfg.rtp_override_path = *v->as_string();
  if (const auto* v = pg.find("fallbackFontPath"); v && v->as_string()) cfg.fallback_font_path = *v->as_string();

  if (const auto* perf = pg.find("performance"); perf && perf->is_object()) {
    if (const auto* cpu = perf->find("cpuProfile"); cpu && cpu->as_string()) {
      cfg.cpu_profile = *cpu->as_string();
    }
  }

  if (const auto* vid = pg.find("video"); vid && vid->is_object()) {
    if (const auto* sm = vid->find("scalingMode"); sm && sm->as_string()) {
      cfg.scaling_mode = parse_scaling_mode(*sm->as_string());
    }
    if (const auto* b = vid->find("integerScale"); b && b->as_bool()) cfg.integer_scale = *b->as_bool();
    if (const auto* b = vid->find("vsync"); b && b->as_bool()) cfg.vsync = *b->as_bool();
    if (const auto* b = vid->find("showFps"); b && b->as_bool()) cfg.show_fps = *b->as_bool();
  }

  if (const auto* inp = pg.find("input"); inp && inp->is_object()) {
    if (const auto* p = inp->find("profile"); p && p->as_string()) cfg.input_profile = *p->as_string();
    if (const auto* b = inp->find("swapConfirmCancel"); b && b->as_bool()) cfg.swap_confirm_cancel = *b->as_bool();
    if (const auto* b = inp->find("enableTouch"); b && b->as_bool()) cfg.enable_touch = *b->as_bool();
  }

  if (const auto* er = pg.find("easyrpgOptions"); er && er->is_object()) {
    if (const auto* s = er->find("soundfontPath"); s && s->as_string()) cfg.easyrpg_soundfont_path = *s->as_string();
    if (const auto* b = er->find("disableRtp"); b && b->as_bool()) cfg.easyrpg_disable_rtp = *b->as_bool();
  }

  if (const auto* mvz = pg.find("mvzOptions"); mvz && mvz->is_object()) {
    if (const auto* s = mvz->find("renderer"); s && s->as_string()) cfg.mvz_renderer = *s->as_string();
    if (const auto* b = mvz->find("pretendNwjs"); b && b->as_bool()) cfg.mvz_pretend_nwjs = *b->as_bool();
    if (const auto* i = mvz->find("maxTextureSize"); i && i->as_int64()) {
      cfg.mvz_max_texture_size = static_cast<std::uint32_t>(*i->as_int64());
    }
  }

  if (const auto* mkxp = pg.find("mkxpOptions"); mkxp && mkxp->is_object()) {
    if (const auto* b = mkxp->find("enableWin32ApiStubs"); b && b->as_bool()) {
      cfg.mkxp_enable_win32_api_stubs = *b->as_bool();
    }
    if (const auto* b = mkxp->find("solidFonts"); b && b->as_bool()) cfg.mkxp_solid_fonts = *b->as_bool();
    if (const auto* i = mkxp->find("smoothScaling"); i && i->as_int64()) {
      cfg.mkxp_smooth_scaling = static_cast<std::uint8_t>(*i->as_int64());
    }
    if (const auto* b = mkxp->find("frameSkip"); b && b->as_bool()) cfg.mkxp_frame_skip = *b->as_bool();
    if (const auto* b = mkxp->find("subImageFix"); b && b->as_bool()) cfg.mkxp_sub_image_fix = *b->as_bool();
  }

  return cfg;
}

bool ConfigService::save_per_game(
    const fs::path& sd_root,
    std::string_view game_id,
    const PerGameConfig& cfg) {
  if (game_id.empty()) return false;
  JsonValue root = JsonValue::make_object();
  root.set("schemaVersion", static_cast<std::int64_t>(1));
  if (!cfg.display_title.empty()) root.set("displayTitle", cfg.display_title);
  root.set("engineOverride", cfg.engine_override.empty() ? "auto" : cfg.engine_override);
  if (!cfg.exec_name_override.empty()) root.set("execNameOverride", cfg.exec_name_override);
  root.set("encoding", to_string(cfg.encoding));
  if (!cfg.rtp_override_path.empty()) root.set("rtpOverridePath", cfg.rtp_override_path);
  if (!cfg.fallback_font_path.empty()) root.set("fallbackFontPath", cfg.fallback_font_path);

  JsonValue perf = JsonValue::make_object();
  perf.set("cpuProfile", cfg.cpu_profile.empty() ? "inherit" : cfg.cpu_profile);
  root.set("performance", std::move(perf));

  if (cfg.scaling_mode || cfg.integer_scale || cfg.vsync || cfg.show_fps) {
    JsonValue vid = JsonValue::make_object();
    if (cfg.scaling_mode) vid.set("scalingMode", to_string(*cfg.scaling_mode));
    if (cfg.integer_scale) vid.set("integerScale", *cfg.integer_scale);
    if (cfg.vsync) vid.set("vsync", *cfg.vsync);
    if (cfg.show_fps) vid.set("showFps", *cfg.show_fps);
    root.set("video", std::move(vid));
  }

  if (cfg.input_profile || cfg.swap_confirm_cancel || cfg.enable_touch) {
    JsonValue inp = JsonValue::make_object();
    if (cfg.input_profile) inp.set("profile", *cfg.input_profile);
    if (cfg.swap_confirm_cancel) inp.set("swapConfirmCancel", *cfg.swap_confirm_cancel);
    if (cfg.enable_touch) inp.set("enableTouch", *cfg.enable_touch);
    root.set("input", std::move(inp));
  }

  JsonValue er = JsonValue::make_object();
  if (!cfg.easyrpg_soundfont_path.empty()) er.set("soundfontPath", cfg.easyrpg_soundfont_path);
  er.set("disableRtp", cfg.easyrpg_disable_rtp);
  root.set("easyrpgOptions", std::move(er));

  JsonValue mvz = JsonValue::make_object();
  mvz.set("renderer", cfg.mvz_renderer);
  mvz.set("pretendNwjs", cfg.mvz_pretend_nwjs);
  mvz.set("maxTextureSize", static_cast<std::int64_t>(cfg.mvz_max_texture_size));
  root.set("mvzOptions", std::move(mvz));

  JsonValue mkxp = JsonValue::make_object();
  mkxp.set("enableWin32ApiStubs", cfg.mkxp_enable_win32_api_stubs);
  mkxp.set("solidFonts", cfg.mkxp_solid_fonts);
  mkxp.set("smoothScaling", static_cast<std::int64_t>(cfg.mkxp_smooth_scaling));
  mkxp.set("frameSkip", cfg.mkxp_frame_skip);
  mkxp.set("subImageFix", cfg.mkxp_sub_image_fix);
  root.set("mkxpOptions", std::move(mkxp));

  return paths::write_file_atomic(
      sd_root / "config/games" / (std::string(game_id) + ".json"),
      root.dump(true));
}

bool ConfigService::update_last_selected_game(const fs::path& sd_root, std::string_view game_id) {
  GlobalConfig cfg = load_global(sd_root);
  cfg.library.last_selected_game_id = std::string(game_id);
  return save_global(sd_root, cfg);
}

}  // namespace nxrpg::launcher
