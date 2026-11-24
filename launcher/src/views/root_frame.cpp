#include "launcher/views/root_frame.hpp"

#include <array>
#include <filesystem>

#include "launcher/services/config_service.hpp"
#include "launcher/views/diagnostics_tab.hpp"
#include "launcher/views/game_grid_tab.hpp"

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

RootFrame::RootFrame(BootContext ctx, std::shared_ptr<I18nService> i18n)
    : brls::AppletFrame(new brls::TabFrame()),
      ctx_(std::move(ctx)),
      i18n_(std::move(i18n)) {
  tab_frame_ = dynamic_cast<brls::TabFrame*>(getContentView());

  std::string badge = i18n_->translate("app.full_ram_badge", "Title Takeover: 3.2+ GB OK");
  if (ctx_.memory_mode == MemoryExecutionMode::AppletRestrictedRam) {
    badge = i18n_->translate("app.applet_badge", "Applet Mode: ~440 MB");
  } else if (ctx_.memory_mode == MemoryExecutionMode::HostDesktop) {
    badge = i18n_->translate("app.desktop_badge", "Desktop Preview");
  }
  setTitle(i18n_->translate("app.title", "nx.rpg") + "  [" + badge + "]");

  tab_frame_->addTab(i18n_->translate("tabs.library", "All Games"),
                     [this]() { return new GameGridTab(ctx_, i18n_, false); });

  tab_frame_->addTab(i18n_->translate("tabs.recent", "Recent"),
                     [this]() { return new GameGridTab(ctx_, i18n_, true); });

  tab_frame_->addTab(i18n_->translate("tabs.diagnostics", "Environment & Logs"),
                     [this]() { return new DiagnosticsTab(ctx_, i18n_); });

  tab_frame_->addTab(i18n_->translate("tabs.settings", "Global Settings"), [this]() {
    auto* scroll = new brls::ScrollingFrame();
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(24);
    GlobalConfig cfg = ConfigService::load_global(ctx_.sd_root);

    auto* header = new brls::Header();
    header->setTitle(i18n_->translate("tabs.settings", "Global Settings"));
    box->addView(header);

    // Language selector
    static const std::array<std::string, 5> kLangs = {"zh-CN", "zh-TW", "en-US", "ja-JP", "ko-KR"};
    auto* lang_cell = new brls::SelectorCell();
    lang_cell->init(
        i18n_->translate("settings.language", "UI Language"),
        {"zh-CN", "zh-TW", "en-US", "ja-JP", "ko-KR"},
        0,
        [this](int selected) {
          GlobalConfig updated = ConfigService::load_global(ctx_.sd_root);
          updated.ui_language = kLangs[static_cast<std::size_t>(selected)];
          (void)ConfigService::save_global(ctx_.sd_root, updated);
        });
    for (std::size_t i = 0; i < kLangs.size(); ++i) {
      if (kLangs[i] == cfg.ui_language) {
        lang_cell->setSelection(static_cast<int>(i), true);
        break;
      }
    }
    box->addView(lang_cell);

    // CPU profile selector
    static const std::array<CpuProfile, 3> kCpus = {
        CpuProfile::Stock1020, CpuProfile::Boost1581, CpuProfile::Boost1785};
    auto* cpu_cell = new brls::SelectorCell();
    int cpu_idx = 0;
    for (std::size_t i = 0; i < kCpus.size(); ++i) {
      if (kCpus[i] == cfg.performance.default_cpu_profile) cpu_idx = static_cast<int>(i);
    }
    cpu_cell->init(
        i18n_->translate("settings.cpu_profile", "CPU Clock Profile"),
        {"1020 MHz (Stock)", "1581 MHz (Boost)", "1785 MHz (Max Boost)"},
        cpu_idx,
        [this](int selected) {
          GlobalConfig updated = ConfigService::load_global(ctx_.sd_root);
          updated.performance.default_cpu_profile = kCpus[static_cast<std::size_t>(selected)];
          (void)ConfigService::save_global(ctx_.sd_root, updated);
        });
    box->addView(cpu_cell);

    // Scaling mode selector
    static const std::array<ScalingMode, 3> kScales = {
        ScalingMode::Fit, ScalingMode::Stretch, ScalingMode::Native};
    auto* scale_cell = new brls::SelectorCell();
    int scale_idx = 0;
    for (std::size_t i = 0; i < kScales.size(); ++i) {
      if (kScales[i] == cfg.video.scaling_mode) scale_idx = static_cast<int>(i);
    }
    scale_cell->init(
        i18n_->translate("settings.scaling_mode", "Scaling Mode"),
        {"fit", "stretch", "native"},
        scale_idx,
        [this](int selected) {
          GlobalConfig updated = ConfigService::load_global(ctx_.sd_root);
          updated.video.scaling_mode = kScales[static_cast<std::size_t>(selected)];
          (void)ConfigService::save_global(ctx_.sd_root, updated);
        });
    box->addView(scale_cell);

    // Boolean toggles
    auto add_bool = [&](const std::string& label, bool initial, std::function<void(GlobalConfig&, bool)> setter) {
      auto* cell = new brls::BooleanCell();
      cell->init(label, initial, [this, setter](bool val) {
        GlobalConfig updated = ConfigService::load_global(ctx_.sd_root);
        setter(updated, val);
        (void)ConfigService::save_global(ctx_.sd_root, updated);
      });
      box->addView(cell);
    };

    add_bool(i18n_->translate("settings.integer_scale"), cfg.video.integer_scale,
             [](GlobalConfig& c, bool v) { c.video.integer_scale = v; });
    add_bool(i18n_->translate("settings.vsync"), cfg.video.vsync,
             [](GlobalConfig& c, bool v) { c.video.vsync = v; });
    add_bool(i18n_->translate("settings.show_fps"), cfg.video.show_fps,
             [](GlobalConfig& c, bool v) { c.video.show_fps = v; });
    add_bool(i18n_->translate("settings.swap_confirm_cancel"), cfg.input.swap_confirm_cancel,
             [](GlobalConfig& c, bool v) { c.input.swap_confirm_cancel = v; });
    add_bool(i18n_->translate("settings.enable_touch"), cfg.input.enable_touch,
             [](GlobalConfig& c, bool v) { c.input.enable_touch = v; });
    add_bool(i18n_->translate("settings.warn_applet"), cfg.warn_on_applet_mode,
             [](GlobalConfig& c, bool v) { c.warn_on_applet_mode = v; });

    scroll->setContentView(box);
    return scroll;
  });
}

#endif  // NXRPG_HAS_BOREALIS

}  // namespace nxrpg::launcher::views
