#pragma once

#include <memory>

#include "launcher/app.hpp"
#include "launcher/services/i18n_service.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher::views {

/// Displays `EnvironmentDoctor` checklist (Cores, RTPs, Fonts, Applet Mode, FAT32/exFAT)
/// and renders the tail + `[nx.rpg][FATAL]` summary of `runtime/last_run.log`.
#ifdef NXRPG_HAS_BOREALIS
class DiagnosticsTab : public brls::Box {
 public:
  DiagnosticsTab(BootContext ctx, std::shared_ptr<I18nService> i18n);
  ~DiagnosticsTab() override = default;

  void refresh();

 private:
  BootContext ctx_;
  std::shared_ptr<I18nService> i18n_;
  brls::Box* list_box_ = nullptr;
};
#else
class DiagnosticsTab {
 public:
  void refresh();
};
#endif

}  // namespace nxrpg::launcher::views
