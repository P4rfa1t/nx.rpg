#include "launcher/views/game_detail_activity.hpp"

#include <array>
#include <filesystem>
#include <system_error>

#include "launcher/services/config_service.hpp"

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

namespace {

std::string format_play_time(std::uint64_t seconds) {
  std::uint64_t h = seconds / 3600;
  std::uint64_t m = (seconds % 3600) / 60;
  std::uint64_t s = seconds % 60;
  return std::to_string(h) + "h " + std::to_string(m) + "m " + std::to_string(s) + "s";
}

std::size_t count_save_files(const std::filesystem::path& save_dir) {
  std::error_code ec;
  if (!std::filesystem::is_directory(save_dir, ec)) return 0;
  std::size_t count = 0;
  for (std::filesystem::recursive_directory_iterator it(save_dir, ec);
       !ec && it != std::filesystem::recursive_directory_iterator();
       it.increment(ec)) {
    if (it->is_regular_file(ec)) ++count;
  }
  return count;
}

}  // namespace

GameDetailActivity::GameDetailActivity(
    LibraryEntry entry,
    BootContext ctx,
    std::shared_ptr<I18nService> i18n,
    std::function<void(const DetectedGame&)> on_launch,
    std::function<void()> on_changed)
    : entry_(std::move(entry)),
      ctx_(std::move(ctx)),
      i18n_(std::move(i18n)),
      on_launch_(std::move(on_launch)),
      on_changed_(std::move(on_changed)) {}

