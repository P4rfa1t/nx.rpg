#include "launcher/services/environment_doctor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include "launcher/services/config_service.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg::launcher {

namespace {

namespace fs = std::filesystem;

std::string to_lower_ascii(std::string_view sv) {
  std::string out;
  out.reserve(sv.size());
  for (unsigned char c : sv) {
    out.push_back(static_cast<char>(std::tolower(c)));
  }
  return out;
}

bool dir_has_non_placeholder_files(const fs::path& dir) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return false;
  for (fs::directory_iterator it(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    std::string fname = it->path().filename().string();
    if (fname == ".gitkeep" || fname == "." || fname == "..") continue;
    return true;
  }
  return false;
}

bool has_font_file(const fs::path& fonts_dir) {
  std::error_code ec;
  if (!fs::is_directory(fonts_dir, ec)) return false;
  for (fs::directory_iterator it(fonts_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    std::string ext = to_lower_ascii(it->path().extension().string());
    if (ext == ".ttf" || ext == ".ttc" || ext == ".otf") {
      return true;
    }
  }
  return false;
}

std::string extract_bracket_tag(std::string_view line, std::string_view key) {
  std::string needle = "[" + std::string(key) + "=";
  std::size_t pos = line.find(needle);
  if (pos == std::string_view::npos) return "";
  std::size_t start = pos + needle.size();
  std::size_t end = line.find(']', start);
  if (end == std::string_view::npos) return "";
  return std::string(line.substr(start, end - start));
}

}  // namespace

std::vector<DiagnosticItem> EnvironmentDoctor::run_all_checks(MemoryExecutionMode memory_mode) {
  return run_all_checks(memory_mode, fs::path(paths::kSdRoot));
}

std::vector<DiagnosticItem> EnvironmentDoctor::run_all_checks(
    MemoryExecutionMode memory_mode,
    const fs::path& sd_root) {
  std::vector<DiagnosticItem> items;
  std::error_code ec;

  // 1. Memory Mode check
  if (memory_mode == MemoryExecutionMode::ApplicationFullRam) {
    items.push_back({DiagnosticItem::Severity::Ok,
                     "Memory Mode",
                     "Application Mode (~3.2+ GiB RAM, full V8 JIT/WASM enabled)",
                     ""});
  } else if (memory_mode == MemoryExecutionMode::AppletRestrictedRam) {
    items.push_back({DiagnosticItem::Severity::Warning,
                     "Memory Mode",
                     "Applet Mode (~440 MiB RAM, V8 JIT disabled)",
                     "Hold R while launching any Switch title to enter Title Takeover (Full RAM)."});
  } else {
    items.push_back({DiagnosticItem::Severity::Ok,
                     "Memory Mode",
                     "Host Desktop Preview Mode",
                     ""});
  }

  // 2. Core NRO binaries check
  struct CoreCheck {
    const char* name;
    const char* filename;
    bool required_for_phase1;
  };
  static constexpr std::array<CoreCheck, 3> kCores = {{
      {"Core: easyrpg", "easyrpg.nro", true},
      {"Core: nxjs-mvz", "nxjs-mvz.nro", false},
      {"Core: mkxp-z", "mkxp-z.nro", false},
  }};
  for (const auto& c : kCores) {
    fs::path nro = sd_root / "cores" / c.filename;
    if (fs::is_regular_file(nro, ec)) {
      items.push_back({DiagnosticItem::Severity::Ok, c.name, "Present: " + nro.generic_string(), ""});
    } else {
      items.push_back({c.required_for_phase1 ? DiagnosticItem::Severity::Error : DiagnosticItem::Severity::Warning,
                       c.name,
                       "Not found: " + nro.generic_string(),
                       "Copy built " + std::string(c.filename) + " to sdmc:/switch/nx.rpg/cores/."});
    }
  }

  // 3. Shared RTP pools (5 legacy generations)
  static constexpr std::array<const char*, 5> kRtpDirs = {
      "rm2000", "rm2003", "rmxp", "rmvx", "rmvxa",
  };
  for (const char* rtp_name : kRtpDirs) {
    fs::path rtp_dir = sd_root / "rtp" / rtp_name;
    if (dir_has_non_placeholder_files(rtp_dir)) {
      items.push_back({DiagnosticItem::Severity::Ok,
                       std::string("RTP: ") + rtp_name,
                       "Shared assets available",
                       ""});
    } else {
      items.push_back({DiagnosticItem::Severity::Warning,
                       std::string("RTP: ") + rtp_name,
                       "Directory empty (games with FullPackageFlag=1 still work)",
                       "Optional: place official " + std::string(rtp_name) + " RTP files here for games without bundled RTP."});
    }
  }

  // 4. CJK Fallback Font check
  GlobalConfig gcfg = ConfigService::load_global(sd_root);
  fs::path configured_font(gcfg.fallback_font_path);
  bool font_ok = (!gcfg.fallback_font_path.empty() && fs::is_regular_file(configured_font, ec)) ||
                 has_font_file(sd_root / "fonts");
  if (font_ok) {
    items.push_back({DiagnosticItem::Severity::Ok,
                     "Font: CJK",
                     "Fallback font available",
                     ""});
  } else {
    items.push_back({DiagnosticItem::Severity::Warning,
                     "Font: CJK",
                     "No CJK fallback font found under fonts/",
                     "Place NotoSansCJK-Regular.ttc in sdmc:/switch/nx.rpg/fonts/ to avoid missing CJK glyphs."});
  }

  // 5. Orphan `.tmp` cleanup & SD Filesystem advice
  std::size_t cleaned = cleanup_orphaned_tmp_files(sd_root);
  items.push_back({DiagnosticItem::Severity::Ok,
                   "SD Filesystem",
                   cleaned > 0 ? ("Cleaned " + std::to_string(cleaned) + " orphaned .tmp file(s); FAT32 recommended")
                               : "No orphaned .tmp files; FAT32 recommended over exFAT",
                   ""});

  // 6. Per-game run log inspection (logs/games/<game_id>.log, newest first)
  fs::path game_logs_dir = sd_root / "logs/games";
  auto game_logs = list_game_logs(game_logs_dir);
  bool any_fatal = false;
  for (const auto& [game_id, log_path] : game_logs) {
    LastRunLogSummary log = inspect_last_run_log(log_path);
    if (log.has_fatal) {
      any_fatal = true;
      items.push_back({DiagnosticItem::Severity::Error,
                       "Game Log: " + game_id,
                       "Fatal error in last run [" + log.fatal_core + "/" + log.fatal_code + "]: " + log.fatal_message,
                       "Inspect " + fs::path(log_path).generic_string() + " in the Diagnostics view."});
    }
  }
  if (!game_logs.empty() && !any_fatal) {
    items.push_back({DiagnosticItem::Severity::Ok,
                     "Game Logs",
                     std::to_string(game_logs.size()) + " per-game log(s), none with [nx.rpg][FATAL]",
                     ""});
  }

  return items;
}

std::size_t EnvironmentDoctor::count_orphaned_tmp_files(const fs::path& sd_root) {
  std::size_t count = 0;
  std::error_code ec;
  for (const char* sub : {"config", "cache", "runtime", "saves", "logs"}) {
    fs::path dir = sd_root / sub;
    if (!fs::is_directory(dir, ec)) continue;
    for (fs::recursive_directory_iterator it(dir, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
      if (!it->is_regular_file(ec)) continue;
      if (it->path().extension() == ".tmp") {
        ++count;
      }
    }
  }
  return count;
}

std::size_t EnvironmentDoctor::cleanup_orphaned_tmp_files(const fs::path& sd_root) {
  std::size_t removed = 0;
  std::error_code ec;
  for (const char* sub : {"config", "cache", "runtime", "saves", "logs"}) {
    fs::path dir = sd_root / sub;
    if (!fs::is_directory(dir, ec)) continue;
    std::vector<fs::path> to_delete;
    for (fs::recursive_directory_iterator it(dir, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
      if (!it->is_regular_file(ec)) continue;
      if (it->path().extension() == ".tmp") {
        to_delete.push_back(it->path());
      }
    }
    for (const auto& p : to_delete) {
      if (fs::remove(p, ec)) {
        ++removed;
      }
    }
  }
  return removed;
}

std::optional<fs::path> EnvironmentDoctor::most_recent_game_log(const fs::path& game_logs_dir) {
  auto logs = list_game_logs(game_logs_dir);
  if (logs.empty()) return std::nullopt;
  return logs.front().second;
}

std::vector<std::pair<std::string, fs::path>> EnvironmentDoctor::list_game_logs(
    const fs::path& game_logs_dir) {
  std::vector<std::pair<std::string, fs::path>> out;
  std::error_code ec;
  if (!fs::is_directory(game_logs_dir, ec)) return out;

  struct Entry {
    std::string game_id;
    fs::path path;
    fs::file_time_type mtime;
  };
  std::vector<Entry> entries;
  for (fs::directory_iterator it(game_logs_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    fs::path p = it->path();
    std::string ext = to_lower_ascii(p.extension().string());
    if (ext != ".log") continue;
    std::string stem = p.stem().string();
    if (stem.empty()) continue;
    entries.push_back({std::move(stem), p, it->last_write_time(ec)});
  }
  std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.mtime > b.mtime; });
  out.reserve(entries.size());
  for (auto& e : entries) {
    out.push_back({std::move(e.game_id), std::move(e.path)});
  }
  return out;
}

LastRunLogSummary EnvironmentDoctor::inspect_last_run_log(
    const fs::path& log_path,
    std::size_t max_tail_lines) {
  LastRunLogSummary summary;
  std::error_code ec;
  if (!fs::is_regular_file(log_path, ec)) return summary;

  std::ifstream ifs(log_path, std::ios::binary);
  if (!ifs) return summary;

  summary.exists = true;
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(ifs, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.rfind("[nx.rpg][FATAL]", 0) == 0) {
      summary.has_fatal = true;
      summary.fatal_line = line;
      summary.fatal_core = extract_bracket_tag(line, "core");
      summary.fatal_game = extract_bracket_tag(line, "game");
      summary.fatal_code = extract_bracket_tag(line, "code");
      std::size_t last_bracket = line.rfind(']');
      if (last_bracket != std::string::npos && last_bracket + 1 < line.size()) {
        std::size_t msg_start = last_bracket + 1;
        while (msg_start < line.size() && line[msg_start] == ' ') ++msg_start;
        summary.fatal_message = line.substr(msg_start);
      }
    }
    lines.push_back(std::move(line));
  }

  if (lines.size() > max_tail_lines) {
    summary.tail_lines.assign(
        lines.end() - static_cast<std::ptrdiff_t>(max_tail_lines),
        lines.end());
  } else {
    summary.tail_lines = std::move(lines);
  }

  return summary;
}

}  // namespace nxrpg::launcher
