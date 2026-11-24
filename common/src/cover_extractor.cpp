#include "nxrpg/cover_extractor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "nxrpg/json.hpp"
#include "nxrpg/paths.hpp"

namespace nxrpg {

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

bool ends_with_ci(std::string_view s, std::string_view suffix) {
  if (s.size() < suffix.size()) return false;
  return to_lower_ascii(s.substr(s.size() - suffix.size())) == to_lower_ascii(suffix);
}

std::optional<std::string> read_file_str(const fs::path& p, std::size_t max_bytes = 32u * 1024u * 1024u) {
  std::error_code ec;
  if (!fs::is_regular_file(p, ec)) return std::nullopt;
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) return std::nullopt;
  ifs.seekg(0, std::ios::end);
  auto sz = ifs.tellg();
  if (sz < 0 || static_cast<std::size_t>(sz) > max_bytes) return std::nullopt;
  ifs.seekg(0, std::ios::beg);
  std::string buf(static_cast<std::size_t>(sz), '\0');
  if (sz > 0) {
    ifs.read(buf.data(), sz);
  }
  return buf;
}

std::uint32_t read_u32_le(const std::uint8_t* p) noexcept {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

bool is_title_asset_path(std::string_view rel_path) {
  std::string lower = to_lower_ascii(rel_path);
  std::replace(lower.begin(), lower.end(), '\\', '/');
  bool in_titles = (lower.rfind("graphics/titles/", 0) == 0 ||
                    lower.rfind("graphics/title/", 0) == 0);
  if (!in_titles) return false;
  return ends_with_ci(lower, ".png") || ends_with_ci(lower, ".jpg") || ends_with_ci(lower, ".jpeg");
}

std::string image_ext_for_bytes(const std::string& bytes, std::string_view fallback_ext) {
  if (bytes.size() >= 8 &&
      static_cast<unsigned char>(bytes[0]) == 0x89 &&
      bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    return ".png";
  }
  if (bytes.size() >= 3 &&
      static_cast<unsigned char>(bytes[0]) == 0xFF &&
      static_cast<unsigned char>(bytes[1]) == 0xD8 &&
      static_cast<unsigned char>(bytes[2]) == 0xFF) {
    return ".jpg";
  }
  return std::string(fallback_ext);
}

std::array<std::uint8_t, 16> parse_mvz_key(const fs::path& content_root, bool& ok) {
  ok = false;
  std::array<std::uint8_t, 16> key{};
  auto sys_path = paths::resolve_case_insensitive(content_root, "data/System.json");
  if (!sys_path) return key;
  auto raw = read_file_str(*sys_path, 4u * 1024u * 1024u);
  if (!raw) return key;
  auto parsed = JsonValue::parse(*raw);
  if (!parsed || !parsed->is_object()) return key;
  const auto* key_v = parsed->find("encryptionKey");
  const auto* hex = key_v ? key_v->as_string() : nullptr;
  if (!hex) return key;

  std::string clean;
  clean.reserve(32);
  for (char ch : *hex) {
    if (std::isxdigit(static_cast<unsigned char>(ch))) {
      clean.push_back(ch);
    }
  }
  if (clean.size() < 32) return key;
  for (std::size_t i = 0; i < 16; ++i) {
    auto hex_val = [](char c) -> std::uint8_t {
      if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
      if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
      return static_cast<std::uint8_t>(c - 'A' + 10);
    };
    key[i] = static_cast<std::uint8_t>((hex_val(clean[i * 2]) << 4) | hex_val(clean[i * 2 + 1]));
  }
  ok = true;
  return key;
}

std::optional<std::string> extract_mvz_encrypted_title(const fs::path& content_root) {
  auto titles_dir = paths::resolve_case_insensitive(content_root, "img/titles1");
  if (!titles_dir) return std::nullopt;

  bool has_key = false;
  auto key = parse_mvz_key(content_root, has_key);
  if (!has_key) return std::nullopt;

  std::error_code ec;
  std::vector<fs::path> encrypted;
  for (fs::directory_iterator it(*titles_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    std::string ext = to_lower_ascii(it->path().extension().string());
    if (ext == ".rpgmvp" || ext == ".png_") {
      encrypted.push_back(it->path());
    }
  }
  if (encrypted.empty()) return std::nullopt;
  std::sort(encrypted.begin(), encrypted.end());

  auto raw = read_file_str(encrypted.front(), 16u * 1024u * 1024u);
  if (!raw || raw->size() <= 16) return std::nullopt;
  if (raw->compare(0, 5, "RPGMV") != 0) return std::nullopt;

  std::string payload = raw->substr(16);
  std::size_t n = std::min<std::size_t>(16, payload.size());
  for (std::size_t i = 0; i < n; ++i) {
    payload[i] = static_cast<char>(static_cast<std::uint8_t>(payload[i]) ^ key[i]);
  }
  return payload;
}

void decrypt_rgss_payload(std::string& data, std::uint32_t magic) {
  std::uint32_t key = magic;
  for (std::size_t i = 0; i < data.size(); ++i) {
    std::size_t shift = (i % 4) * 8;
    std::uint8_t kb = static_cast<std::uint8_t>((key >> shift) & 0xFFu);
    data[i] = static_cast<char>(static_cast<std::uint8_t>(data[i]) ^ kb);
    if ((i % 4) == 3) {
      key = key * 7u + 3u;
    }
  }
}

std::optional<std::pair<std::string, std::string>> extract_rgss_title(
    const fs::path& game_dir,
    std::string_view exec_name) {
  std::error_code ec;
  std::vector<fs::path> archives;
  for (const char* ext : {".rgss3a", ".rgss2a", ".rgssad"}) {
    if (auto p = paths::resolve_case_insensitive(game_dir, std::string(exec_name) + ext)) {
      archives.push_back(*p);
    }
  }
  if (archives.empty()) {
    for (fs::directory_iterator it(game_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
      if (!it->is_regular_file(ec)) continue;
      std::string ext = to_lower_ascii(it->path().extension().string());
      if (ext == ".rgss3a" || ext == ".rgss2a" || ext == ".rgssad") {
        archives.push_back(it->path());
      }
    }
  }
  if (archives.empty()) return std::nullopt;

  std::ifstream ifs(archives.front(), std::ios::binary);
  if (!ifs) return std::nullopt;

  std::uint8_t hdr[8]{};
  ifs.read(reinterpret_cast<char*>(hdr), 8);
  if (ifs.gcount() != 8 || std::memcmp(hdr, "RGSSAD\0", 7) != 0) {
    return std::nullopt;
  }

  std::uint8_t version = hdr[7];
  if (version == 1) {
    std::uint32_t magic = 0xDEADCAFEu;
    while (ifs) {
      std::uint8_t len_buf[4]{};
      ifs.read(reinterpret_cast<char*>(len_buf), 4);
      if (ifs.gcount() != 4) break;
      std::uint32_t name_len = read_u32_le(len_buf) ^ magic;
      magic = magic * 7u + 3u;
      if (name_len == 0 || name_len > 1024) break;

      std::string name(name_len, '\0');
      ifs.read(name.data(), static_cast<std::streamsize>(name_len));
      if (static_cast<std::uint32_t>(ifs.gcount()) != name_len) break;
      for (std::uint32_t i = 0; i < name_len; ++i) {
        name[i] = static_cast<char>(static_cast<std::uint8_t>(name[i]) ^ (magic & 0xFFu));
        magic = magic * 7u + 3u;
      }

      std::uint8_t size_buf[4]{};
      ifs.read(reinterpret_cast<char*>(size_buf), 4);
      if (ifs.gcount() != 4) break;
      std::uint32_t data_size = read_u32_le(size_buf) ^ magic;
      magic = magic * 7u + 3u;

      if (data_size > 16u * 1024u * 1024u) break;
      if (is_title_asset_path(name)) {
        std::string data(data_size, '\0');
        ifs.read(data.data(), static_cast<std::streamsize>(data_size));
        if (static_cast<std::uint32_t>(ifs.gcount()) != data_size) break;
        decrypt_rgss_payload(data, magic);
        std::string ext = image_ext_for_bytes(data, to_lower_ascii(fs::path(name).extension().string()));
        return std::make_pair(std::move(data), std::move(ext));
      } else {
        ifs.seekg(static_cast<std::streamoff>(data_size), std::ios::cur);
      }
    }
  } else if (version == 3) {
    std::uint8_t seed_buf[4]{};
    ifs.read(reinterpret_cast<char*>(seed_buf), 4);
    if (ifs.gcount() != 4) return std::nullopt;
    std::uint32_t magic = read_u32_le(seed_buf) * 9u + 3u;

    struct Entry {
      std::uint32_t offset;
      std::uint32_t size;
      std::uint32_t key;
      std::string name;
    };
    std::vector<Entry> candidates;
    for (int guard = 0; guard < 20000 && ifs; ++guard) {
      std::uint8_t meta[16]{};
      ifs.read(reinterpret_cast<char*>(meta), 16);
      if (ifs.gcount() != 16) break;
      std::uint32_t offset = read_u32_le(meta) ^ magic;
      if (offset == 0) break;
      std::uint32_t size = read_u32_le(meta + 4) ^ magic;
      std::uint32_t key = read_u32_le(meta + 8) ^ magic;
      std::uint32_t name_len = read_u32_le(meta + 12) ^ magic;
      if (name_len == 0 || name_len > 1024) break;

      std::string name(name_len, '\0');
      ifs.read(name.data(), static_cast<std::streamsize>(name_len));
      if (static_cast<std::uint32_t>(ifs.gcount()) != name_len) break;
      for (std::uint32_t i = 0; i < name_len; ++i) {
        std::size_t shift = (i % 4) * 8;
        name[i] = static_cast<char>(static_cast<std::uint8_t>(name[i]) ^ ((magic >> shift) & 0xFFu));
      }
      if (is_title_asset_path(name) && size <= 16u * 1024u * 1024u) {
        candidates.push_back({offset, size, key, std::move(name)});
        break;  // First title match is enough
      }
    }

    if (!candidates.empty()) {
      const auto& e = candidates.front();
      ifs.clear();
      ifs.seekg(static_cast<std::streamoff>(e.offset), std::ios::beg);
      std::string data(e.size, '\0');
      ifs.read(data.data(), static_cast<std::streamsize>(e.size));
      if (static_cast<std::uint32_t>(ifs.gcount()) == e.size) {
        decrypt_rgss_payload(data, e.key);
        std::string ext = image_ext_for_bytes(data, to_lower_ascii(fs::path(e.name).extension().string()));
        return std::make_pair(std::move(data), std::move(ext));
      }
    }
  }

  return std::nullopt;
}

}  // namespace

fs::path CoverExtractor::extract_to_cache(
    const DetectedGame& game,
    const fs::path& cover_cache_dir) {
  if (game.id.empty()) return {};
  std::error_code ec;

  // Return already cached cover image if present
  for (const char* ext : {".jpg", ".png", ".bmp"}) {
    fs::path existing = cover_cache_dir / (game.id + ext);
    if (fs::is_regular_file(existing, ec)) {
      return existing;
    }
  }

  // 1. Copy plain unencrypted cover if available
  if (!game.cover_source_path.empty()) {
    fs::path src(game.cover_source_path);
    if (auto bytes = read_file_str(src, 16u * 1024u * 1024u)) {
      std::string ext = image_ext_for_bytes(*bytes, to_lower_ascii(src.extension().string()));
      if (ext == ".jpeg") ext = ".jpg";
      if (ext.empty()) ext = ".png";
      fs::path dst = cover_cache_dir / (game.id + ext);
      if (paths::write_file_atomic(dst, *bytes)) {
        return dst;
      }
    }
  }

  // 2. Decrypt MV/MZ encrypted title image (.rpgmvp / .png_)
  if ((game.engine == EngineKind::Rmmv || game.engine == EngineKind::Rmmz) &&
      !game.content_root.empty()) {
    if (auto png_bytes = extract_mvz_encrypted_title(fs::path(game.content_root))) {
      std::string ext = image_ext_for_bytes(*png_bytes, ".png");
      fs::path dst = cover_cache_dir / (game.id + ext);
      if (paths::write_file_atomic(dst, *png_bytes)) {
        return dst;
      }
    }
  }

  // 3. Extract RGSS1/2/3 archive title image (.rgssad / .rgss2a / .rgss3a)
  if (rgss_version_for_engine(game.engine) > 0 && !game.game_path.empty()) {
    if (auto extracted = extract_rgss_title(fs::path(game.game_path), game.exec_name)) {
      std::string ext = extracted->second;
      if (ext == ".jpeg") ext = ".jpg";
      fs::path dst = cover_cache_dir / (game.id + ext);
      if (paths::write_file_atomic(dst, extracted->first)) {
        return dst;
      }
    }
  }

  return {};
}

}  // namespace nxrpg