brls::View* GameDetailActivity::createContentView() {
  const DetectedGame& game = entry_.game;
  auto* scroll = new brls::ScrollingFrame();
  auto* box = new brls::Box(brls::Axis::COLUMN);
  box->setPadding(24);

  PerGameConfig cfg = ConfigService::load_per_game(ctx_.sd_root, game.id);
  GlobalConfig gcfg = ConfigService::load_global(ctx_.sd_root);

  // Launch action button
  auto* launch_btn = new brls::DetailCell();
  launch_btn->setText(i18n_->translate("actions.launch", "Launch Game"));
  launch_btn->setDetailText("Core: " + std::string(to_string(game.core)) + " | execName: " + game.exec_name);
  launch_btn->registerClickAction([this](brls::View*) {
    if (on_launch_) on_launch_(entry_.game);
    return true;
  });
  box->addView(launch_btn);

  // Save folder & Play time summary
  std::filesystem::path save_dir = std::filesystem::path(ctx_.sd_root) / "saves" / game.id;
  auto* stats_cell = new brls::DetailCell();
  stats_cell->setText(i18n_->translate("stats.play_time", "Play Time") + ": " +
                      format_play_time(entry_.play_time_seconds));
  stats_cell->setDetailText(i18n_->translate("stats.save_dir", "Save Path") + ": " +
                            save_dir.generic_string() + " (" +
                            std::to_string(count_save_files(save_dir)) + " files)");
  box->addView(stats_cell);

  // Favorite toggle
  auto* fav_cell = new brls::BooleanCell();
  fav_cell->init(
      i18n_->translate("stats.favorite", "Favorite"),
      entry_.favorite,
      [this](bool val) {
        LibraryService svc(ctx_.sd_root);
        auto all = svc.load_cached_entries();
        for (auto& e : all) {
          if (e.game.id == entry_.game.id) {
            e.favorite = val;
            entry_.favorite = val;
            break;
          }
        }
        (void)svc.save_entries(all);
        if (on_changed_) on_changed_();
      });
  box->addView(fav_cell);

  // Text encoding override
  static const std::array<TextEncoding, 7> kEncodings = {
      TextEncoding::Auto, TextEncoding::Utf8, TextEncoding::ShiftJis,
      TextEncoding::Gbk, TextEncoding::Big5, TextEncoding::Cp949, TextEncoding::Cp1252};
  int enc_idx = 0;
  for (std::size_t i = 0; i < kEncodings.size(); ++i) {
    if (kEncodings[i] == cfg.encoding) enc_idx = static_cast<int>(i);
  }
  auto* enc_cell = new brls::SelectorCell();
  enc_cell->init(
      i18n_->translate("settings.default_encoding", "Text Encoding"),
      {"auto", "utf-8", "shift_jis", "gbk", "big5", "cp949", "cp1252"},
      enc_idx,
      [this](int selected) {
        PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, entry_.game.id);
        updated.encoding = kEncodings[static_cast<std::size_t>(selected)];
        (void)ConfigService::save_per_game(ctx_.sd_root, entry_.game.id, updated);
      });
  box->addView(enc_cell);

  // CPU profile override
  static const std::array<const char*, 4> kCpus = {
      "inherit", "stock_1020", "boost_1581", "boost_1785"};
  int cpu_idx = 0;
  for (std::size_t i = 0; i < kCpus.size(); ++i) {
    if (cfg.cpu_profile == kCpus[i]) cpu_idx = static_cast<int>(i);
  }
  auto* cpu_cell = new brls::SelectorCell();
  cpu_cell->init(
      i18n_->translate("settings.cpu_profile", "CPU Profile"),
      {"Inherit Global", "1020 MHz", "1581 MHz", "1785 MHz"},
      cpu_idx,
      [this](int selected) {
        PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, entry_.game.id);
        updated.cpu_profile = kCpus[static_cast<std::size_t>(selected)];
        (void)ConfigService::save_per_game(ctx_.sd_root, entry_.game.id, updated);
      });
  box->addView(cpu_cell);

  // Scaling override
  static const std::array<ScalingMode, 3> kScales = {
      ScalingMode::Fit, ScalingMode::Stretch, ScalingMode::Native};
  int scale_idx = 0;
  ScalingMode eff_scale = cfg.scaling_mode.value_or(gcfg.video.scaling_mode);
  for (std::size_t i = 0; i < kScales.size(); ++i) {
    if (eff_scale == kScales[i]) scale_idx = static_cast<int>(i);
  }
  auto* scale_cell = new brls::SelectorCell();
  scale_cell->init(
      i18n_->translate("settings.scaling_mode", "Scaling Mode"),
      {"fit", "stretch", "native"},
      scale_idx,
      [this](int selected) {
        PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, entry_.game.id);
        updated.scaling_mode = kScales[static_cast<std::size_t>(selected)];
        (void)ConfigService::save_per_game(ctx_.sd_root, entry_.game.id, updated);
      });
  box->addView(scale_cell);

  auto add_bool = [&](const std::string& label, bool initial, std::function<void(PerGameConfig&, bool)> setter) {
    auto* cell = new brls::BooleanCell();
    cell->init(label, initial, [this, setter](bool val) {
      PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, entry_.game.id);
      setter(updated, val);
      (void)ConfigService::save_per_game(ctx_.sd_root, entry_.game.id, updated);
    });
    box->addView(cell);
  };

  add_bool(i18n_->translate("settings.integer_scale"),
           cfg.integer_scale.value_or(gcfg.video.integer_scale),
           [](PerGameConfig& c, bool v) { c.integer_scale = v; });
  add_bool(i18n_->translate("settings.vsync"),
           cfg.vsync.value_or(gcfg.video.vsync),
           [](PerGameConfig& c, bool v) { c.vsync = v; });
  add_bool(i18n_->translate("settings.show_fps"),
           cfg.show_fps.value_or(gcfg.video.show_fps),
           [](PerGameConfig& c, bool v) { c.show_fps = v; });
  add_bool(i18n_->translate("settings.swap_confirm_cancel"),
           cfg.swap_confirm_cancel.value_or(gcfg.input.swap_confirm_cancel),
           [](PerGameConfig& c, bool v) { c.swap_confirm_cancel = v; });
  add_bool(i18n_->translate("settings.enable_touch"),
           cfg.enable_touch.value_or(gcfg.input.enable_touch),
           [](PerGameConfig& c, bool v) { c.enable_touch = v; });

  // Core-specific toggles
  if (game.core == CoreKind::EasyRpg) {
    add_bool(i18n_->translate("settings.disable_rtp", "Disable Shared RTP"),
             cfg.easyrpg_disable_rtp,
             [](PerGameConfig& c, bool v) { c.easyrpg_disable_rtp = v; });
  } else if (game.core == CoreKind::NxjsMvz) {
    add_bool(i18n_->translate("settings.pretend_nwjs", "Emulate NW.js"),
             cfg.mvz_pretend_nwjs,
             [](PerGameConfig& c, bool v) { c.mvz_pretend_nwjs = v; });
  } else if (game.core == CoreKind::MkxpZ) {
    add_bool(i18n_->translate("settings.win32_stubs", "Enable Win32API Stubs"),
             cfg.mkxp_enable_win32_api_stubs,
             [](PerGameConfig& c, bool v) { c.mkxp_enable_win32_api_stubs = v; });
  }

  scroll->setContentView(box);
  auto* frame = new brls::AppletFrame(scroll);
  frame->setTitle(game.title + " [" + std::string(to_string(game.engine)) + "]");
  return frame;
}

#else

GameDetailActivity::GameDetailActivity(DetectedGame game) : game_(std::move(game)) {}

#endif  // NXRPG_HAS_BOREALIS

}  // namespace nxrpg::launcher::views
