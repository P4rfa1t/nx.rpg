#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "launcher/services/i18n_service.hpp"
#include "launcher/services/transfer_service.hpp"

#ifdef NXRPG_HAS_BOREALIS
#include <borealis.hpp>
#endif

namespace nxrpg::launcher::views {

/// The File Transfer tab: one switch per backend (USB MTP, LAN FTP) plus a
/// live status line for each — connection, address, current file and speed.
/// The services are owned by the launcher, so a transfer keeps running while
/// the user moves to another tab; only the switches, a game launch or exiting
/// the launcher stop it.
#ifdef NXRPG_HAS_BOREALIS
class TransferTab : public brls::Box {
 public:
  TransferTab(std::shared_ptr<I18nService> i18n,
              std::shared_ptr<TransferService> transfer,
              std::filesystem::path sd_root);
  ~TransferTab() override;

 private:
  class RefreshTask;

  void set_mode(TransferService::Mode mode, bool on);
  void refresh();
  std::string describe(const TransferService::ServiceStatus& status,
                       const std::string& address) const;

  std::shared_ptr<I18nService> i18n_;
  std::shared_ptr<TransferService> transfer_;
  std::filesystem::path sd_root_;
  brls::BooleanCell* mtp_toggle_ = nullptr;
  brls::BooleanCell* ftp_toggle_ = nullptr;
  brls::Label* mtp_status_ = nullptr;
  brls::Label* ftp_status_ = nullptr;
  std::unique_ptr<RefreshTask> task_;
};
#else
class TransferTab {
 public:
  TransferTab() = default;
};
#endif

}  // namespace nxrpg::launcher::views
