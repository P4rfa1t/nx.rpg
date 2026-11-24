#include "nxrpg/engine_detector.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include "nxrpg/encoding.hpp"
#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg {

namespace {

namespace fs = std::filesystem;

char ascii_lower(unsigned char c) noexcept {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return static_cast<char>(c);
}

std::string to_lower_ascii(std::string_view sv) {
  std::string out;
  out.reserve(sv.size());
  for (unsigned char c : sv) {
    out.push_back(ascii_lower(c));
  }
  return out;
}

bool contains_icase(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) return true;
  if (haystack.size() < needle.size()) return false;
  std::string h = to_lower_ascii(haystack);
  std::string n = to_lower_ascii(needle);
  return h.find(n) != std::string::npos;
}

std::string trim_ws(std::string_view sv) {
  std::size_t b = 0;
  while (b < sv.size() && (sv[b] == ' ' || sv[b] == '\t' || sv[b] == '\r' || sv[b] == '\n')) {
    ++b;
  }
  std::size_t e = sv.size();
  while (e > b && (sv[e - 1] == ' ' || sv[e - 1] == '\t' || sv[e - 1] == '\r' || sv[e - 1] == '\n')) {
    --e;
  }
  return std::string(sv.substr(b, e - b));
}

std::string normalize_path_string(const fs::path& p) {
  std::string s = p.generic_string();
  while (s.size() > 1 && s.back() == '/') {
    // Keep "sdmc:/" or "C:/" intact
    if (s.size() >= 2 && s[s.size() - 2] == ':') break;
    s.pop_back();
  }
  return s;
}

std::uint64_t read_dir_mtime_epoch(const fs::path& dir) noexcept {
  std::error_code ec;
  auto ftime = fs::last_write_time(dir, ec);
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

std::optional<std::string> read_file_bytes(const fs::path& p, std::size_t max_bytes = 4u * 1024u * 1024u) {
  std::error_code ec;
  if (!fs::is_regular_file(p, ec)) return std::nullopt;
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) return std::nullopt;

  std::string out;
  char buf[8192];
  while (ifs && out.size() < max_bytes) {
    std::size_t to_read = std::min<std::size_t>(sizeof(buf), max_bytes - out.size());
    ifs.read(buf, static_cast<std::streamsize>(to_read));
    auto n = ifs.gcount();
    if (n <= 0) break;
    out.append(buf, static_cast<std::size_t>(n));
  }
  return out;
}

struct IniData {
  // Lowercase section -> (lowercase key -> raw value bytes)
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;

  bool has_section(std::string_view section) const {
    return sections.find(to_lower_ascii(section)) != sections.end();
  }

  std::optional<std::string> get(std::string_view section, std::string_view key) const {
    auto sit = sections.find(to_lower_ascii(section));
    if (sit == sections.end()) return std::nullopt;
    auto kit = sit->second.find(to_lower_ascii(key));
    if (kit == sit->second.end()) return std::nullopt;
    return kit->second;
  }

  std::optional<std::string> get_any_section(std::string_view key) const {
    if (auto v = get("game", key)) return v;
    if (auto v = get("rpg_rt", key)) return v;
    std::string lk = to_lower_ascii(key);
    for (const auto& [sec, kvs] : sections) {
      auto it = kvs.find(lk);
      if (it != kvs.end()) return it->second;
    }
    return std::nullopt;
  }
};

