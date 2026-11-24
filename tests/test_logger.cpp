// Host-side unit tests for nxrpg::Logger (common/src/logger.cpp).
#include "nxrpg/logger.hpp"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include "test_common.hpp"

namespace fs = std::filesystem;

using nxrpg::Logger;
using nxrpg::LogLevel;

namespace {

std::string read_all(const fs::path& p) {
  std::ifstream ifs(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  // --- Timestamp formatting ---
  {
    // 2026-01-02 03:04:05 UTC = 1767325445
    const std::string ts = nxrpg::format_log_timestamp(1767325445ULL);
    NXRPG_CHECK(ts == "2026-01-02 03:04:05");
    NXRPG_CHECK(nxrpg::format_log_timestamp(0ULL) == "0000-00-00 00:00:00");
  }

  // --- File open, append, level filtering ---
  {
    fs::path dir = fs::temp_directory_path() / "nxrpg_logger_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path log_file = dir / "runtime/launcher.log";

    auto& logger = Logger::instance();
    NXRPG_CHECK(logger.open_file(log_file));
    NXRPG_CHECK(logger.file_open());
    NXRPG_CHECK(fs::is_regular_file(log_file));

    logger.set_min_level(LogLevel::Info);  // drops Debug
    NXLOG_DEBUG("test", "dropped debug line");
    NXLOG_INFO("test", "hello %s (%d)", "world", 42);
    NXLOG_WARN("test", "careful");
    NXLOG_ERROR("test", "boom");
    logger.close_file();

    const std::string content = read_all(log_file);
    NXRPG_CHECK(content.find("[INFO][test][") != std::string::npos);
    NXRPG_CHECK(content.find("hello world (42)") != std::string::npos);
    NXRPG_CHECK(content.find("[WARN][test][") != std::string::npos);
    NXRPG_CHECK(content.find("[ERROR][test][") != std::string::npos);
    NXRPG_CHECK(content.find("dropped debug line") == std::string::npos);
    NXRPG_CHECK(content.find("session start ---") != std::string::npos);

    // --- Reopen appends, does not truncate ---
    NXRPG_CHECK(logger.open_file(log_file));
    NXLOG_INFO("test", "second session");
    logger.close_file();
    const std::string appended = read_all(log_file);
    NXRPG_CHECK(appended.find("hello world (42)") != std::string::npos);
    NXRPG_CHECK(appended.find("second session") != std::string::npos);
    NXRPG_CHECK(fs::remove_all(dir) >= 1);
  }

  // --- Disabled logger writes nothing (no crash with no file) ---
  {
    auto& logger = Logger::instance();
    NXLOG_INFO("test", "no file open yet — must not crash");
    NXRPG_CHECK(!logger.file_open());
    NXRPG_CHECK(logger.log_path().empty());
  }

  std::puts("All Logger tests passed.");
  return 0;
}
