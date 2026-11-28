#include "launcher/views/transfer_tab.hpp"

#include <cstdint>
#include <cstdio>

namespace nxrpg::launcher::views {

#ifdef NXRPG_HAS_BOREALIS

namespace {

std::string format_speed(double bytes_per_second) {
  char buffer[48];
  if (bytes_per_second >= 1024.0 * 1024.0) {
    std::snprintf(buffer, sizeof buffer, "%.1f MB/s", bytes_per_second / (1024.0 * 1024.0));
  } else if (bytes_per_second >= 1024.0) {
    std::snprintf(buffer, sizeof buffer, "%.0f KB/s", bytes_per_second / 1024.0);
  } else {
    std::snprintf(buffer, sizeof buffer, "%.0f B/s", bytes_per_second);
  }
  return buffer;
}

std::string format_bytes(std::uint64_t bytes) {
  char buffer[48];
  constexpr double kKib = 1024.0;
  constexpr double kMib = kKib * 1024.0;
  constexpr double kGib = kMib * 1024.0;
  if (static_cast<double>(bytes) >= kGib) {
    std::snprintf(buffer, sizeof buffer, "%.1f GB", static_cast<double>(bytes) / kGib);
  } else if (static_cast<double>(bytes) >= kMib) {
    std::snprintf(buffer, sizeof buffer, "%.1f MB", static_cast<double>(bytes) / kMib);
  } else if (bytes >= 1024) {
    std::snprintf(buffer, sizeof buffer, "%.0f KB", static_cast<double>(bytes) / kKib);
  } else {
    std::snprintf(buffer, sizeof buffer, "%llu B", static_cast<unsigned long long>(bytes));
  }
  return buffer;
}

}  // namespace

class TransferTab::RefreshTask : public brls::RepeatingTask {
 public:
  explicit RefreshTask(TransferTab* tab) : brls::RepeatingTask(500), tab_(tab) {}
  void run() override { tab_->refresh(); }

 private:
  TransferTab* tab_;
};

TransferTab::TransferTab(std::shared_ptr<I18nService> i18n,
                         std::shared_ptr<TransferService> transfer,
                         std::filesystem::path sd_root)
    : brls::Box(brls::Axis::COLUMN),
      i18n_(std::move(i18n)),
      transfer_(std::move(transfer)),
      sd_root_(std::move(sd_root)) {
  setPadding(24);
  setGrow(1.0f);

  auto* header = new brls::Header();
  header->setTitle(i18n_->translate("tabs.transfer", "File Transfer"));
  addView(header);

  auto add_note = [this](const std::string& text, float bottom) {
    auto* label = new brls::Label();
    label->setSingleLine(false);
    label->setFontSize(16);
    label->setText(text);
    label->setMarginBottom(bottom);
    addView(label);
    return label;
  };

  add_note(i18n_->translate("transfer.intro",
      "Shares switch/nx.rpg/. Both switches can be on at once and keep running "
      "on other pages; they stop when switched off, before a game launches, "
      "or when nx.rpg exits."), 12);

  mtp_toggle_ = new brls::BooleanCell();
  mtp_toggle_->init(i18n_->translate("transfer.mtp", "USB MTP"),
                    transfer_->active(TransferService::Mode::Mtp),
                    [this](bool on) { set_mode(TransferService::Mode::Mtp, on); });
  addView(mtp_toggle_);
  mtp_status_ = add_note({}, 16);

  ftp_toggle_ = new brls::BooleanCell();
  ftp_toggle_->init(i18n_->translate("transfer.ftp", "LAN FTP"),
                    transfer_->active(TransferService::Mode::Ftp),
                    [this](bool on) { set_mode(TransferService::Mode::Ftp, on); });
  addView(ftp_toggle_);
  ftp_status_ = add_note({}, 16);

  refresh();
  task_ = std::make_unique<RefreshTask>(this);
  task_->start();
}

TransferTab::~TransferTab() {
  if (task_) task_->stop();
}

void TransferTab::set_mode(TransferService::Mode mode, bool on) {
  brls::BooleanCell* cell = mode == TransferService::Mode::Mtp ? mtp_toggle_ : ftp_toggle_;
  if (!on) {
    transfer_->stop(mode);
    refresh();
    return;
  }
  std::string error = transfer_->start(mode, sd_root_);
  if (!error.empty()) {
    cell->setOn(false, true);
    brls::Application::notify(
        i18n_->translate("transfer.error", "Could not start transfer") + ": " + error);
  }
  refresh();
}

void TransferTab::refresh() {
  const TransferService::Status status = transfer_->status();
  if (mtp_toggle_->isOn() != status.mtp.active) mtp_toggle_->setOn(status.mtp.active, false);
  if (ftp_toggle_->isOn() != status.ftp.active) ftp_toggle_->setOn(status.ftp.active, false);
  mtp_status_->setText(describe(status.mtp, {}));
  ftp_status_->setText(describe(status.ftp, status.ftp_address));
}

std::string TransferTab::describe(const TransferService::ServiceStatus& status,
                                  const std::string& address) const {
  if (!status.active) return i18n_->translate("transfer.off", "Off");

  std::string text;
  const bool stopped_with_error = !status.running && !status.error.empty();
  if (stopped_with_error) {
    text = i18n_->translate("transfer.error_short", "Error") + ": " + status.error;
  } else if (status.connected) {
    text = i18n_->translate("transfer.connected", "Connected");
    if (!status.peer.empty()) text += ": " + status.peer;
  } else {
    text = i18n_->translate("transfer.waiting", "Waiting for a connection");
  }
  if (!address.empty() && !stopped_with_error) {
    text += "\n" + i18n_->translate("transfer.address", "Address") + ": " + address + "  (" +
            i18n_->translate("transfer.anonymous", "anonymous, no password") + ")";
  }
  if (status.running && !status.error.empty()) {
    text += "\n" + i18n_->translate("transfer.error_short", "Error") + ": " + status.error;
  }
  if (!status.file.empty()) {
    text += "\n" + i18n_->translate("transfer.file", "File") + ": " + status.file;
  }
  std::string activity =
      i18n_->translate("transfer.speed", "Speed") + ": " + format_speed(status.bytes_per_second);
  if (status.files) {
    activity += "  |  " + i18n_->translate("transfer.received", "Received") + ": " +
                std::to_string(status.files) + " " +
                i18n_->translate("transfer.files", "files") + " (" + format_bytes(status.bytes) + ")";
  }
  text += "\n" + activity;
  return text;
}

#endif  // NXRPG_HAS_BOREALIS

}  // namespace nxrpg::launcher::views
