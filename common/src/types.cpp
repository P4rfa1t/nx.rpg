#include "nxrpg/types.hpp"

namespace nxrpg {

std::string_view to_string(EngineKind engine) noexcept {
  switch (engine) {
    case EngineKind::Rm2k:
      return "rm2k";
    case EngineKind::Rm2k3:
      return "rm2k3";
    case EngineKind::Rmxp:
      return "rmxp";
    case EngineKind::Rmvx:
      return "rmvx";
    case EngineKind::Rmvxa:
      return "rmvxa";
    case EngineKind::Rmmv:
      return "rmmv";
    case EngineKind::Rmmz:
      return "rmmz";
    case EngineKind::Unknown:
    default:
      return "unknown";
  }
}

std::string_view to_string(CoreKind core) noexcept {
  switch (core) {
    case CoreKind::EasyRpg:
      return "easyrpg";
    case CoreKind::NxjsMvz:
      return "nxjs-mvz";
    case CoreKind::MkxpZ:
      return "mkxp-z";
    case CoreKind::Unknown:
    default:
      return "unknown";
  }
}

std::string_view to_string(TextEncoding encoding) noexcept {
  switch (encoding) {
    case TextEncoding::Auto:
      return "auto";
    case TextEncoding::Utf8:
      return "utf-8";
    case TextEncoding::ShiftJis:
      return "shift_jis";
    case TextEncoding::Gbk:
      return "gbk";
    case TextEncoding::Big5:
      return "big5";
    case TextEncoding::Cp949:
      return "cp949";
    case TextEncoding::Cp1252:
      return "cp1252";
    default:
      return "auto";
  }
}

std::string_view to_string(CpuProfile profile) noexcept {
  switch (profile) {
    case CpuProfile::SystemDefault:
      return "system_default";
    case CpuProfile::Stock1020:
      return "stock_1020";
    case CpuProfile::Boost1581:
      return "boost_1581";
    case CpuProfile::Boost1785:
      return "boost_1785";
    default:
      return "system_default";
  }
}

std::string_view to_string(ScalingMode mode) noexcept {
  switch (mode) {
    case ScalingMode::Fit:
      return "fit";
    case ScalingMode::Stretch:
      return "stretch";
    case ScalingMode::Native:
      return "native";
    default:
      return "fit";
  }
}

std::optional<EngineKind> parse_engine_kind(std::string_view text) noexcept {
  if (text == "rm2k") return EngineKind::Rm2k;
  if (text == "rm2k3") return EngineKind::Rm2k3;
  if (text == "rmxp") return EngineKind::Rmxp;
  if (text == "rmvx") return EngineKind::Rmvx;
  if (text == "rmvxa") return EngineKind::Rmvxa;
  if (text == "rmmv") return EngineKind::Rmmv;
  if (text == "rmmz") return EngineKind::Rmmz;
  if (text == "unknown") return EngineKind::Unknown;
  return std::nullopt;
}

std::optional<CoreKind> parse_core_kind(std::string_view text) noexcept {
  if (text == "easyrpg") return CoreKind::EasyRpg;
  if (text == "nxjs-mvz") return CoreKind::NxjsMvz;
  if (text == "mkxp-z") return CoreKind::MkxpZ;
  if (text == "unknown") return CoreKind::Unknown;
  return std::nullopt;
}

std::optional<TextEncoding> parse_text_encoding(std::string_view text) noexcept {
  if (text == "auto") return TextEncoding::Auto;
  if (text == "utf-8" || text == "utf8") return TextEncoding::Utf8;
  if (text == "shift_jis" || text == "cp932" || text == "sjis") return TextEncoding::ShiftJis;
  if (text == "gbk" || text == "cp936") return TextEncoding::Gbk;
  if (text == "big5" || text == "cp950") return TextEncoding::Big5;
  if (text == "cp949" || text == "euc-kr") return TextEncoding::Cp949;
  if (text == "cp1252" || text == "latin1") return TextEncoding::Cp1252;
  return std::nullopt;
}

std::optional<CpuProfile> parse_cpu_profile(std::string_view text) noexcept {
  if (text == "system_default") return CpuProfile::SystemDefault;
  if (text == "stock_1020") return CpuProfile::Stock1020;
  if (text == "boost_1581") return CpuProfile::Boost1581;
  if (text == "boost_1785") return CpuProfile::Boost1785;
  return std::nullopt;
}

std::optional<ScalingMode> parse_scaling_mode(std::string_view text) noexcept {
  if (text == "fit") return ScalingMode::Fit;
  if (text == "stretch") return ScalingMode::Stretch;
  if (text == "native") return ScalingMode::Native;
  return std::nullopt;
}

}  // namespace nxrpg
