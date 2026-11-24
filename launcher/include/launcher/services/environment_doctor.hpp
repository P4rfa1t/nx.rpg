#pragma once

#include <string>
#include <vector>

#include "nxrpg/types.hpp"

namespace nxrpg::launcher {

struct DiagnosticItem {
  enum class Severity { Ok, Warning, Error } severity = Severity::Ok;
  std::string component;  // e.g. "Memory Mode", "Core: nxjs-mvz", "RTP: rm2003", "Font: CJK", "SD Filesystem"
  std::string summary;
  std::string remediation_hint;
};

/// Inspects Switch runtime health: Applet vs Full RAM, core NRO presence,
/// 5 shared RTP pools, CJK fallback fonts, and FAT32 vs exFAT warning.
class EnvironmentDoctor {
 public:
  [[nodiscard]] static std::vector<DiagnosticItem> run_all_checks(
      MemoryExecutionMode memory_mode);
};

}  // namespace nxrpg::launcher
