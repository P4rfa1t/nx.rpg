#pragma once

namespace nxrpg::launcher::views {

/// Displays `EnvironmentDoctor` checklist (Cores, RTPs, Fonts, Applet Mode, FAT32/exFAT)
/// and renders the tail + `[nx.rpg][FATAL]` summary of `runtime/last_run.log`.
class DiagnosticsTab {
 public:
  void refresh();
};

}  // namespace nxrpg::launcher::views
