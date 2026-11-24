#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "nxrpg/types.hpp"

namespace nxrpg {

/// C++20 in-memory representation of `common/schemas/launch-manifest.schema.json`.
struct LaunchManifest {
  std::uint32_t schema_version = 1;
  std::string session_id;
  std::string return_to;
  std::string log_path;

  struct GameSection {
    std::string id;
    std::string title;
    EngineKind engine = EngineKind::Unknown;
    CoreKind core = CoreKind::Unknown;
    std::string game_path;
    std::string content_root;
    std::string save_path;
    std::string exec_name = "Game";
  } game;

  struct AssetsSection {
    std::string rtp_path;
    std::string fallback_font_path;
  } assets;

  struct RuntimeSection {
    TextEncoding encoding = TextEncoding::Auto;
    std::uint8_t rgss_version = 0;
    CpuProfile cpu_profile = CpuProfile::Stock1020;
    bool enable_touch = true;
    bool enable_software_keyboard = true;
  } runtime;

  struct VideoSection {
    ScalingMode scaling_mode = ScalingMode::Fit;
    bool integer_scale = false;
    bool vsync = true;
    bool show_fps = false;
  } video;

  struct InputSection {
    std::string profile = "default";
    bool swap_confirm_cancel = false;
    std::vector<std::string> exit_combo = {"L", "R", "PLUS"};
  } input;

  struct CoreOptionsSection {
    struct EasyRpgOpts {
      std::string soundfont_path;
      bool disable_rtp = false;
    } easyrpg;

    struct MvzOpts {
      std::string renderer = "auto";
      bool pretend_nwjs = true;
      std::uint32_t max_texture_size = 4096;
    } mvz;

    struct MkxpOpts {
      bool enable_win32_api_stubs = true;
      bool solid_fonts = false;
      std::uint8_t smooth_scaling = 0;
      bool frame_skip = false;
      bool sub_image_fix = false;
    } mkxp;
  } core_options;

  [[nodiscard]] std::string to_json_string(bool pretty = true) const;
  [[nodiscard]] static std::optional<LaunchManifest> from_json_string(std::string_view json);
  [[nodiscard]] static std::optional<LaunchManifest> load_from_file(const std::filesystem::path& path);
  [[nodiscard]] bool save_to_file_atomic(const std::filesystem::path& path) const;
};

/// Extracts `--nxrpg-manifest <path>` from process `argc`/`argv` if present.
[[nodiscard]] std::optional<std::string> parse_manifest_arg(int argc, const char* const* argv);

}  // namespace nxrpg
