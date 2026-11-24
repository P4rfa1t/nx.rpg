#include "nxrpg/cover_extractor.hpp"
#include "test_common.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace nxrpg;

namespace {

void append_u32_le(std::string& out, std::uint32_t v) {
  out.push_back(static_cast<char>(v & 0xFFu));
  out.push_back(static_cast<char>((v >> 8) & 0xFFu));
  out.push_back(static_cast<char>((v >> 16) & 0xFFu));
  out.push_back(static_cast<char>((v >> 24) & 0xFFu));
}

void xor_rgss_payload(std::string& data, std::uint32_t magic) {
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

std::string read_all(const std::filesystem::path& p) {
  std::ifstream ifs(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  nxrpg::test::TempDir root("cover_extractor");
  const auto cache_dir = root.path() / "cache/covers";
  const std::string fake_png = "\x89PNG\r\n\x1a\n" "0123456789ABCDEF_TITLE";

  // 1. Plain unencrypted cover copy
  {
    const auto game_dir = root.path() / "plain_rm2k";
    nxrpg::test::write_bytes(game_dir / "Title/title.png", fake_png);

    DetectedGame g;
    g.id = "plain-rm2k-111111";
    g.engine = EngineKind::Rm2k;
    g.game_path = game_dir.generic_string();
    g.content_root = game_dir.generic_string();
    g.cover_source_path = (game_dir / "Title/title.png").generic_string();

    auto out = CoverExtractor::extract_to_cache(g, cache_dir);
    NXRPG_CHECK(!out.empty());
    NXRPG_CHECK(out.filename() == "plain-rm2k-111111.png");
    NXRPG_CHECK(read_all(out) == fake_png);
  }

  // 2. Encrypted MV/MZ `.rpgmvp` title decryption
  {
    const auto game_dir = root.path() / "enc_mv/www";
    nxrpg::test::write_bytes(
        game_dir / "data/System.json",
        R"({"gameTitle":"MV Encrypted","encryptionKey":"00112233445566778899aabbccddeeff"})");

    const std::array<std::uint8_t, 16> key = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::string enc;
    enc.append("RPGMV", 5);
    enc.append(11, '\0');
    std::string payload = fake_png;
    for (std::size_t i = 0; i < 16 && i < payload.size(); ++i) {
      payload[i] = static_cast<char>(static_cast<std::uint8_t>(payload[i]) ^ key[i]);
    }
    enc.append(payload);
    nxrpg::test::write_bytes(game_dir / "img/titles1/Castle.rpgmvp", enc);

    DetectedGame g;
    g.id = "enc-mv-222222";
    g.engine = EngineKind::Rmmv;
    g.game_path = (root.path() / "enc_mv").generic_string();
    g.content_root = game_dir.generic_string();

    auto out = CoverExtractor::extract_to_cache(g, cache_dir);
    NXRPG_CHECK(!out.empty());
    NXRPG_CHECK(out.filename() == "enc-mv-222222.png");
    NXRPG_CHECK(read_all(out) == fake_png);
  }

  // 3. Synthetic RGSS1 `.rgssad` archive title extraction
  {
    const auto game_dir = root.path() / "rgss1_game";
    std::string arch;
    arch.append("RGSSAD\0\x01", 8);
    std::uint32_t magic = 0xDEADCAFEu;

    const std::string entry_name = "Graphics/Titles/001-Title01.png";
    append_u32_le(arch, static_cast<std::uint32_t>(entry_name.size()) ^ magic);
    magic = magic * 7u + 3u;
    for (char ch : entry_name) {
      arch.push_back(static_cast<char>(static_cast<std::uint8_t>(ch) ^ (magic & 0xFFu)));
      magic = magic * 7u + 3u;
    }
    append_u32_le(arch, static_cast<std::uint32_t>(fake_png.size()) ^ magic);
    magic = magic * 7u + 3u;
    std::string enc_payload = fake_png;
    xor_rgss_payload(enc_payload, magic);
    arch.append(enc_payload);

    nxrpg::test::write_bytes(game_dir / "Game.rgssad", arch);

    DetectedGame g;
    g.id = "rgss1-333333";
    g.engine = EngineKind::Rmxp;
    g.exec_name = "Game";
    g.game_path = game_dir.generic_string();
    g.content_root = game_dir.generic_string();

    auto out = CoverExtractor::extract_to_cache(g, cache_dir);
    NXRPG_CHECK(!out.empty());
    NXRPG_CHECK(out.filename() == "rgss1-333333.png");
    NXRPG_CHECK(read_all(out) == fake_png);
  }

  // 4. Synthetic RGSS3 `.rgss3a` renamed archive title extraction
  {
    const auto game_dir = root.path() / "rgss3_game";
    std::string arch;
    arch.append("RGSSAD\0\x03", 8);
    const std::uint32_t seed = 0x12345678u;
    append_u32_le(arch, seed);
    const std::uint32_t magic = seed * 9u + 3u;

    const std::string entry_name = "Graphics/Titles/TitleScreen.png";
    const std::uint32_t file_key = 0xCAFEBABEu;
    const std::uint32_t data_offset = static_cast<std::uint32_t>(
        8 + 4 + 16 + entry_name.size() + 16);

    append_u32_le(arch, data_offset ^ magic);
    append_u32_le(arch, static_cast<std::uint32_t>(fake_png.size()) ^ magic);
    append_u32_le(arch, file_key ^ magic);
    append_u32_le(arch, static_cast<std::uint32_t>(entry_name.size()) ^ magic);
    for (std::size_t i = 0; i < entry_name.size(); ++i) {
      std::size_t shift = (i % 4) * 8;
      arch.push_back(static_cast<char>(
          static_cast<std::uint8_t>(entry_name[i]) ^ ((magic >> shift) & 0xFFu)));
    }
    // End-of-table sentinel (offset == 0 -> encoded as 0 ^ magic)
    append_u32_le(arch, magic);
    append_u32_le(arch, magic);
    append_u32_le(arch, magic);
    append_u32_le(arch, magic);

    std::string enc_payload = fake_png;
    xor_rgss_payload(enc_payload, file_key);
    arch.append(enc_payload);

    nxrpg::test::write_bytes(game_dir / "Touhou.rgss3a", arch);

    DetectedGame g;
    g.id = "rgss3-444444";
    g.engine = EngineKind::Rmvxa;
    g.exec_name = "Touhou";
    g.game_path = game_dir.generic_string();
    g.content_root = game_dir.generic_string();

    auto out = CoverExtractor::extract_to_cache(g, cache_dir);
    NXRPG_CHECK(!out.empty());
    NXRPG_CHECK(out.filename() == "rgss3-444444.png");
    NXRPG_CHECK(read_all(out) == fake_png);
  }

  return nxrpg::test::finish();
}