IniData parse_ini(std::string_view bytes) {
  IniData ini;
  // Strip UTF-8 BOM if present
  if (bytes.size() >= 3 &&
      static_cast<unsigned char>(bytes[0]) == 0xEF &&
      static_cast<unsigned char>(bytes[1]) == 0xBB &&
      static_cast<unsigned char>(bytes[2]) == 0xBF) {
    bytes = bytes.substr(3);
  }

  std::string current_section;
  std::size_t pos = 0;
  while (pos < bytes.size()) {
    std::size_t nl = bytes.find('\n', pos);
    std::string_view line = (nl == std::string_view::npos) ? bytes.substr(pos) : bytes.substr(pos, nl - pos);
    pos = (nl == std::string_view::npos) ? bytes.size() : (nl + 1);

    std::string trimmed = trim_ws(line);
    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      current_section = to_lower_ascii(trim_ws(std::string_view(trimmed).substr(1, trimmed.size() - 2)));
      ini.sections[current_section];
      continue;
    }
    std::size_t eq = trimmed.find('=');
    if (eq != std::string::npos) {
      std::string k = to_lower_ascii(trim_ws(std::string_view(trimmed).substr(0, eq)));
      std::string v = trim_ws(std::string_view(trimmed).substr(eq + 1));
      if (!k.empty()) {
        ini.sections[current_section][std::move(k)] = std::move(v);
      }
    }
  }
  return ini;
}

bool read_ber_uint(std::string_view data, std::size_t& pos, std::uint64_t& out) noexcept {
  out = 0;
  for (int i = 0; i < 10; ++i) {
    if (pos >= data.size()) return false;
    auto b = static_cast<std::uint8_t>(data[pos++]);
    out = (out << 7) | (b & 0x7Fu);
    if ((b & 0x80u) == 0) {
      return true;
    }
  }
  return false;
}

bool is_ldb_rm2003(const fs::path& ldb_path) {
  auto bytes_opt = read_file_bytes(ldb_path, 512u * 1024u);
  if (!bytes_opt) return false;
  std::string_view data = *bytes_opt;
  std::size_t pos = 0;
  std::uint64_t hdr_len = 0;
  if (!read_ber_uint(data, pos, hdr_len)) return false;
  if (hdr_len != 11 || pos + hdr_len > data.size()) return false;
  if (data.substr(pos, hdr_len) != "LcfDataBase") return false;
  pos += static_cast<std::size_t>(hdr_len);

  // Walk top-level LcfDataBase chunks safely
  while (pos < data.size()) {
    std::uint64_t chunk_id = 0;
    if (!read_ber_uint(data, pos, chunk_id)) break;
    if (chunk_id == 0) break;
    std::uint64_t chunk_len = 0;
    if (!read_ber_uint(data, pos, chunk_len)) break;
    if (chunk_len > data.size() - pos) break;

    // Top-level RM2003-only chunks in LcfDataBase:
    // 29 (0x1D) = battlecommands, 30 (0x1E) = classes, 32 (0x20) = battleranimations
    if (chunk_id == 29 || chunk_id == 30 || chunk_id == 32) {
      return true;
    }

    // Chunk 22 (0x16) = system -> sub-chunk 10 (0x0A) = ldb_id (2003 for RM2003)
    if (chunk_id == 22) {
      std::size_t sub_pos = pos;
      std::size_t sub_end = pos + static_cast<std::size_t>(chunk_len);
      while (sub_pos < sub_end) {
        std::uint64_t sub_id = 0;
        if (!read_ber_uint(data, sub_pos, sub_id)) break;
        if (sub_id == 0) break;
        std::uint64_t sub_len = 0;
        if (!read_ber_uint(data, sub_pos, sub_len)) break;
        if (sub_len > sub_end - sub_pos) break;
        if (sub_id == 10) {
          if (sub_len == 4) {
            auto b0 = static_cast<std::uint8_t>(data[sub_pos]);
            auto b1 = static_cast<std::uint8_t>(data[sub_pos + 1]);
            auto b2 = static_cast<std::uint8_t>(data[sub_pos + 2]);
            auto b3 = static_cast<std::uint8_t>(data[sub_pos + 3]);
            std::uint32_t val = static_cast<std::uint32_t>(b0) |
                                (static_cast<std::uint32_t>(b1) << 8) |
                                (static_cast<std::uint32_t>(b2) << 16) |
                                (static_cast<std::uint32_t>(b3) << 24);
            if (val == 2003u) return true;
          } else if (sub_len == 2) {
            auto b0 = static_cast<std::uint8_t>(data[sub_pos]);
            auto b1 = static_cast<std::uint8_t>(data[sub_pos + 1]);
            std::uint16_t val = static_cast<std::uint16_t>(b0) |
                                (static_cast<std::uint16_t>(b1) << 8);
            if (val == 2003u) return true;
          }
        }
        sub_pos += static_cast<std::size_t>(sub_len);
      }
    }

    pos += static_cast<std::size_t>(chunk_len);
  }
  return false;
}

