#include "nxrpg/logger.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <utility>

#if defined(_MSC_VER)
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(_WIN32)
// MinGW and MSVC both expose gmtime_s in <time.h> on UCRT/MSVCRT.
#define NXRPG_HAVE_GMTIME_S 1
#define NXRPG_ON_WINDOWS 1
#else
#define NXRPG_HAVE_GMTIME_S 0
#define NXRPG_ON_WINDOWS 0
#endif

namespace nxrpg {

namespace {

const char* level_tag(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "LOG";
}

std::uint64_t now_epoch_seconds() {
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  return secs > 0 ? static_cast<std::uint64_t>(secs) : 0;
}

}  // namespace

std::string format_log_timestamp(std::uint64_t epoch_seconds) {
  if (epoch_seconds == 0) {
    return "0000-00-00 00:00:00";
  }
  std::time_t tt = static_cast<std::time_t>(epoch_seconds);
  std::tm tm_buf {};
#if NXRPG_HAVE_GMTIME_S
  if (gmtime_s(&tm_buf, &tt) != 0) {
    return std::to_string(epoch_seconds);
  }
#else
  if (gmtime_r(&tt, &tm_buf) == nullptr) {
    return std::to_string(epoch_seconds);
  }
#endif
  char buf[32] = {};
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
  return buf;
}

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

Logger::~Logger() {
  close_file();
}

bool Logger::open_file(const std::filesystem::path& log_path) {
  close_file();

  std::error_code ec;
  if (log_path.has_parent_path()) {
    std::filesystem::create_directories(log_path.parent_path(), ec);
  }

  FILE* fp = std::fopen(log_path.string().c_str(), "ab");
  if (!fp) {
    log_path_.clear();
    return false;
  }
  // Append-mode line buffering: each write() flushes explicitly below.
  std::setvbuf(fp, nullptr, _IOFBF, 4096);
  log_path_ = log_path;
  file_ = fp;

  write(LogLevel::Info, "log", "--- nx.rpg launcher session start ---");
  return true;
}

void Logger::close_file() {
  if (file_) {
    std::fclose(static_cast<FILE*>(file_));
    file_ = nullptr;
    log_path_.clear();
  }
}

void Logger::set_stderr_mirror(bool enabled) {
  stderr_mirror_ = enabled;
}

void Logger::set_min_level(LogLevel level) {
  min_level_ = level;
}

void Logger::write(LogLevel level, std::string_view tag, std::string_view message) {
  if (level < min_level_) {
    return;
  }

  std::string line;
  line.reserve(8 + 8 + tag.size() + message.size() + 4);
  line += '[';
  line += level_tag(level);
  line += "][";
  line += tag;
  line += "][";
  line += format_log_timestamp(now_epoch_seconds());
  line += "] ";
  line += message;
  line += '\n';

  if (file_) {
    std::fwrite(line.data(), 1, line.size(), static_cast<FILE*>(file_));
    std::fflush(static_cast<FILE*>(file_));
    // fsync/_commit keeps the line durable across crashes. Failures are
    // deliberately ignored (logging must not throw).
    const int fd = ::fileno(static_cast<FILE*>(file_));
#if NXRPG_ON_WINDOWS
    (void)::_commit(fd);
#else
    (void)::fsync(fd);
#endif
  }

  if (stderr_mirror_) {
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
  }
}

void log_line(LogLevel level, std::string_view tag, const char* fmt, ...) {
  if (!fmt) {
    return;
  }
  char stack_buf[512];
  va_list args;
  va_start(args, fmt);
  int n = std::vsnprintf(stack_buf, sizeof(stack_buf), fmt, args);
  va_end(args);
  if (n < 0) {
    return;
  }
  std::string message;
  if (static_cast<std::size_t>(n) < sizeof(stack_buf)) {
    message.assign(stack_buf, static_cast<std::size_t>(n));
  } else {
    message.resize(static_cast<std::size_t>(n));
    va_start(args, fmt);
    std::vsnprintf(message.data(), message.size() + 1, fmt, args);
    va_end(args);
  }
  Logger::instance().write(level, tag, message);
}

}  // namespace nxrpg
