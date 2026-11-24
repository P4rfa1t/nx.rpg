#include "launcher/views/game_detail_activity.hpp"

#include <array>

#include "launcher/services/config_service.hpp"

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

GameDetailActivity::GameDetailActivity(
    DetectedGame game,
    BootContext ctx,
    std::shared_ptr<I18nService> i18n,
    std::function<void(const DetectedGame&)> on_launch)
    : game_(std::move(game)),
      ctx_(std::move(ctx)),
      i18n_(std::move(i18n)),
      on_launch_(std::move(on_launch)) {}

brls::View* GameDetailActivity::createContentView() {
  auto* scroll = new brls::ScrollingFrame();
  auto* box = new brls::Box(brls::Axis::COLUMN);
  box->setPadding(24);

  PerGameConfig cfg = ConfigService::load_per_game(ctx_.sd_root, game_.id);

  // Launch action button
  auto* launch_btn = new brls::DetailCell();
  launch_btn->setText(i18n_->translate("actions.launch", "Launch Game"));
  launch_btn->setDetailText("Core: " + std::string(to_string(game_.core)) + " | execName: " + game_.exec_name);
  launch_btn->registerClickAction([this](brls::View*) {
    if (on_launch_) on_launch_(game_);
    return true;
  });
  box->addView(launch_btn);

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
        PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, game_.id);
        updated.encoding = kEncodings[static_cast<std::size_t>(selected)];
        (void)ConfigService::save_per_game(ctx_.sd_root, game_.id, updated);
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
        PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, game_.id);
        updated.cpu_profile = kCpus[static_cast<std::size_t>(selected)];
        (void)ConfigService::save_per_game(ctx_.sd_root, game_.id, updated);
      });
  box->addView(cpu_cell);

  // Scaling override
  static const std::array<ScalingMode, 3> kScales = {
      ScalingMode::Fit, ScalingMode::Stretch, ScalingMode::Native};
  int scale_idx = 0;
  if (cfg.scaling_mode) {
    for (std::size_t i = 0; i < kScales.size(); ++i) {
      if (*cfg.scaling_mode == kScales[i]) scale_idx = static_cast<int>(i);
    }
  }
  auto* scale_cell = new brls::SelectorCell();
  scale_cell->init(
      i18n_->translate("settings.scaling_mode", "Scaling Mode"),
      {"fit", "stretch", "native"},
      scale_idx,
      [this](int selected) {
        PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, game_.id);
        updated.scaling_mode = kScales[static_cast<std::size_t>(selected)];
        (void)ConfigService::save_per_game(ctx_.sd_root, game_.id, updated);
      });
  box->addView(scale_cell);

  // Core-specific toggles
  if (game_.core == CoreKind::EasyRpg) {
    auto* rtp_cell = new brls::BooleanCell();
    rtp_cell->init(
        i18n_->translate("settings.disable_rtp", "Disable Shared RTP"),
        cfg.easyrpg_disable_rtp,
        [this](bool val) {
          PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, game_.id);
          updated.easyrpg_disable_rtp = val;
          (void)ConfigService::save_per_game(ctx_.sd_root, game_.id, updated);
        });
    box->addView(rtp_cell);
  } else if (game_.core == CoreKind::NxjsMvz) {
    auto* nwjs_cell = new brls::BooleanCell();
    nwjs_cell->init(
        i18n_->translate("settings.pretend_nwjs", "Emulate NW.js"),
        cfg.mvz_pretend_nwjs,
        [this](bool val) {
          PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, game_.id);
          updated.mvz_pretend_nwjs = val;
          (void)ConfigService::save_per_game(ctx_.sd_root, game_.id, updated);
        });
    box->addView(nwjs_cell);
  } else if (game_.core == CoreKind::MkxpZ) {
    auto* w32_cell = new brls::BooleanCell();
    w32_cell->init(
        i18n_->translate("settings.win32_stubs", "Enable Win32API Stubs"),
        cfg.mkxp_enable_win32_api_stubs,
        [this](bool val) {
          PerGameConfig updated = ConfigService::load_per_game(ctx_.sd_root, game_.id);
          updated.mkxp_enable_win32_api_stubs = val;
          (void)ConfigService::save_per_game(ctx_.sd_root, game_.id, updated);
        });
    box->addView(w32_cell);
  }

  scroll->setContentView(box);
  auto* frame = new brls::AppletFrame(scroll);
  frame->setTitle(game_.title + " [" + std::string(to_string(game_.engine)) + "]");
  return frame;
}

#else

GameDetailActivity::GameDetailActivity(DetectedGame game) : game_(std::move(game)) {}

#endif  // NXRPG_HAS_BOREALIS

}  // namespace nxrpg::launcher::views
