#include "nxrpg/paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace nxrpg::paths {

namespace {

char ascii_lower(unsigned char c) noexcept {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return static_cast<char>(c);
}

bool ascii_iequals(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ascii_lower(static_cast<unsigned char>(a[i])) !=
        ascii_lower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

std::vector<std::string_view> split_rel_segments(std::string_view rel) {
  std::vector<std::string_view> parts;
  std::size_t i = 0;
  while (i < rel.size()) {
    while (i < rel.size() && (rel[i] == '/' || rel[i] == '\\')) {
      ++i;
    }
    if (i >= rel.size()) break;
    std::size_t j = i;
    while (j < rel.size() && rel[j] != '/' && rel[j] != '\\') {
      ++j;
    }
    std::string_view seg = rel.substr(i, j - i);
    if (seg == ".") {
      // Skip no-op self segment
    } else {
      parts.push_back(seg);
    }
    i = j;
  }
  return parts;
}

}  // namespace

std::string core_nro_path(CoreKind core) {
  switch (core) {
    case CoreKind::EasyRpg:
      return std::string(kCoresDir) + "/easyrpg.nro";
    case CoreKind::NxjsMvz:
      return std::string(kCoresDir) + "/nxjs-mvz.nro";
    case CoreKind::MkxpZ:
      return std::string(kCoresDir) + "/mkxp-z.nro";
    case CoreKind::Unknown:
    default:
      return "";
  }
}

std::string default_rtp_path(EngineKind engine) {
  switch (engine) {
    case EngineKind::Rm2k:
      return std::string(kRtpDir) + "/rm2000";
    case EngineKind::Rm2k3:
      return std::string(kRtpDir) + "/rm2003";
    case EngineKind::Rmxp:
      return std::string(kRtpDir) + "/rmxp";
    case EngineKind::Rmvx:
      return std::string(kRtpDir) + "/rmvx";
    case EngineKind::Rmvxa:
      return std::string(kRtpDir) + "/rmvxa";
    case EngineKind::Rmmv:
    case EngineKind::Rmmz:
    case EngineKind::Unknown:
    default:
      return "";
  }
}

std::optional<std::filesystem::path> resolve_case_insensitive(
    const std::filesystem::path& base_dir,
    std::string_view relative_path) {
  namespace fs = std::filesystem;
  std::error_code ec;

  if (!fs::exists(base_dir, ec) || !fs::is_directory(base_dir, ec)) {
    return std::nullopt;
  }

  auto segments = split_rel_segments(relative_path);
  if (segments.empty()) {
    return std::nullopt;
  }

  fs::path current = base_dir;
  for (std::size_t idx = 0; idx < segments.size(); ++idx) {
    std::string_view seg = segments[idx];
    if (seg == "..") {
      return std::nullopt;
    }

    if (!fs::is_directory(current, ec)) {
      return std::nullopt;
    }

    std::optional<fs::path> exact_match;
    std::optional<fs::path> ci_match;

    for (fs::directory_iterator it(current, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
      std::string entry_name = it->path().filename().string();
      if (entry_name == seg) {
        exact_match = it->path();
        break;
      }
      if (!ci_match && ascii_iequals(entry_name, seg)) {
        ci_match = it->path();
      }
    }

    if (ec) {
      return std::nullopt;
    }

    if (exact_match) {
      current = std::move(*exact_match);
    } else if (ci_match) {
      current = std::move(*ci_match);
    } else {
      return std::nullopt;
    }
  }

  if (!fs::exists(current, ec)) {
    return std::nullopt;
  }
  return current;
}

bool write_file_atomic(
    const std::filesystem::path& target_path,
    std::string_view bytes) {
  namespace fs = std::filesystem;
  std::error_code ec;

  if (target_path.empty()) {
    return false;
  }

  fs::path parent = target_path.parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent, ec);
    if (ec) {
      return false;
    }
  }

  fs::path tmp_path = target_path;
  tmp_path += ".tmp";

#ifdef _WIN32
  std::FILE* fp = _wfopen(tmp_path.c_str(), L"wb");
#else
  std::FILE* fp = std::fopen(tmp_path.c_str(), "wb");
#endif
  if (!fp) {
    return false;
  }

  bool ok = true;
  if (!bytes.empty()) {
    std::size_t written = std::fwrite(bytes.data(), 1, bytes.size(), fp);
    if (written != bytes.size()) {
      ok = false;
    }
  }

  if (ok && std::fflush(fp) != 0) {
    ok = false;
  }

  if (ok) {
#ifdef _WIN32
    int fd = _fileno(fp);
    if (fd >= 0) {
      // _commit flushes OS buffers to disk on Windows
      if (_commit(fd) != 0) {
        ok = false;
      }
    }
#else
    int fd = fileno(fp);
    if (fd >= 0) {
      if (::fsync(fd) != 0) {
        ok = false;
      }
    }
#endif
  }

  if (std::fclose(fp) != 0) {
    ok = false;
  }

  if (!ok) {
    fs::remove(tmp_path, ec);
    return false;
  }

  // Horizon fsdev rename() does NOT clobber an existing target (non-POSIX),
  // so a stale target must be removed first. This made every re-write of
  // launch.json / global.json / library.json fail after the first write.
  fs::remove(target_path, ec);
  ec.clear();
  fs::rename(tmp_path, target_path, ec);
  if (ec) {
    fs::remove(tmp_path, ec);
    return false;
  }

  return true;
}

}  // namespace nxrpg::paths
