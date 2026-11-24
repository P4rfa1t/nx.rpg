#pragma once

#include <string>

#include "nxrpg/types.hpp"

namespace nxrpg::launcher {

struct BootContext {
  bool returned_from_core = false;  // True when launched with `--from-core`
  MemoryExecutionMode memory_mode = MemoryExecutionMode::ApplicationFullRam;
  std::string sd_root = "sdmc:/switch/nx.rpg";
};

/// Top-level Borealis application controller for `nx.rpg.nro`.
class LauncherApp {
 public:
  static BootContext parse_boot_context(int argc, const char* const* argv);

  /// Initializes libnx/romfs (on Switch), loads `global.json` + i18n, checks
  /// `runtime/last_run.log` if `returned_from_core` is true, and mounts `RootFrame`.
  int run(const BootContext& ctx);
};

}  // namespace nxrpg::launcher
