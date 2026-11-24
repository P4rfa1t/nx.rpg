#pragma once

#include <memory>
#include <vector>

#include "launcher/app.hpp"
#include "launcher/services/i18n_service.hpp"
#include "launcher/services/library_service.hpp"
#include "nxrpg/types.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher::views {

/// Controller- and touch-friendly Borealis list/grid displaying game cards
/// with engine badges (`2K` / `2K3` / `XP` / `VX` / `VXA` / `MV` / `MZ`) and filter chips.
#ifdef NXRPG_HAS_BOREALIS
class GameGridTab : public brls::Box {
 public:
  GameGridTab(BootContext ctx, std::shared_ptr<I18nService> i18n, bool recent_only);
  ~GameGridTab() override = default;

  void set_engine_filter(EngineKind filter_or_unknown_for_all);
  void reload(bool force_rescan);

 private:
  void rebuild_rows();
  void launch_game_with_precheck(const DetectedGame& game);

  BootContext ctx_;
  std::shared_ptr<I18nService> i18n_;
  bool recent_only_ = false;
  EngineKind filter_ = EngineKind::Unknown;
  std::vector<LibraryEntry> entries_;
  brls::ScrollingFrame* scroll_ = nullptr;
  brls::Box* list_box_ = nullptr;
};
#else
class GameGridTab {
 public:
  void set_engine_filter(EngineKind filter_or_unknown_for_all);
};
#endif

}  // namespace nxrpg::launcher::views
