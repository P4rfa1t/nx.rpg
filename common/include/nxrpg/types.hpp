#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace nxrpg {

/// Supported RPG Maker generations across all three cores.
enum class EngineKind : std::uint8_t {
  Unknown = 0,
  Rm2k,   // RPG Maker 2000   -> EasyRPG Player
  Rm2k3,  // RPG Maker 2003   -> EasyRPG Player
  Rmxp,   // RPG Maker XP     -> mkxp-z (RGSS1)
  Rmvx,   // RPG Maker VX     -> mkxp-z (RGSS2)
  Rmvxa,  // RPG Maker VX Ace -> mkxp-z (RGSS3)
  Rmmv,   // RPG Maker MV     -> nxjs-mvz (PixiJS v4)
  Rmmz,   // RPG Maker MZ     -> nxjs-mvz (PixiJS v5 + Effekseer WASM)
};

/// Target core NRO executable responsible for running an EngineKind.
enum class CoreKind : std::uint8_t {
  Unknown = 0,
  EasyRpg,  // sdmc:/switch/nx.rpg/cores/easyrpg.nro
  NxjsMvz,  // sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro
  MkxpZ,    // sdmc:/switch/nx.rpg/cores/mkxp-z.nro
};

/// Legacy text encoding override for INI/LDB/script metadata.
enum class TextEncoding : std::uint8_t {
  Auto = 0,
  Utf8,
  ShiftJis,  // CP932 (Japanese)
  Gbk,       // CP936 (Simplified Chinese)
  Big5,      // CP950 (Traditional Chinese)
  Cp949,     // Korean
  Cp1252,    // Western European
};

/// Switch PCV / clkrst CPU clock preset.
/// `SystemDefault` never touches clkrst: the Horizon powerd profile (or a user
/// overclock sysmodule / KIP patch) stays in control of CPU clocks.
enum class CpuProfile : std::uint8_t {
  SystemDefault = 0,  // Do not change clocks (follow system powerd / OC kip)
  Stock1020,          // Pin to 1020 MHz (Horizon OS default)
  Boost1581,          // 1581 MHz (balanced boost for heavy maps/scripts)
  Boost1785,          // 1785 MHz (maximum safe Switch CPU clock)
};

/// Viewport aspect scaling policy.
enum class ScalingMode : std::uint8_t {
  Fit = 0,  // Pillarboxed / letterboxed preserving aspect ratio
  Stretch,  // Stretch to 1280x720 / 1920x1080
  Native,   // 1:1 unscaled centered pixels
};

/// Switch memory execution context detected via appletGetAppletType().
enum class MemoryExecutionMode : std::uint8_t {
  ApplicationFullRam = 0,  // ~3.2+ GiB available (Title Takeover / NSP forwarder)
  AppletRestrictedRam,     // ~440 MiB available (Album hbmenu launch; JIT/WASM restricted)
  HostDesktop,             // Non-Switch desktop test environment
};

[[nodiscard]] constexpr CoreKind core_for_engine(EngineKind engine) noexcept {
  switch (engine) {
    case EngineKind::Rm2k:
    case EngineKind::Rm2k3:
      return CoreKind::EasyRpg;
    case EngineKind::Rmxp:
    case EngineKind::Rmvx:
    case EngineKind::Rmvxa:
      return CoreKind::MkxpZ;
    case EngineKind::Rmmv:
    case EngineKind::Rmmz:
      return CoreKind::NxjsMvz;
    case EngineKind::Unknown:
    default:
      return CoreKind::Unknown;
  }
}

[[nodiscard]] constexpr std::uint8_t rgss_version_for_engine(EngineKind engine) noexcept {
  switch (engine) {
    case EngineKind::Rmxp:
      return 1;
    case EngineKind::Rmvx:
      return 2;
    case EngineKind::Rmvxa:
      return 3;
    default:
      return 0;
  }
}

[[nodiscard]] std::string_view to_string(EngineKind engine) noexcept;
[[nodiscard]] std::string_view to_string(CoreKind core) noexcept;
[[nodiscard]] std::string_view to_string(TextEncoding encoding) noexcept;
[[nodiscard]] std::string_view to_string(CpuProfile profile) noexcept;
[[nodiscard]] std::string_view to_string(ScalingMode mode) noexcept;

[[nodiscard]] std::optional<EngineKind> parse_engine_kind(std::string_view text) noexcept;
[[nodiscard]] std::optional<CoreKind> parse_core_kind(std::string_view text) noexcept;
[[nodiscard]] std::optional<TextEncoding> parse_text_encoding(std::string_view text) noexcept;
[[nodiscard]] std::optional<CpuProfile> parse_cpu_profile(std::string_view text) noexcept;
[[nodiscard]] std::optional<ScalingMode> parse_scaling_mode(std::string_view text) noexcept;

}  // namespace nxrpg