std::string strip_rm_control_codes(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '\\' && i + 1 < input.size()) {
      char next = input[i + 1];
      if (next == '.' || next == '|' || next == '^' || next == '>' || next == '<' || next == '!') {
        ++i;
        continue;
      }
      if ((next == 'C' || next == 'c' || next == 'N' || next == 'n' || next == 'V' || next == 'v' ||
           next == 'P' || next == 'p' || next == 'I' || next == 'i') &&
          i + 2 < input.size() && input[i + 2] == '[') {
        std::size_t close = input.find(']', i + 3);
        if (close != std::string_view::npos) {
          i = close;
          continue;
        }
      }
    }
    out.push_back(input[i]);
  }
  return trim_ws(out);
}

std::string find_first_image_in_dir(const fs::path& base, std::string_view rel_dir) {
  auto dir_opt = paths::resolve_case_insensitive(base, rel_dir);
  if (!dir_opt) return "";
  std::error_code ec;
  if (!fs::is_directory(*dir_opt, ec)) return "";
  std::vector<fs::path> candidates;
  for (fs::directory_iterator it(*dir_opt, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    std::string ext = to_lower_ascii(it->path().extension().string());
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
      candidates.push_back(it->path());
    }
  }
  if (candidates.empty()) return "";
  std::sort(candidates.begin(), candidates.end());
  return normalize_path_string(candidates.front());
}

bool dir_contains_encrypted_mvz_assets(const fs::path& content_root) {
  static constexpr const char* kDirs[] = {"img", "audio"};
  for (const char* sub : kDirs) {
    auto sub_opt = paths::resolve_case_insensitive(content_root, sub);
    if (!sub_opt) continue;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(*sub_opt, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
      if (it.depth() > 2) {
        it.disable_recursion_pending();
        continue;
      }
      if (!it->is_regular_file(ec)) continue;
      std::string ext = to_lower_ascii(it->path().extension().string());
      if (ext == ".rpgmvp" || ext == ".rpgmvo" || ext == ".rpgmvm" ||
          ext == ".png_" || ext == ".ogg_" || ext == ".m4a_") {
        return true;
      }
    }
  }
  return false;
}

struct RootIniFile {
  fs::path path;
  std::string stem;
  IniData ini;
};

struct RootArchiveFile {
  fs::path path;
  std::string stem;
  std::string ext_lower;
};

