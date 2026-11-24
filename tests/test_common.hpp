#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace nxrpg::test {

inline int g_failures = 0;

inline void check_impl(bool cond, const char* expr, const char* file, int line, std::string_view msg = {}) {
  if (!cond) {
    ++g_failures;
    std::cerr << "[FAIL] " << file << ":" << line << ": (" << expr << ")";
    if (!msg.empty()) {
      std::cerr << " - " << msg;
    }
    std::cerr << "\n";
  }
}

#define NXRPG_CHECK(expr) ::nxrpg::test::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define NXRPG_CHECK_MSG(expr, msg) ::nxrpg::test::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))

class TempDir {
 public:
  explicit TempDir(std::string_view name) {
    auto base = std::filesystem::temp_directory_path();
    path_ = base / ("nxrpg_test_" + std::string(name));
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    std::filesystem::create_directories(path_, ec);
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

inline void write_bytes(const std::filesystem::path& p, std::string_view bytes) {
  std::error_code ec;
  if (p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path(), ec);
  }
  std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
  ofs.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline int finish() {
  if (g_failures > 0) {
    std::cerr << g_failures << " check(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All checks passed.\n";
  return EXIT_SUCCESS;
}

}  // namespace nxrpg::test
