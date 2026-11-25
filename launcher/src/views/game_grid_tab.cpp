#include "launcher/views/game_grid_tab.hpp"

#include <algorithm>
#include <array>

#include "launcher/services/config_service.hpp"
#include "launcher/services/core_dispatcher.hpp"
#include "launcher/views/game_detail_activity.hpp"
#include "nxrpg/logger.hpp"

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

namespace {

constexpr std::array<EngineKind, 8> kFilterOrder = {
    EngineKind::Unknown,  // All engines
    EngineKind::Rm2k,
    EngineKind::Rm2k3,
    EngineKind::Rmxp,
    EngineKind::Rmvx,
    EngineKind::Rmvxa,
    EngineKind::Rmmv,
    EngineKind::Rmmz,
};

std::string format_duration_short(std::uint64_t secs) {
  if (secs == 0) return "";
  std::uint64_t mins = secs / 60;
  std::uint64_t hrs = mins / 60;
  if (hrs > 0) return std::to_string(hrs) + "h " + std::to_string(mins % 60) + "m";
  if (mins > 0) return std::to_string(mins) + "m";
  return std::to_string(secs) + "s";
}

}  // namespace

GameGridTab::GameGridTab(BootContext ctx, std::shared_ptr<I18nService> i18n, bool recent_only)
    : brls::Box(brls::Axis::COLUMN),
      ctx_(std::move(ctx)),
      i18n_(std::move(i18n)),
      recent_only_(recent_only) {
  setGrow(1.0f);
  setPadding(16);

  scroll_ = new brls::ScrollingFrame();
  scroll_->setGrow(1.0f);
  list_box_ = new brls::Box(brls::Axis::COLUMN);
  scroll_->setContentView(list_box_);
  addView(scroll_);

  registerAction(
      i18n_->translate("actions.rescan", "Rescan"),
      brls::BUTTON_X,
      [this](brls::View*) {
        reload(true);
        return true;
      });

  auto cycle_filter = [this](int delta) {
    auto it = std::find(kFilterOrder.begin(), kFilterOrder.end(), filter_);
    std::size_t idx = (it == kFilterOrder.end()) ? 0 : static_cast<std::size_t>(it - kFilterOrder.begin());
    idx = (idx + kFilterOrder.size() + static_cast<std::size_t>(delta)) % kFilterOrder.size();
    set_engine_filter(kFilterOrder[idx]);
  };

  registerAction(
      i18n_->translate("actions.filter_engine", "Filter -"),
      brls::BUTTON_LT,
      [cycle_filter](brls::View*) {
        cycle_filter(-1);
        return true;
      });

  registerAction(
      i18n_->translate("actions.filter_engine", "Filter +"),
      brls::BUTTON_RT,
      [cycle_filter](brls::View*) {
        cycle_filter(1);
        return true;
      });

  reload(false);
}

void GameGridTab::set_engine_filter(EngineKind filter_or_unknown_for_all) {
  filter_ = filter_or_unknown_for_all;
  rebuild_rows();
}

void GameGridTab::reload(bool force_rescan) {
  LibraryService svc(ctx_.sd_root);
  entries_ = svc.load_or_refresh_entries(force_rescan);
  bool updated_covers = false;
  for (auto& entry : entries_) {
    if (entry.cover_cache_path.empty()) {
      auto cover = svc.ensure_cover_thumbnail(entry.game);
      if (!cover.empty()) {
        entry.cover_cache_path = cover.generic_string();
        updated_covers = true;
      }
    }
  }
  if (updated_covers) {
    (void)svc.save_entries(entries_);
  }
  rebuild_rows();
}