std::optional<DetectedGame> inspect_direct(
    const fs::path& game_dir,
    std::string_view rel_path_for_id,
    const ScanOptions& options) {
  std::error_code ec;
  if (!fs::exists(game_dir, ec) || !fs::is_directory(game_dir, ec)) {
    return std::nullopt;
  }

  auto make_base_game = [&](EngineKind engine, const fs::path& content_root) {
    DetectedGame g;
    g.id = EngineDetector::compute_game_id(rel_path_for_id);
    g.engine = engine;
    g.core = core_for_engine(engine);
    g.game_path = normalize_path_string(game_dir);
    g.content_root = normalize_path_string(content_root);
    g.dir_mtime = read_dir_mtime_epoch(game_dir);
    return g;
  };

  auto extract_mvz_metadata = [&](DetectedGame& g, const fs::path& content_root) {
    g.detected_encoding = TextEncoding::Utf8;
    g.needs_rtp = false;
    g.exec_name = "Game";

    if (auto sys_json = paths::resolve_case_insensitive(content_root, "data/System.json")) {
      if (auto bytes = read_file_bytes(*sys_json)) {
        if (auto parsed = JsonValue::parse(*bytes)) {
          if (const auto* title_v = parsed->find("gameTitle")) {
            if (const auto* s = title_v->as_string()) {
              g.title = strip_rm_control_codes(*s);
            }
          }
          if (const auto* enc_img = parsed->find("hasEncryptedImages")) {
            if (enc_img->as_bool().value_or(false)) g.has_encrypted_archive = true;
          }
          if (const auto* enc_aud = parsed->find("hasEncryptedAudio")) {
            if (enc_aud->as_bool().value_or(false)) g.has_encrypted_archive = true;
          }
          if (const auto* key = parsed->find("encryptionKey")) {
            if (const auto* ks = key->as_string(); ks && !ks->empty()) {
              g.has_encrypted_archive = true;
            }
          }
        }
      }
    }

    if (!g.has_encrypted_archive) {
      g.has_encrypted_archive = dir_contains_encrypted_mvz_assets(content_root);
    }

    if (g.title.empty()) {
      g.title = game_dir.filename().string();
    }

    g.cover_source_path = find_first_image_in_dir(content_root, "img/titles1");
    if (g.cover_source_path.empty()) {
      if (auto icon = paths::resolve_case_insensitive(content_root, "icon/icon.png")) {
        g.cover_source_path = normalize_path_string(*icon);
      }
    }
  };

  // -------------------------------------------------------------------------
  // Priority 1: RPG Maker MZ (rmmz)
  // -------------------------------------------------------------------------
  if (paths::resolve_case_insensitive(game_dir, "js/rmmz_core.js")) {
    DetectedGame g = make_base_game(EngineKind::Rmmz, game_dir);
    extract_mvz_metadata(g, game_dir);
    return g;
  }
  if (auto www_core = paths::resolve_case_insensitive(game_dir, "www/js/rmmz_core.js")) {
    fs::path content_root = www_core->parent_path().parent_path();
    DetectedGame g = make_base_game(EngineKind::Rmmz, content_root);
    extract_mvz_metadata(g, content_root);
    return g;
  }

  // -------------------------------------------------------------------------
  // Priority 2: RPG Maker MV (rmmv)
  // -------------------------------------------------------------------------
  if (auto www_core = paths::resolve_case_insensitive(game_dir, "www/js/rpg_core.js")) {
    fs::path content_root = www_core->parent_path().parent_path();
    DetectedGame g = make_base_game(EngineKind::Rmmv, content_root);
    extract_mvz_metadata(g, content_root);
    return g;
  }
  if (paths::resolve_case_insensitive(game_dir, "js/rpg_core.js")) {
    DetectedGame g = make_base_game(EngineKind::Rmmv, game_dir);
    extract_mvz_metadata(g, game_dir);
    return g;
  }

  // Collect root .ini and .rgss* archives once for Priorities 3..7
  std::vector<RootIniFile> root_inis;
  std::vector<RootArchiveFile> root_archives;
  for (fs::directory_iterator it(game_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    std::string ext = to_lower_ascii(it->path().extension().string());
    std::string stem = it->path().stem().string();
    if (ext == ".rgss3a" || ext == ".rgss2a" || ext == ".rgssad") {
      root_archives.push_back({it->path(), stem, ext});
    } else if (ext == ".ini") {
      if (auto bytes = read_file_bytes(it->path(), 64u * 1024u)) {
        root_inis.push_back({it->path(), stem, parse_ini(*bytes)});
      }
    }
  }
  std::sort(root_archives.begin(), root_archives.end(), [](const auto& a, const auto& b) {
    return a.stem < b.stem;
  });
  std::sort(root_inis.begin(), root_inis.end(), [](const auto& a, const auto& b) {
    return a.stem < b.stem;
  });

  auto detect_rgss = [&](EngineKind engine,
                         std::string_view archive_ext,
                         std::string_view scripts_file,
                         std::string_view system_file,
                         std::string_view lib_prefix) -> std::optional<DetectedGame> {
    const RootArchiveFile* matched_archive = nullptr;
    for (const auto& arch : root_archives) {
      if (arch.ext_lower == archive_ext) {
        if (!matched_archive || to_lower_ascii(arch.stem) == "game") {
          matched_archive = &arch;
        }
      }
    }

    bool has_data = paths::resolve_case_insensitive(game_dir, scripts_file).has_value() ||
                    paths::resolve_case_insensitive(game_dir, system_file).has_value();

    const RootIniFile* matched_ini = nullptr;
    for (const auto& ini_file : root_inis) {
      if (auto lib = ini_file.ini.get_any_section("Library")) {
        if (contains_icase(*lib, lib_prefix)) {
          if (!matched_ini || to_lower_ascii(ini_file.stem) == "game") {
            matched_ini = &ini_file;
          }
        }
      }
    }

    if (!matched_archive && !has_data && !matched_ini) {
      return std::nullopt;
    }

    DetectedGame g = make_base_game(engine, game_dir);
    g.has_encrypted_archive = (matched_archive != nullptr);

    // Choose execName + best INI for title/RTP metadata
    const RootIniFile* meta_ini = matched_ini;
    if (matched_archive) {
      g.exec_name = matched_archive->stem;
      for (const auto& ini_file : root_inis) {
        if (to_lower_ascii(ini_file.stem) == to_lower_ascii(matched_archive->stem)) {
          meta_ini = &ini_file;
          break;
        }
      }
    } else if (matched_ini) {
      g.exec_name = matched_ini->stem;
    } else {
      // Fallback: check Game.ini or any [Game] INI
      for (const auto& ini_file : root_inis) {
        if (to_lower_ascii(ini_file.stem) == "game") {
          meta_ini = &ini_file;
          g.exec_name = ini_file.stem;
          break;
        }
        if (!meta_ini && ini_file.ini.has_section("game")) {
          meta_ini = &ini_file;
          g.exec_name = ini_file.stem;
        }
      }
    }

    if (meta_ini) {
      if (auto raw_title = meta_ini->ini.get_any_section("Title")) {
        TextEncoding det = TextEncoding::Auto;
        g.title = trim_ws(encoding::decode_to_utf8(*raw_title, TextEncoding::Auto, &det));
        if (det == TextEncoding::Auto && !raw_title->empty()) {
          det = TextEncoding::Utf8;
        }
        g.detected_encoding = det;
      }
      for (const char* rtp_key : {"RTP", "RTP1", "RTP2", "RTP3"}) {
        if (auto rtp_val = meta_ini->ini.get_any_section(rtp_key)) {
          if (!trim_ws(*rtp_val).empty()) {
            g.needs_rtp = true;
            break;
          }
        }
      }
    }

    if (g.detected_encoding == TextEncoding::Auto && options.fallback_encoding != TextEncoding::Auto) {
      g.detected_encoding = options.fallback_encoding;
    }
    if (g.title.empty()) {
      g.title = game_dir.filename().string();
    }
    g.cover_source_path = find_first_image_in_dir(game_dir, "Graphics/Titles");
    if (g.cover_source_path.empty()) {
      g.cover_source_path = find_first_image_in_dir(game_dir, "Graphics/Title");
    }
    return g;
  };

  // -------------------------------------------------------------------------
  // Priority 3: RPG Maker VX Ace (rmvxa)
  // -------------------------------------------------------------------------
  if (auto g = detect_rgss(EngineKind::Rmvxa, ".rgss3a", "Data/Scripts.rvdata2", "Data/System.rvdata2", "rgss3")) {
    return g;
  }

  // -------------------------------------------------------------------------
  // Priority 4: RPG Maker VX (rmvx)
  // -------------------------------------------------------------------------
  if (auto g = detect_rgss(EngineKind::Rmvx, ".rgss2a", "Data/Scripts.rvdata", "Data/System.rvdata", "rgss2")) {
    return g;
  }

  // -------------------------------------------------------------------------
  // Priority 5: RPG Maker XP (rmxp)
  // -------------------------------------------------------------------------
  if (auto g = detect_rgss(EngineKind::Rmxp, ".rgssad", "Data/Scripts.rxdata", "Data/System.rxdata", "rgss1")) {
    return g;
  }

  // -------------------------------------------------------------------------
  // Priority 6 & 7: RPG Maker 2003 (rm2k3) / 2000 (rm2k)
  // -------------------------------------------------------------------------
  auto ldb_path = paths::resolve_case_insensitive(game_dir, "RPG_RT.ldb");
  auto lmt_path = paths::resolve_case_insensitive(game_dir, "RPG_RT.lmt");
  if (ldb_path && lmt_path) {
    bool is_2003 = false;
    if (paths::resolve_case_insensitive(game_dir, "Battle2") ||
        paths::resolve_case_insensitive(game_dir, "BattleCharSet") ||
        paths::resolve_case_insensitive(game_dir, "BattleWeapon")) {
      is_2003 = true;
    }
    if (!is_2003 && is_ldb_rm2003(*ldb_path)) {
      is_2003 = true;
    }

    const RootIniFile* rpg_rt_ini = nullptr;
    for (const auto& ini_file : root_inis) {
      if (to_lower_ascii(ini_file.stem) == "rpg_rt") {
        rpg_rt_ini = &ini_file;
        break;
      }
    }

    if (!is_2003 && rpg_rt_ini) {
      for (const char* k : {"RTP", "Version", "Engine"}) {
        if (auto v = rpg_rt_ini->ini.get_any_section(k)) {
          if (contains_icase(*v, "2003")) {
            is_2003 = true;
            break;
          }
        }
      }
    }

    DetectedGame g = make_base_game(is_2003 ? EngineKind::Rm2k3 : EngineKind::Rm2k, game_dir);
    g.exec_name = "RPG_RT";
    g.has_encrypted_archive = false;
    g.needs_rtp = true;

    if (rpg_rt_ini) {
      if (auto full_pkg = rpg_rt_ini->ini.get_any_section("FullPackageFlag")) {
        if (trim_ws(*full_pkg) == "1") {
          g.needs_rtp = false;
        }
      }
      auto raw_title = rpg_rt_ini->ini.get_any_section("GameTitle");
      if (!raw_title) {
        raw_title = rpg_rt_ini->ini.get_any_section("Title");
      }
      if (raw_title) {
        TextEncoding det = TextEncoding::Auto;
        g.title = trim_ws(encoding::decode_to_utf8(*raw_title, TextEncoding::Auto, &det));
        if (det == TextEncoding::Auto && !raw_title->empty()) {
          det = TextEncoding::Utf8;
        }
        g.detected_encoding = det;
      }
    }

    if (g.detected_encoding == TextEncoding::Auto && options.fallback_encoding != TextEncoding::Auto) {
      g.detected_encoding = options.fallback_encoding;
    }
    if (g.title.empty()) {
      g.title = game_dir.filename().string();
    }
    g.cover_source_path = find_first_image_in_dir(game_dir, "Title");
    return g;
  }

  return std::nullopt;
}

bool is_ignored_dir_name(std::string_view name) {
  return name.empty() || name[0] == '.' || name == "__MACOSX";
}

void scan_recursive(
    const fs::path& current_dir,
    const fs::path& games_root,
    std::uint32_t current_depth,
    const ScanOptions& options,
    std::vector<DetectedGame>& out) {
  std::error_code ec;
  std::vector<fs::path> subdirs;
  for (fs::directory_iterator it(current_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_directory(ec)) continue;
    std::string fname = it->path().filename().string();
    if (is_ignored_dir_name(fname)) continue;
    subdirs.push_back(it->path());
  }
  std::sort(subdirs.begin(), subdirs.end());

  for (const auto& sub : subdirs) {
    std::string rel = fs::relative(sub, games_root, ec).generic_string();
    if (ec || rel.empty()) rel = sub.filename().string();

    if (auto detected = inspect_direct(sub, rel, options)) {
      out.push_back(std::move(*detected));
      continue;  // Never recurse into an identified game folder (e.g. www/ or Data/)
    }

    if (current_depth < options.max_depth) {
      scan_recursive(sub, games_root, current_depth + 1, options, out);
    }
  }
}

}  // namespace

