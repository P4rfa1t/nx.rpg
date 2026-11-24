#pragma once

#include <filesystem>
#include <memory>

#include "launcher/app.hpp"
#include "launcher/services/i18n_service.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher::views {

/// Primary Borealis `brls::AppletFrame` wrapping a `brls::TabFrame` hosting:
/// 1. All Games (`GameGridTab`)
/// 2. Recently Played / Favorites (`GameGridTab` recent mode)
/// 3. Environment & Crash Log Doctor (`DiagnosticsTab`)
/// 4. Global Settings (`config/global.json`)
#ifdef NXRPG_HAS_BOREALIS
class RootFrame : public brls::AppletFrame {
 public:
  RootFrame(BootContext ctx, std::shared_ptr<I18nService> i18n);
  ~RootFrame() override = default;

 private:
  BootContext ctx_;
  std::shared_ptr<I18nService> i18n_;
  brls::TabFrame* tab_frame_ = nullptr;
};
#else
class RootFrame {
 public:
  RootFrame() = default;
};
#endif

}  // namespace nxrpg::launcher::views
