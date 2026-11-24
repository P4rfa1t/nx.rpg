#include "nxrpg/launch_manifest.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg {

namespace {

bool is_valid_exit_button(std::string_view btn) noexcept {
  return btn == "A" || btn == "B" || btn == "X" || btn == "Y" ||
         btn == "L" || btn == "R" || btn == "ZL" || btn == "ZR" ||
         btn == "PLUS" || btn == "MINUS" || btn == "LSTICK" || btn == "RSTICK";
}

bool is_valid_game_id(std::string_view id) noexcept {
  if (id.empty()) return false;
  for (unsigned char c : id) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

const std::string* get_str(const JsonValue& obj, std::string_view key) noexcept {
  const JsonValue* v = obj.find(key);
  return v ? v->as_string() : nullptr;
}

std::optional<bool> get_bool(const JsonValue& obj, std::string_view key) noexcept {
  const JsonValue* v = obj.find(key);
  return v ? v->as_bool() : std::nullopt;
}

std::optional<std::int64_t> get_int(const JsonValue& obj, std::string_view key) noexcept {
  const JsonValue* v = obj.find(key);
  return v ? v->as_int64() : std::nullopt;
}

}  // namespace

std::string LaunchManifest::to_json_string(bool pretty) const {
  JsonValue root = JsonValue::make_object();
  root.set("schemaVersion", static_cast<std::int64_t>(schema_version));
  root.set("sessionId", session_id);
  root.set("returnTo", return_to);
  root.set("logPath", log_path);

  JsonValue game_obj = JsonValue::make_object();
  game_obj.set("id", game.id);
  game_obj.set("title", game.title);
  game_obj.set("engine", to_string(game.engine));
  game_obj.set("core", to_string(game.core));
  game_obj.set("gamePath", game.game_path);
  game_obj.set("contentRoot", game.content_root);
  game_obj.set("savePath", game.save_path);
  game_obj.set("execName", game.exec_name);
  root.set("game", std::move(game_obj));

  JsonValue assets_obj = JsonValue::make_object();
  assets_obj.set("rtpPath", assets.rtp_path);
  assets_obj.set("fallbackFontPath", assets.fallback_font_path);
  root.set("assets", std::move(assets_obj));

  JsonValue runtime_obj = JsonValue::make_object();
  runtime_obj.set("encoding", to_string(runtime.encoding));
  runtime_obj.set("rgssVersion", static_cast<std::int64_t>(runtime.rgss_version));
  runtime_obj.set("cpuProfile", to_string(runtime.cpu_profile));
  runtime_obj.set("enableTouch", runtime.enable_touch);
  runtime_obj.set("enableSoftwareKeyboard", runtime.enable_software_keyboard);
  root.set("runtime", std::move(runtime_obj));

  JsonValue video_obj = JsonValue::make_object();
  video_obj.set("scalingMode", to_string(video.scaling_mode));
  video_obj.set("integerScale", video.integer_scale);
  video_obj.set("vsync", video.vsync);
  video_obj.set("showFps", video.show_fps);
  root.set("video", std::move(video_obj));

  JsonValue input_obj = JsonValue::make_object();
  input_obj.set("profile", input.profile);
  input_obj.set("swapConfirmCancel", input.swap_confirm_cancel);
  JsonValue combo_arr = JsonValue::make_array();
  for (const auto& btn : input.exit_combo) {
    combo_arr.push_back(btn);
  }
  input_obj.set("exitCombo", std::move(combo_arr));
  root.set("input", std::move(input_obj));

  if (game.core == CoreKind::EasyRpg || game.core == CoreKind::NxjsMvz || game.core == CoreKind::MkxpZ) {
    JsonValue core_opts = JsonValue::make_object();
    if (game.core == CoreKind::EasyRpg) {
      JsonValue er = JsonValue::make_object();
      er.set("soundfontPath", core_options.easyrpg.soundfont_path);
      er.set("disableRtp", core_options.easyrpg.disable_rtp);
      core_opts.set("easyrpg", std::move(er));
    } else if (game.core == CoreKind::NxjsMvz) {
      JsonValue mvz = JsonValue::make_object();
      mvz.set("renderer", core_options.mvz.renderer);
      mvz.set("pretendNwjs", core_options.mvz.pretend_nwjs);
      mvz.set("maxTextureSize", static_cast<std::int64_t>(core_options.mvz.max_texture_size));
      core_opts.set("mvz", std::move(mvz));
    } else if (game.core == CoreKind::MkxpZ) {
      JsonValue mkxp = JsonValue::make_object();
      mkxp.set("enableWin32ApiStubs", core_options.mkxp.enable_win32_api_stubs);
      mkxp.set("solidFonts", core_options.mkxp.solid_fonts);
      mkxp.set("smoothScaling", static_cast<std::int64_t>(core_options.mkxp.smooth_scaling));
      mkxp.set("frameSkip", core_options.mkxp.frame_skip);
      mkxp.set("subImageFix", core_options.mkxp.sub_image_fix);
      core_opts.set("mkxp", std::move(mkxp));
    }
    root.set("coreOptions", std::move(core_opts));
  }

  return root.dump(pretty);
}

std::optional<LaunchManifest> LaunchManifest::from_json_string(std::string_view json) {
  auto root_opt = JsonValue::parse(json);
  if (!root_opt || !root_opt->is_object()) return std::nullopt;
  const JsonValue& root = *root_opt;

  auto ver = get_int(root, "schemaVersion");
  if (!ver || *ver != 1) return std::nullopt;

  const auto* sid = get_str(root, "sessionId");
  const auto* ret = get_str(root, "returnTo");
  const auto* logp = get_str(root, "logPath");
  if (!sid || sid->empty() || !ret || !logp) return std::nullopt;

  LaunchManifest m;
  m.schema_version = 1;
  m.session_id = *sid;
  m.return_to = *ret;
  m.log_path = *logp;

  const JsonValue* game_v = root.find("game");
  if (!game_v || !game_v->is_object()) return std::nullopt;
  {
    const auto* id = get_str(*game_v, "id");
    const auto* title = get_str(*game_v, "title");
    const auto* eng_s = get_str(*game_v, "engine");
    const auto* core_s = get_str(*game_v, "core");
    const auto* gp = get_str(*game_v, "gamePath");
    const auto* cr = get_str(*game_v, "contentRoot");
    const auto* sp = get_str(*game_v, "savePath");
    const auto* ex = get_str(*game_v, "execName");
    if (!id || !is_valid_game_id(*id) || !title || !eng_s || !core_s || !gp || !cr || !sp || !ex) {
      return std::nullopt;
    }
    auto eng = parse_engine_kind(*eng_s);
    auto core = parse_core_kind(*core_s);
    if (!eng || *eng == EngineKind::Unknown || !core || *core == CoreKind::Unknown) {
      return std::nullopt;
    }
    m.game.id = *id;
    m.game.title = *title;
    m.game.engine = *eng;
    m.game.core = *core;
    m.game.game_path = *gp;
    m.game.content_root = *cr;
    m.game.save_path = *sp;
    m.game.exec_name = *ex;
  }

  const JsonValue* assets_v = root.find("assets");
  if (!assets_v || !assets_v->is_object()) return std::nullopt;
  {
    const auto* rtp = get_str(*assets_v, "rtpPath");
    const auto* font = get_str(*assets_v, "fallbackFontPath");
    if (!rtp || !font) return std::nullopt;
    m.assets.rtp_path = *rtp;
    m.assets.fallback_font_path = *font;
  }

  const JsonValue* rt_v = root.find("runtime");
  if (!rt_v || !rt_v->is_object()) return std::nullopt;
  {
    const auto* enc_s = get_str(*rt_v, "encoding");
    auto rgss = get_int(*rt_v, "rgssVersion");
    const auto* cpu_s = get_str(*rt_v, "cpuProfile");
    auto touch = get_bool(*rt_v, "enableTouch");
    auto swkbd = get_bool(*rt_v, "enableSoftwareKeyboard");
    if (!enc_s || !rgss || *rgss < 0 || *rgss > 3 || !cpu_s || !touch || !swkbd) {
      return std::nullopt;
    }
    auto enc = parse_text_encoding(*enc_s);
    auto cpu = parse_cpu_profile(*cpu_s);
    if (!enc || !cpu) return std::nullopt;
    m.runtime.encoding = *enc;
    m.runtime.rgss_version = static_cast<std::uint8_t>(*rgss);
    m.runtime.cpu_profile = *cpu;
    m.runtime.enable_touch = *touch;
    m.runtime.enable_software_keyboard = *swkbd;
  }

  const JsonValue* vid_v = root.find("video");
  if (!vid_v || !vid_v->is_object()) return std::nullopt;
  {
    const auto* sm_s = get_str(*vid_v, "scalingMode");
    auto isc = get_bool(*vid_v, "integerScale");
    auto vs = get_bool(*vid_v, "vsync");
    auto fps = get_bool(*vid_v, "showFps");
    if (!sm_s || !isc || !vs || !fps) return std::nullopt;
    auto sm = parse_scaling_mode(*sm_s);
    if (!sm) return std::nullopt;
    m.video.scaling_mode = *sm;
    m.video.integer_scale = *isc;
    m.video.vsync = *vs;
    m.video.show_fps = *fps;
  }

  const JsonValue* inp_v = root.find("input");
  if (!inp_v || !inp_v->is_object()) return std::nullopt;
  {
    const auto* prof = get_str(*inp_v, "profile");
    auto swap = get_bool(*inp_v, "swapConfirmCancel");
    const JsonValue* combo_v = inp_v->find("exitCombo");
    const auto* combo_arr = combo_v ? combo_v->as_array() : nullptr;
    if (!prof || !swap || !combo_arr || combo_arr->size() < 2) return std::nullopt;
    m.input.profile = *prof;
    m.input.swap_confirm_cancel = *swap;
    m.input.exit_combo.clear();
    for (const auto& elem : *combo_arr) {
      const auto* btn = elem.as_string();
      if (!btn || !is_valid_exit_button(*btn)) return std::nullopt;
      m.input.exit_combo.push_back(*btn);
    }
  }

  if (const JsonValue* copts = root.find("coreOptions")) {
    if (!copts->is_object()) return std::nullopt;
    if (const JsonValue* er = copts->find("easyrpg")) {
      if (!er->is_object()) return std::nullopt;
      if (const auto* sf = get_str(*er, "soundfontPath")) m.core_options.easyrpg.soundfont_path = *sf;
      if (auto dr = get_bool(*er, "disableRtp")) m.core_options.easyrpg.disable_rtp = *dr;
    }
    if (const JsonValue* mvz = copts->find("mvz")) {
      if (!mvz->is_object()) return std::nullopt;
      if (const auto* r = get_str(*mvz, "renderer")) {
        if (*r != "auto" && *r != "webgl2" && *r != "canvas2d") return std::nullopt;
        m.core_options.mvz.renderer = *r;
      }
      if (auto pnw = get_bool(*mvz, "pretendNwjs")) m.core_options.mvz.pretend_nwjs = *pnw;
      if (auto mts = get_int(*mvz, "maxTextureSize")) {
        if (*mts != 0 && *mts != 2048 && *mts != 4096) return std::nullopt;
        m.core_options.mvz.max_texture_size = static_cast<std::uint32_t>(*mts);
      }
    }
    if (const JsonValue* mkxp = copts->find("mkxp")) {
      if (!mkxp->is_object()) return std::nullopt;
      if (auto w32 = get_bool(*mkxp, "enableWin32ApiStubs")) m.core_options.mkxp.enable_win32_api_stubs = *w32;
      if (auto sf = get_bool(*mkxp, "solidFonts")) m.core_options.mkxp.solid_fonts = *sf;
      if (auto ss = get_int(*mkxp, "smoothScaling")) {
        if (*ss < 0 || *ss > 4) return std::nullopt;
        m.core_options.mkxp.smooth_scaling = static_cast<std::uint8_t>(*ss);
      }
      if (auto fs = get_bool(*mkxp, "frameSkip")) m.core_options.mkxp.frame_skip = *fs;
      if (auto sif = get_bool(*mkxp, "subImageFix")) m.core_options.mkxp.sub_image_fix = *sif;
    }
  }

  return m;
}

std::optional<LaunchManifest> LaunchManifest::load_from_file(const std::filesystem::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return std::nullopt;
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  return from_json_string(content);
}

bool LaunchManifest::save_to_file_atomic(const std::filesystem::path& path) const {
  return paths::write_file_atomic(path, to_json_string(true));
}

std::optional<std::string> parse_manifest_arg(int argc, const char* const* argv) {
  if (!argv || argc <= 1) return std::nullopt;
  constexpr std::string_view kFlag = "--nxrpg-manifest";
  constexpr std::string_view kPrefix = "--nxrpg-manifest=";
  for (int i = 1; i < argc; ++i) {
    if (!argv[i]) continue;
    std::string_view arg(argv[i]);
    if (arg == kFlag) {
      if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '\0') {
        return std::string(argv[i + 1]);
      }
      return std::nullopt;
    }
    if (arg.rfind(kPrefix, 0) == 0 && arg.size() > kPrefix.size()) {
      return std::string(arg.substr(kPrefix.size()));
    }
  }
  return std::nullopt;
}

}  // namespace nxrpg