std::string EngineDetector::compute_game_id(std::string_view relative_game_path) {
  // Normalize separators and trim slashes
  std::string norm;
  norm.reserve(relative_game_path.size());
  bool prev_slash = false;
  for (char c : relative_game_path) {
    char ch = (c == '\\') ? '/' : c;
    if (ch == '/') {
      if (!norm.empty() && !prev_slash) {
        norm.push_back('/');
      }
      prev_slash = true;
    } else {
      norm.push_back(ch);
      prev_slash = false;
    }
  }
  while (!norm.empty() && norm.back() == '/') {
    norm.pop_back();
  }
  if (norm.empty()) {
    norm = "game";
  }

  // FNV-1a 32-bit hash -> 6 lowercase hex chars (24-bit fingerprint)
  std::uint32_t hash = 2166136261u;
  for (unsigned char c : norm) {
    hash ^= c;
    hash *= 16777619u;
  }
  std::uint32_t fp = hash & 0xFFFFFFu;
  char hex_buf[8];
  std::snprintf(hex_buf, sizeof(hex_buf), "%06x", static_cast<unsigned>(fp));

  // Extract leaf directory name and sanitize to ASCII slug matching ^[a-zA-Z0-9._-]+$
  std::size_t last_slash = norm.rfind('/');
  std::string_view leaf = (last_slash == std::string::npos) ? std::string_view(norm)
                                                            : std::string_view(norm).substr(last_slash + 1);

  std::string slug;
  slug.reserve(32);
  bool last_dash = false;
  for (unsigned char c : leaf) {
    if (slug.size() >= 32) break;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.') {
      slug.push_back(static_cast<char>(c));
      last_dash = false;
    } else if (c >= 'A' && c <= 'Z') {
      slug.push_back(static_cast<char>(c - 'A' + 'a'));
      last_dash = false;
    } else {
      if (!slug.empty() && !last_dash) {
        slug.push_back('-');
        last_dash = true;
      }
    }
  }
  while (!slug.empty() && (slug.back() == '-' || slug.back() == '.')) {
    slug.pop_back();
  }
  if (slug.empty()) {
    slug = "game";
  }

  return slug + "-" + hex_buf;
}

