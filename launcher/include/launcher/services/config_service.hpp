#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "nxrpg/types.hpp"

namespace nxrpg::launcher {

/// In-memory representation of `common/schemas/global-config.schema.json`.
struct GlobalConfig {
  std::uint32_t schema_version = 1;
  std::string ui_language = "zh-CN";
  TextEncoding default_encoding = TextEncoding::Auto;
  std::string fallback_font_path = "sdmc:/switch/nx.rpg/fonts/NotoSansCJK-Regular.ttc";
  bool warn_on_applet_mode = true;

  struct {
    CpuProfile default_cpu_profile = CpuProfile::SystemDefault;
  } performance;

  struct {
    ScalingMode scaling_mode = ScalingMode::Fit;
    bool integer_scale = false;
    bool vsync = true;
    bool show_fps = false;
  } video;

  struct {
    std::string profile = "default";
    bool swap_confirm_cancel = false;
    bool enable_touch = true;
    std::vector<std::string> exit_combo = {"L", "R", "PLUS"};
  } input;

  struct {
    std::uint32_t max_scan_depth = 2;
    std::string sort_order = "recent";  // "recent" | "title" | "engine"
    std::string last_selected_game_id;
  } library;
};

/// In-memory representation of `common/schemas/game-config.schema.json`.
struct PerGameConfig {
  std::uint32_t schema_version = 1;
  std::string display_title;
  std::string engine_override = "auto";  // "auto" or EngineKind string
  std::string exec_name_override;
  TextEncoding encoding = TextEncoding::Auto;
  std::string rtp_override_path;
  std::string fallback_font_path;
  std::string cpu_profile = "inherit";  // "inherit" | "stock_1020" | "boost_1581" | "boost_1785"

  // Optional overrides (std::nullopt = inherit global)
  std::optional<ScalingMode> scaling_mode;
  std::optional<bool> integer_scale;
  std::optional<bool> vsync;
  std::optional<bool> show_fps;

  std::optional<std::string> input_profile;
  std::optional<bool> swap_confirm_cancel;
  std::optional<bool> enable_touch;

  // Core-specific options
  std::string easyrpg_soundfont_path;
  bool easyrpg_disable_rtp = false;

  std::string mvz_renderer = "auto";
  bool mvz_pretend_nwjs = true;
  std::uint32_t mvz_max_texture_size = 4096;

  bool mkxp_enable_win32_api_stubs = true;
  bool mkxp_solid_fonts = false;
  std::uint8_t mkxp_smooth_scaling = 0;
  bool mkxp_frame_skip = false;
  bool mkxp_sub_image_fix = false;
};

/// Reads and atomically persists `config/global.json` and `config/games/<id>.json`.
class ConfigService {
 public:
  [[nodiscard]] static GlobalConfig load_global(const std::filesystem::path& sd_root);
  [[nodiscard]] static bool save_global(const std::filesystem::path& sd_root, const GlobalConfig& cfg);

  [[nodiscard]] static PerGameConfig load_per_game(
      const std::filesystem::path& sd_root,
      std::string_view game_id);
  [[nodiscard]] static bool save_per_game(
      const std::filesystem::path& sd_root,
      std::string_view game_id,
      const PerGameConfig& cfg);

  [[nodiscard]] static bool update_last_selected_game(
      const std::filesystem::path& sd_root,
      std::string_view game_id);
};

}  // namespace nxrpg::launcher
