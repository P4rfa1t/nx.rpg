#include "launcher/services/library_service.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unordered_map>

#include "launcher/services/config_service.hpp"
#include "nxrpg/cover_extractor.hpp"
#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg::launcher {

namespace {

namespace fs = std::filesystem;

std::string normalize_path_str(const fs::path& p) {
  std::string s = p.generic_string();
  while (s.size() > 1 && s.back() == '/') {
    if (s.size() >= 2 && s[s.size() - 2] == ':') break;
    s.pop_back();
  }
  return s;
}

std::uint64_t read_dir_mtime(const fs::path& p) noexcept {
  std::error_code ec;
  auto ftime = fs::last_write_time(p, ec);
  if (ec) return 0;
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
  auto stime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(stime.time_since_epoch()).count();
  return secs > 0 ? static_cast<std::uint64_t>(secs) : 0;
#else
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
  return secs > 0 ? static_cast<std::uint64_t>(secs) : 0;
#endif
}

std::uint64_t current_epoch_seconds() noexcept {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(now).count();
  return secs > 0 ? static_cast<std::uint64_t>(secs) : 0;
}

bool is_ignored_dir(std::string_view name) noexcept {
  return name.empty() || name[0] == '.' || name == "__MACOSX";
}

void incremental_walk(
    const fs::path& dir,
    const fs::path& games_root,
    std::uint32_t depth,
    std::uint32_t max_depth,
    const ScanOptions& scan_opts,
    const std::unordered_map<std::string, LibraryEntry>& cached_by_path,
    const std::unordered_map<std::string, LibraryEntry>& cached_by_id,
    std::vector<LibraryEntry>& out) {
  std::error_code ec;
  std::vector<fs::path> subdirs;
  for (fs::directory_iterator it(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_directory(ec)) continue;
    std::string fname = it->path().filename().string();
    if (is_ignored_dir(fname)) continue;
    subdirs.push_back(it->path());
  }
  std::sort(subdirs.begin(), subdirs.end());

  for (const auto& sub : subdirs) {
    std::string norm_sub = normalize_path_str(sub);
    auto hit = cached_by_path.find(norm_sub);
    if (hit != cached_by_path.end()) {
      std::uint64_t cur_mtime = read_dir_mtime(sub);
      if (cur_mtime == hit->second.game.dir_mtime) {
        out.push_back(hit->second);
        continue;
      }
    }

    // Inspect directory directly (only treat as game if `sub` itself is the game root,
    // not via 1-level unwrap, so nested structures align with `max_depth` scan).
    if (auto det = EngineDetector::inspect_directory(sub, scan_opts)) {
      if (det->game_path == norm_sub) {
        std::string rel = fs::relative(sub, games_root, ec).generic_string();
        if (!ec && !rel.empty()) {
          det->id = EngineDetector::compute_game_id(rel);
        }
        LibraryEntry entry;
        entry.game = std::move(*det);
        if (auto prev = cached_by_id.find(entry.game.id); prev != cached_by_id.end()) {
          entry.cover_cache_path = prev->second.cover_cache_path;
          entry.favorite = prev->second.favorite;
          entry.last_played_at = prev->second.last_played_at;
          entry.play_time_seconds = prev->second.play_time_seconds;
        }
        if (entry.cover_cache_path.empty() && !entry.game.cover_source_path.empty()) {
          entry.cover_cache_path = entry.game.cover_source_path;
        }
        out.push_back(std::move(entry));
        continue;
      }
    }

    if (depth < max_depth) {
      incremental_walk(
          sub, games_root, depth + 1, max_depth, scan_opts, cached_by_path, cached_by_id, out);
    }
  }
}

}  // namespace

LibraryService::LibraryService(fs::path sd_root) : sd_root_(std::move(sd_root)) {}

