#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nxrpg {

/// Severity levels for `Logger`, mirrored to both the log file and stderr/nxlink.
enum class LogLevel {
  Debug = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
};

/// Format `epoch_seconds` as `YYYY-MM-DD HH:MM:SS` (UTC). Falls back to the raw
/// epoch number when the platform cannot convert (e.g. no RTC configured yet).
[[nodiscard]] std::string format_log_timestamp(std::uint64_t epoch_seconds);

/// Central launcher logger: appends timestamped, level-tagged lines to a
/// configured log file (default `runtime/launcher.log`) and mirrors every line
/// to stderr. On Switch, stderr is redirected to nxlink when the socket is up.
///
/// All writes are crash-safe appends (O_APPEND semantics via fopen("a") plus
/// per-line fflush/fsync); the file itself never uses the write-temp-rename
/// pattern because concurrent core processes append to it as well.
///
/// Not thread-safe by design: the launcher UI and services run on one thread,
/// and each core process owns its own instance.
class Logger {
 public:
  /// Returns the process-wide singleton (created lazily, disabled until
  /// `open_file` is called or `NXRPG_LOG_DISABLE` logic says otherwise).
  [[nodiscard]] static Logger& instance();

  /// Opens (creates or appends to) `log_path`, writes a session banner, and
  /// enables file output. Safe to call again to switch files.
  bool open_file(const std::filesystem::path& log_path);

  /// Disables file output (lines still mirror to stderr).
  void close_file();

  /// Enables/disables stderr mirroring (off by default on Switch until nxlink).
  void set_stderr_mirror(bool enabled);

  /// Minimum level that is written anywhere. Messages below it are dropped.
  void set_min_level(LogLevel level);

  [[nodiscard]] const std::filesystem::path& log_path() const { return log_path_; }
  [[nodiscard]] bool file_open() const { return file_ != nullptr; }

  /// Appends one formatted line: `[LEVEL][tag] message`. Never throws.
  void write(LogLevel level, std::string_view tag, std::string_view message);

 private:
  Logger() = default;
  ~Logger();

  std::filesystem::path log_path_;
  void* file_ = nullptr;  // FILE* kept as void* to keep this header lightweight
  bool stderr_mirror_ = true;
  LogLevel min_level_ = LogLevel::Debug;
};

/// Convenience formatted free functions used by the NXLOG_* macros.
/// `fmt` follows printf conventions (%s/%d/...); returns the formatted string.
void log_line(LogLevel level, std::string_view tag, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

}  // namespace nxrpg

#define NXLOG_DEBUG(tag, ...) ::nxrpg::log_line(::nxrpg::LogLevel::Debug, tag, __VA_ARGS__)
#define NXLOG_INFO(tag, ...) ::nxrpg::log_line(::nxrpg::LogLevel::Info, tag, __VA_ARGS__)
#define NXLOG_WARN(tag, ...) ::nxrpg::log_line(::nxrpg::LogLevel::Warning, tag, __VA_ARGS__)
#define NXLOG_ERROR(tag, ...) ::nxrpg::log_line(::nxrpg::LogLevel::Error, tag, __VA_ARGS__)
