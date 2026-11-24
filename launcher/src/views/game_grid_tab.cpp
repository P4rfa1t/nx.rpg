#include "launcher/views/game_grid_tab.hpp"

#include <algorithm>

#include "launcher/services/config_service.hpp"
#include "launcher/services/core_dispatcher.hpp"
#include "launcher/views/game_detail_activity.hpp"

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

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

  reload(false);
}

void GameGridTab::set_engine_filter(EngineKind filter_or_unknown_for_all) {
  filter_ = filter_or_unknown_for_all;
  rebuild_rows();
}

void GameGridTab::reload(bool force_rescan) {
  LibraryService svc(ctx_.sd_root);
  entries_ = svc.load_or_refresh_entries(force_rescan);
  rebuild_rows();
}

void GameGridTab::rebuild_rows() {
  list_box_->clearViews();

  GlobalConfig gcfg = ConfigService::load_global(ctx_.sd_root);
  brls::View* focus_target = nullptr;

  for (const auto& entry : entries_) {
    if (recent_only_ && entry.last_played_at == 0 && !entry.favorite) {
      continue;
    }
    if (filter_ != EngineKind::Unknown && entry.game.engine != filter_) {
      continue;
    }

    auto* cell = new brls::DetailCell();
    std::string engine_badge = "[" + std::string(to_string(entry.game.engine)) + "]";
    std::string subtitle = engine_badge + " -> " + std::string(to_string(entry.game.core));
    if (entry.game.needs_rtp) subtitle += " | RTP";
    if (entry.favorite) subtitle += " | ★";

    cell->setText(entry.game.title);
    cell->setDetailText(subtitle);

    DetectedGame game_copy = entry.game;
    cell->registerClickAction([this, game_copy](brls::View*) {
      launch_game_with_precheck(game_copy);
      return true;
    });
    cell->registerAction(
        i18n_->translate("actions.per_game_settings", "Game Settings"),
        brls::BUTTON_Y,
        [this, game_copy](brls::View*) {
          auto* act = new GameDetailActivity(
              game_copy, ctx_, i18n_, [this](const DetectedGame& g) {
                launch_game_with_precheck(g);
              });
          brls::Application::pushActivity(act);
          return true;
        });

    list_box_->addView(cell);
    if (!focus_target && !gcfg.library.last_selected_game_id.empty() &&
        entry.game.id == gcfg.library.last_selected_game_id) {
      focus_target = cell;
    }
  }

  if (list_box_->getChildren().empty()) {
    auto* empty = new brls::Label();
    empty->setText("No RPG Maker games found under sdmc:/switch/nx.rpg/games/");
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
      brls::Application::notify("Dispatched launch.json (Switch envSetNextLoad inactive on host)");
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