std::optional<DetectedGame> EngineDetector::inspect_directory(
    const fs::path& candidate_dir,
    const ScanOptions& options) {
  std::string rel = candidate_dir.filename().string();
  if (rel.empty()) rel = normalize_path_string(candidate_dir);

  if (auto direct = inspect_direct(candidate_dir, rel, options)) {
    return direct;
  }

  // Allow 1-level wrapper unwrap (e.g., games/GameA/GameA/RPG_RT.ldb)
  std::error_code ec;
  if (!fs::is_directory(candidate_dir, ec)) {
    return std::nullopt;
  }
  std::vector<fs::path> child_dirs;
  for (fs::directory_iterator it(candidate_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_directory(ec)) continue;
    std::string fname = it->path().filename().string();
    if (is_ignored_dir_name(fname)) continue;
    child_dirs.push_back(it->path());
  }
  if (child_dirs.size() == 1) {
    std::string nested_rel = rel + "/" + child_dirs[0].filename().string();
    return inspect_direct(child_dirs[0], nested_rel, options);
  }
  return std::nullopt;
}

std::vector<DetectedGame> EngineDetector::scan_library(
    const fs::path& games_root,
    const ScanOptions& options) {
  std::vector<DetectedGame> results;
  std::error_code ec;
  if (!fs::exists(games_root, ec) || !fs::is_directory(games_root, ec)) {
    return results;
  }
  ScanOptions eff = options;
  if (eff.max_depth < 1) eff.max_depth = 1;
  scan_recursive(games_root, games_root, 1, eff, results);
  std::sort(results.begin(), results.end(), [](const DetectedGame& a, const DetectedGame& b) {
    return a.id < b.id;
  });
  return results;
}

}  // namespace nxrpg