std::vector<LibraryEntry> LibraryService::load_cached_entries() const {
  std::vector<LibraryEntry> entries;
  const fs::path cache_path = sd_root_ / "cache/library.json";
  std::error_code ec;
  if (!fs::is_regular_file(cache_path, ec)) return entries;

  std::ifstream ifs(cache_path, std::ios::binary);
  if (!ifs) return entries;
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  auto root_opt = JsonValue::parse(content);
  if (!root_opt || !root_opt->is_object()) return entries;

  const auto* ver = root_opt->find("schemaVersion");
  if (!ver || ver->as_int64() != 1) return entries;
  const auto* arr_v = root_opt->find("entries");
  const auto* arr = arr_v ? arr_v->as_array() : nullptr;
  if (!arr) return entries;

  for (const auto& item : *arr) {
    if (!item.is_object()) continue;
    const auto* id = item.find("id") ? item.find("id")->as_string() : nullptr;
    const auto* title = item.find("title") ? item.find("title")->as_string() : nullptr;
    const auto* eng_s = item.find("engine") ? item.find("engine")->as_string() : nullptr;
    const auto* core_s = item.find("core") ? item.find("core")->as_string() : nullptr;
    const auto* gp = item.find("gamePath") ? item.find("gamePath")->as_string() : nullptr;
    const auto* cr = item.find("contentRoot") ? item.find("contentRoot")->as_string() : nullptr;
    const auto* ex = item.find("execName") ? item.find("execName")->as_string() : nullptr;
    auto mtime = item.find("dirMtime") ? item.find("dirMtime")->as_int64() : std::nullopt;
    auto enc_arch = item.find("hasEncryptedArchive") ? item.find("hasEncryptedArchive")->as_bool() : std::nullopt;
    if (!id || !title || !eng_s || !core_s || !gp || !cr || !ex || !mtime || !enc_arch) continue;

    auto eng = parse_engine_kind(*eng_s);
    auto core = parse_core_kind(*core_s);
    if (!eng || *eng == EngineKind::Unknown || !core || *core == CoreKind::Unknown) continue;

    LibraryEntry entry;
    entry.game.id = *id;
    entry.game.title = *title;
    entry.game.engine = *eng;
    entry.game.core = *core;
    entry.game.game_path = *gp;
    entry.game.content_root = *cr;
    entry.game.exec_name = *ex;
    entry.game.dir_mtime = *mtime >= 0 ? static_cast<std::uint64_t>(*mtime) : 0;
    entry.game.has_encrypted_archive = *enc_arch;

    if (const auto* enc_s = item.find("detectedEncoding"); enc_s && enc_s->as_string()) {
      if (auto dec = parse_text_encoding(*enc_s->as_string())) {
        entry.game.detected_encoding = *dec;
      }
    }
    if (const auto* rtp_v = item.find("needsRtp"); rtp_v && rtp_v->as_bool()) {
      entry.game.needs_rtp = *rtp_v->as_bool();
    }
    if (const auto* cov_v = item.find("coverCachePath"); cov_v && cov_v->as_string()) {
      entry.cover_cache_path = *cov_v->as_string();
    }
    if (const auto* fav_v = item.find("favorite"); fav_v && fav_v->as_bool()) {
      entry.favorite = *fav_v->as_bool();
    }
    if (const auto* lp_v = item.find("lastPlayedAt"); lp_v && lp_v->as_int64()) {
      entry.last_played_at = *lp_v->as_int64() >= 0 ? static_cast<std::uint64_t>(*lp_v->as_int64()) : 0;
    }
    if (const auto* pt_v = item.find("playTimeSeconds"); pt_v && pt_v->as_int64()) {
      entry.play_time_seconds = *pt_v->as_int64() >= 0 ? static_cast<std::uint64_t>(*pt_v->as_int64()) : 0;
    }

    entries.push_back(std::move(entry));
  }

  return entries;
}

bool LibraryService::save_entries(const std::vector<LibraryEntry>& entries) const {
  JsonValue root = JsonValue::make_object();
  root.set("schemaVersion", static_cast<std::int64_t>(1));
  root.set("generatedAt", current_epoch_seconds());

  JsonValue arr = JsonValue::make_array();
  for (const auto& entry : entries) {
    JsonValue obj = JsonValue::make_object();
    obj.set("id", entry.game.id);
    obj.set("title", entry.game.title);
    obj.set("engine", to_string(entry.game.engine));
    obj.set("core", to_string(entry.game.core));
    obj.set("gamePath", entry.game.game_path);
    obj.set("contentRoot", entry.game.content_root);
    obj.set("execName", entry.game.exec_name);
    obj.set("detectedEncoding", to_string(entry.game.detected_encoding));
    obj.set("coverCachePath", entry.cover_cache_path);
    obj.set("dirMtime", entry.game.dir_mtime);
    obj.set("hasEncryptedArchive", entry.game.has_encrypted_archive);
    obj.set("needsRtp", entry.game.needs_rtp);
    obj.set("favorite", entry.favorite);
    obj.set("lastPlayedAt", entry.last_played_at);
    obj.set("playTimeSeconds", entry.play_time_seconds);
    arr.push_back(std::move(obj));
  }
  root.set("entries", std::move(arr));

  return paths::write_file_atomic(sd_root_ / "cache/library.json", root.dump(true));
}