void GameGridTab::rebuild_rows() {
  list_box_->clearViews();

  std::vector<const LibraryEntry*> visible;
  for (const auto& entry : entries_) {
    if (recent_only_ && entry.last_played_at == 0 && !entry.favorite) {
      continue;
    }
    if (filter_ != EngineKind::Unknown && entry.game.engine != filter_) {
      continue;
    }
    visible.push_back(&entry);
  }

  auto* header = new brls::Header();
  std::string filter_label = (filter_ == EngineKind::Unknown) ? "ALL" : std::string(to_string(filter_));
  header->setTitle((recent_only_ ? i18n_->translate("tabs.recent", "Recent")
                                 : i18n_->translate("tabs.library", "All Games")) +
                   " (" + std::to_string(visible.size()) + ")");
  header->setSubtitle(i18n_->translate("actions.filter_engine", "Filter") + ": " + filter_label + "  [ZL/ZR]");
  list_box_->addView(header);

  GlobalConfig gcfg = ConfigService::load_global(ctx_.sd_root);
  brls::View* focus_target = nullptr;

  for (const LibraryEntry* ep : visible) {
    const LibraryEntry& entry = *ep;
    auto* cell = new brls::DetailCell();
    std::string engine_badge = "[" + std::string(to_string(entry.game.engine)) + "]";
    std::string subtitle = engine_badge + " -> " + std::string(to_string(entry.game.core));
    if (entry.game.needs_rtp) subtitle += " | RTP";
    if (entry.favorite) subtitle += " | ★";
    std::string pt = format_duration_short(entry.play_time_seconds);
    if (!pt.empty()) subtitle += " | " + pt;

    cell->setText(entry.game.title);
    cell->setDetailText(subtitle);

    DetectedGame game_copy = entry.game;
    LibraryEntry entry_copy = entry;
    cell->registerClickAction([this, game_copy](brls::View*) {
      launch_game_with_precheck(game_copy);
      return true;
    });
    cell->registerAction(
        i18n_->translate("actions.per_game_settings", "Game Settings"),
        brls::BUTTON_Y,
        [this, entry_copy](brls::View*) {
          auto* act = new GameDetailActivity(
              entry_copy,
              ctx_,
              i18n_,
              [this](const DetectedGame& g) { launch_game_with_precheck(g); },
              [this]() { reload(false); });
          brls::Application::pushActivity(act);
          return true;
        });

    list_box_->addView(cell);
    if (!focus_target && !gcfg.library.last_selected_game_id.empty() &&
        entry.game.id == gcfg.library.last_selected_game_id) {
      focus_target = cell;
    }
  }

  if (visible.empty()) {
    auto* empty = new brls::Label();
    empty->setText("No RPG Maker games found matching current filter.");
    list_box_->addView(empty);
  } else if (focus_target) {
    brls::Application::giveFocus(focus_target);
  }
}

void GameGridTab::launch_game_with_precheck(const DetectedGame& game) {
  LaunchPrecheckResult check = CoreDispatcher::precheck(game, ctx_.memory_mode, ctx_.sd_root);

  auto execute_launch = [this, game]() {
    if (CoreDispatcher::dispatch(game, ctx_.sd_root, true)) {
      brls::Application::quit();
    } else {
      // Dispatch failed (launch manifest write or envSetNextLoad) — surface the
      // error instead of the misleading "dispatched" toast; user can retry.
      NXLOG_ERROR("grid", "dispatch failed for game=%s", game.id.c_str());
      brls::Application::notify(
          i18n_->translate("precheck.dispatch_failed", "Launch failed: could not write launch manifest. See Environment & Logs."));
    }
  };

  switch (check.status) {
    case LaunchPrecheckStatus::Ready:
      execute_launch();
      break;
    case LaunchPrecheckStatus::WarnAppletModeHighRamCore:
    case LaunchPrecheckStatus::WarnMissingSharedRtp: {
      auto* dialog = new brls::Dialog(check.detail_message);
      dialog->addButton(i18n_->translate("actions.continue_anyway", "Launch Anyway"), [execute_launch]() {
        execute_launch();
      });
      dialog->addButton(i18n_->translate("actions.cancel", "Cancel"), []() {});
      dialog->open();
      break;
    }
    case LaunchPrecheckStatus::ErrorCoreNroMissing:
    case LaunchPrecheckStatus::ErrorGamePathMissing:
    case LaunchPrecheckStatus::ErrorManifestWriteFailed:
    default:
      brls::Application::notify(check.detail_message);
      break;
  }
}

#else

void GameGridTab::set_engine_filter(EngineKind) {}

#endif  // NXRPG_HAS_BOREALIS

}  // namespace nxrpg::launcher::views
