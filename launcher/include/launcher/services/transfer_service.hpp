#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace nxrpg::launcher {

/// Owns the optional USB MTP / LAN FTP file-transfer backends. Both are
/// independent switches: either, both or neither can run, and they keep
/// running while the user browses other tabs (they only stop when switched
/// off, before a game launches, or when the launcher exits).
class TransferService {
 public:
  enum class Mode { Ftp, Mtp };

  struct ServiceStatus {
    bool active = false;      // switched on from the transfer page
    bool running = false;     // its thread is still serving
    bool connected = false;   // a client is attached right now
    std::string peer;         // "USB" or the client's IPv4 address
    std::string file;         // last file sent or received
    std::string error;        // why a service stopped by itself
    std::uint64_t bytes = 0;  // received since it started
    unsigned files = 0;       // files received since it started
    double bytes_per_second = 0.0;
  };

  struct Status {
    ServiceStatus mtp;
    ServiceStatus ftp;
    std::string ftp_address;  // ftp://<lan-ip>:5000/ while FTP is active
  };

  TransferService() = default;
  ~TransferService();
  TransferService(const TransferService&) = delete;
  TransferService& operator=(const TransferService&) = delete;

  /// Switch one backend on. Empty string on success, otherwise the reason.
  [[nodiscard]] std::string start(Mode mode, const std::filesystem::path& sd_root);
  void stop(Mode mode);
  void stop_all();

  [[nodiscard]] bool active(Mode mode) const;
  [[nodiscard]] bool any_active() const { return mtp_active_ || ftp_active_; }

  /// Current state of both backends plus a fresh speed sample. Call it from
  /// the UI thread; speeds use the time between calls.
  [[nodiscard]] Status status();

 private:
  struct SpeedSampler {
    std::uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_at{};
    double bytes_per_second = 0.0;

    void reset(std::uint64_t bytes);
    double sample(std::uint64_t bytes);
  };

  bool mtp_active_ = false;
  bool ftp_active_ = false;
  std::string ftp_address_;
  SpeedSampler mtp_speed_;
  SpeedSampler ftp_speed_;
};

}  // namespace nxrpg::launcher