std::vector<LibraryEntry> LibraryService::load_or_refresh_entries(bool force_full_rescan) {
  GlobalConfig cfg = ConfigService::load_global(sd_root_);
  ScanOptions scan_opts;
  scan_opts.max_depth = cfg.library.max_scan_depth;
  scan_opts.fallback_encoding = cfg.default_encoding;

  std::vector<LibraryEntry> cached = load_cached_entries();
  std::unordered_map<std::string, LibraryEntry> cached_by_path;
  std::unordered_map<std::string, LibraryEntry> cached_by_id;
  for (const auto& e : cached) {
    cached_by_path[e.game.game_path] = e;
    cached_by_id[e.game.id] = e;
  }

  std::vector<LibraryEntry> fresh;
  const fs::path games_root = sd_root_ / "games";
  std::error_code ec;

  if (fs::exists(games_root, ec) && fs::is_directory(games_root, ec)) {
    if (force_full_rescan || cached.empty()) {
      auto detected = EngineDetector::scan_library(games_root, scan_opts);
      for (auto& g : detected) {
        LibraryEntry entry;
        entry.game = std::move(g);
        if (auto it = cached_by_id.find(entry.game.id); it != cached_by_id.end()) {
          entry.cover_cache_path = it->second.cover_cache_path;
          entry.favorite = it->second.favorite;
          entry.last_played_at = it->second.last_played_at;
          entry.play_time_seconds = it->second.play_time_seconds;
        }
        if (entry.cover_cache_path.empty() && !entry.game.cover_source_path.empty()) {
          entry.cover_cache_path = entry.game.cover_source_path;
        }
        fresh.push_back(std::move(entry));
      }
    } else {
      incremental_walk(
          games_root,
          games_root,
          1,
          std::max<std::uint32_t>(1, scan_opts.max_depth),
          scan_opts,
          cached_by_path,
          cached_by_id,
          fresh);
    }
  }

  // Sort according to global config preference
  if (cfg.library.sort_order == "title") {
    std::sort(fresh.begin(), fresh.end(), [](const LibraryEntry& a, const LibraryEntry& b) {
      return a.game.title < b.game.title;
    });
  } else if (cfg.library.sort_order == "engine") {
    std::sort(fresh.begin(), fresh.end(), [](const LibraryEntry& a, const LibraryEntry& b) {
      if (a.game.engine != b.game.engine) {
        return static_cast<int>(a.game.engine) < static_cast<int>(b.game.engine);
      }
      return a.game.title < b.game.title;
    });
  } else {
    // "recent": favorites / most recently played first, then title
    std::sort(fresh.begin(), fresh.end(), [](const LibraryEntry& a, const LibraryEntry& b) {
      if (a.last_played_at != b.last_played_at) {
        return a.last_played_at > b.last_played_at;
      }
      if (a.favorite != b.favorite) return a.favorite > b.favorite;
      return a.game.title < b.game.title;
    });
  }

  (void)save_entries(fresh);
  return fresh;
}

std::vector<DetectedGame> LibraryService::load_or_refresh(bool force_full_rescan) {
  auto entries = load_or_refresh_entries(force_full_rescan);
  std::vector<DetectedGame> games;
  games.reserve(entries.size());
  for (auto& e : entries) {
    games.push_back(std::move(e.game));
  }
  return games;
}

bool LibraryService::record_play_session(
    std::string_view game_id,
    std::uint64_t played_at_epoch,
    std::uint64_t added_seconds) {
  auto entries = load_cached_entries();
  bool found = false;
  for (auto& e : entries) {
    if (e.game.id == game_id) {
      e.last_played_at = played_at_epoch;
      e.play_time_seconds += added_seconds;
      found = true;
      break;
    }
  }
  if (!found) return false;
  return save_entries(entries);
}

bool LibraryService::toggle_favorite(std::string_view game_id) {
  auto entries = load_cached_entries();
  bool found = false;
  for (auto& e : entries) {
    if (e.game.id == game_id) {
      e.favorite = !e.favorite;
      found = true;
      break;
    }
  }
  if (!found) return false;
  return save_entries(entries);
}

fs::path LibraryService::ensure_cover_thumbnail(const DetectedGame& game) {
  std::error_code ec;
  fs::path extracted = CoverExtractor::extract_to_cache(game, sd_root_ / "cache/covers");
  if (!extracted.empty() && fs::is_regular_file(extracted, ec)) {
    return extracted;
  }
  if (!game.content_root.empty()) {
    fs::path cr(game.content_root);
    for (const char* sub : {"img/titles1", "Graphics/Titles", "Graphics/Title", "Title"}) {
      if (auto dir = paths::resolve_case_insensitive(cr, sub)) {
        for (fs::directory_iterator it(*dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
          if (it->is_regular_file(ec)) {
            return it->path();
          }
        }
      }
    }
  }
  return {};
}

}  // namespace nxrpg::launcher
