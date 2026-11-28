#include "launcher/services/transfer_service.hpp"

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
#include <arpa/inet.h>
#include <switch.h>
#include <sys/socket.h>

extern "C" {
#include "../transfer/ftpd.h"
#include "../transfer/mtp_usb.h"
}
#endif

namespace nxrpg::launcher {

namespace {

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
bool usable_root(const std::filesystem::path& sd_root, std::string& root) {
  root = sd_root.lexically_normal().generic_string();
  while (root.ends_with('/')) root.pop_back();
  std::error_code ec;
  return root.starts_with("sdmc:/") && root.find_first_of(":\\", 5) == std::string::npos &&
         std::filesystem::is_directory(std::filesystem::symlink_status(root, ec)) && !ec;
}
#endif

}  // namespace

TransferService::~TransferService() { stop_all(); }

std::string TransferService::start(Mode mode, const std::filesystem::path& sd_root) {
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  if (active(mode)) return {};
  std::string root;
  if (!usable_root(sd_root, root)) return "Cannot open nx.rpg directory on the SD card";

  char error[256]{};
  if (mode == Mode::Mtp) {
    if (!rsl_mtp_start(root.c_str(), error, sizeof error)) return error;
    mtp_active_ = true;
    mtp_speed_.reset(0);
    return {};
  }

  // Borealis initializes the Switch socket and NIFM services before main().
  struct in_addr local{};
  char address[INET_ADDRSTRLEN]{};
  if (R_FAILED(nifmGetCurrentIpAddress(&local.s_addr)) || !local.s_addr ||
      !inet_ntop(AF_INET, &local, address, sizeof address)) {
    return "No local IPv4 address";
  }
  if (!rsl_ftpd_start(root.c_str(), 5000, local.s_addr, error, sizeof error)) return error;
  ftp_address_ = std::string("ftp://") + address + ":5000/";
  ftp_active_ = true;
  ftp_speed_.reset(0);
  return {};
#else
  (void)mode;
  (void)sd_root;
  return "Available on Nintendo Switch only";
#endif
}

void TransferService::stop(Mode mode) {
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  if (mode == Mode::Mtp && mtp_active_) {
    rsl_mtp_stop();
    mtp_active_ = false;
    mtp_speed_.reset(0);
  } else if (mode == Mode::Ftp && ftp_active_) {
    rsl_ftpd_stop();
    ftp_active_ = false;
    ftp_address_.clear();
    ftp_speed_.reset(0);
  }
#else
  (void)mode;
#endif
}

void TransferService::stop_all() {
  stop(Mode::Mtp);
  stop(Mode::Ftp);
}

bool TransferService::active(Mode mode) const {
  return mode == Mode::Mtp ? mtp_active_ : ftp_active_;
}

TransferService::Status TransferService::status() {
  Status out;
  out.ftp_address = ftp_address_;
#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  if (mtp_active_) {
    rsl_ftpd_status st{};
    rsl_mtp_get(&st);
    out.mtp.active = true;
    out.mtp.running = st.running;
    out.mtp.connected = st.connected;
    out.mtp.peer = st.client;
    out.mtp.file = st.file;
    out.mtp.bytes = st.bytes;
    out.mtp.files = st.files;
    out.mtp.bytes_per_second = mtp_speed_.sample(st.bytes + st.sent);
  } else {
    mtp_speed_.reset(0);
  }
  if (ftp_active_) {
    rsl_ftpd_status st{};
    rsl_ftpd_get(&st);
    out.ftp.active = true;
    out.ftp.running = st.running;
    out.ftp.connected = st.connected;
    out.ftp.peer = st.client;
    out.ftp.file = st.file;
    out.ftp.error = st.error;
    out.ftp.bytes = st.bytes;
    out.ftp.files = st.files;
    out.ftp.bytes_per_second = ftp_speed_.sample(st.bytes + st.sent);
  } else {
    ftp_speed_.reset(0);
  }
#endif
  return out;
}

void TransferService::SpeedSampler::reset(std::uint64_t bytes) {
  last_bytes = bytes;
  last_at = std::chrono::steady_clock::now();
  bytes_per_second = 0.0;
}

double TransferService::SpeedSampler::sample(std::uint64_t bytes) {
  const auto now = std::chrono::steady_clock::now();
  if (bytes < last_bytes) {  // the backend restarted its counters
    reset(bytes);
    return 0.0;
  }
  const double seconds = std::chrono::duration<double>(now - last_at).count();
  if (seconds < 0.2) return bytes_per_second;
  if (seconds > 2.5) {
    // Re-anchor after the transfer tab was away so the next tick shows live
    // throughput instead of a stale multi-second average.
    reset(bytes);
    return 0.0;
  }
  bytes_per_second = static_cast<double>(bytes - last_bytes) / seconds;
  last_bytes = bytes;
  last_at = now;
  return bytes_per_second;
}

}  // namespace nxrpg::launcher
