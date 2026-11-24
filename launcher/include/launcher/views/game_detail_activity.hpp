#pragma once

#include <filesystem>
#include <functional>
#include <memory>

#include "launcher/app.hpp"
#include "launcher/services/i18n_service.hpp"
#include "nxrpg/engine_detector.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher::views {

/// Game detail drawer / activity allowing:
/// - Immediate launch (`A`)
/// - Per-game overrides (`Y` -> encoding, CPU boost, scaling, RTP override, Win32API stubs, NW.js mode)
/// - Open game save folder info & play time stats
#ifdef NXRPG_HAS_BOREALIS
class GameDetailActivity : public brls::Activity {
 public:
  GameDetailActivity(
      DetectedGame game,
      BootContext ctx,
      std::shared_ptr<I18nService> i18n,
      std::function<void(const DetectedGame&)> on_launch);

  brls::View* createContentView() override;

 private:
  DetectedGame game_;
  BootContext ctx_;
  std::shared_ptr<I18nService> i18n_;
  std::function<void(const DetectedGame&)> on_launch_;
};
#else
class GameDetailActivity {
 public:
  explicit GameDetailActivity(DetectedGame game);

 private:
  DetectedGame game_;
};
#endif

}  // namespace nxrpg::launcher::views
