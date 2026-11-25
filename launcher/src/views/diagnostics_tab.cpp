#include "launcher/views/diagnostics_tab.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "launcher/services/environment_doctor.hpp"
#include "nxrpg/logger.hpp"

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

namespace {

const char* severity_prefix(DiagnosticItem::Severity s) {
  switch (s) {
    case DiagnosticItem::Severity::Ok: return "[OK]  ";
    case DiagnosticItem::Severity::Warning: return "[WARN]";
    case DiagnosticItem::Severity::Error: return "[ERR] ";
  }
  return "[?]   ";
}

}  // namespace

DiagnosticsTab::DiagnosticsTab(BootContext ctx, std::shared_ptr<I18nService> i18n)
    : brls::Box(brls::Axis::COLUMN), ctx_(std::move(ctx)), i18n_(std::move(i18n)) {
  setGrow(1.0f);
  setPadding(16);

  auto* scroll = new brls::ScrollingFrame();
  scroll->setGrow(1.0f);
  list_box_ = new brls::Box(brls::Axis::COLUMN);
  scroll->setContentView(list_box_);
  addView(scroll);

  registerAction(
      i18n_->translate("actions.rescan", "Refresh"),
      brls::BUTTON_X,
      [this](brls::View*) {
        refresh();
        return true;
      });

  refresh();
}

void DiagnosticsTab::refresh() {
  list_box_->clearViews();

  auto* header = new brls::Header();
  header->setTitle(i18n_->translate("tabs.diagnostics", "Environment & Logs"));
  list_box_->addView(header);

  // Orphaned .tmp cleanup — plain label row (left column only)
  auto* cleanup_btn = new brls::BooleanCell();
  std::size_t orphaned = EnvironmentDoctor::count_orphaned_tmp_files(ctx_.sd_root);
  cleanup_btn->init(
      i18n_->translate("actions.cleanup_tmp", "Clean .tmp & Refresh"),
      false,
      [this](bool) {
        std::size_t n = EnvironmentDoctor::cleanup_orphaned_tmp_files(ctx_.sd_root);
        NXLOG_INFO("diag", "cleanup removed %zu orphaned .tmp file(s)", n);
        brls::Application::notify(
            i18n_->translate("diagnostics.tmp_cleaned", "Cleaned ") +
            std::to_string(n));
        refresh();
        return true;
      });
  cleanup_btn->setDetailText(std::to_string(orphaned));
  list_box_->addView(cleanup_btn);

  // Single-column checklist: everything renders in the LEFT text only.
  auto items = EnvironmentDoctor::run_all_checks(ctx_.memory_mode, ctx_.sd_root);
  for (const auto& item : items) {
    auto* lbl = new brls::Label();
    std::string line = severity_prefix(item.severity);
    line += item.component;
    line += "  —  ";
    line += item.summary;
    if (!item.remediation_hint.empty()) {
      line += "  (";
      line += item.remediation_hint;
      line += ")";
    }
    lbl->setText(line);
    list_box_->addView(lbl);
  }

  // ---- Last run log tail ----
  auto log_sum = EnvironmentDoctor::inspect_last_run_log(
      std::filesystem::path(ctx_.sd_root) / "runtime/last_run.log", 20);
  if (log_sum.exists) {
    auto* log_header = new brls::Header();
    log_header->setTitle(i18n_->translate("actions.view_last_log", "Last Run Log"));
    if (log_sum.has_fatal) {
      log_header->setSubtitle("FATAL [" + log_sum.fatal_core + "/" + log_sum.fatal_code + "] " +
                              log_sum.fatal_message);
    }
    list_box_->addView(log_header);

    for (const auto& line : log_sum.tail_lines) {
      auto* lbl = new brls::Label();
      lbl->setText("  " + line);
      list_box_->addView(lbl);
    }
  }

  // ---- Launcher log tail ----
  auto launcher_log_sum = EnvironmentDoctor::inspect_last_run_log(
      std::filesystem::path(ctx_.sd_root) / "runtime/launcher.log", 30);
  if (launcher_log_sum.exists) {
    auto* launcher_log_header = new brls::Header();
    launcher_log_header->setTitle(i18n_->translate("diagnostics.launcher_log", "Launcher Log"));
    list_box_->addView(launcher_log_header);

    for (const auto& line : launcher_log_sum.tail_lines) {
      auto* lbl = new brls::Label();
      lbl->setText("  " + line);
      list_box_->addView(lbl);
    }
  }
}


#else

void DiagnosticsTab::refresh() {}

#endif  // NXRPG_HAS_BOREALIS

}  // namespace nxrpg::launcher::views
