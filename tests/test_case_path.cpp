#include "nxrpg/paths.hpp"
#include "test_common.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

using namespace nxrpg;

int main() {
  nxrpg::test::TempDir root("case_path");

  // Case-insensitive path resolution
  nxrpg::test::write_bytes(root.path() / "Audio/BGM/Town1.OGG", "OggS");

  auto p1 = paths::resolve_case_insensitive(root.path(), "audio/bgm/town1.ogg");
  NXRPG_CHECK(p1.has_value());
  if (p1) {
    NXRPG_CHECK(p1->filename().string() == "Town1.OGG");
  }

  auto p2 = paths::resolve_case_insensitive(root.path(), "AUDIO\\bgm\\./Town1.ogg");
  NXRPG_CHECK(p2.has_value());

  auto p_missing = paths::resolve_case_insensitive(root.path(), "audio/bgm/missing.ogg");
  NXRPG_CHECK(!p_missing.has_value());

  auto p_escape = paths::resolve_case_insensitive(root.path(), "../escape");
  NXRPG_CHECK(!p_escape.has_value());

  // Atomic file write & overwrite without leftover .tmp
  const auto target = root.path() / "nested/dir/launch.json";
  NXRPG_CHECK(paths::write_file_atomic(target, R"({"v":1})"));
  NXRPG_CHECK(paths::write_file_atomic(target, R"({"v":2})"));

  std::filesystem::path leftover_tmp = target;
  leftover_tmp += ".tmp";
  NXRPG_CHECK(!std::filesystem::exists(leftover_tmp));
  NXRPG_CHECK(std::filesystem::exists(target));

  std::ifstream ifs(target, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  NXRPG_CHECK(content == R"({"v":2})");

  // Canonical path helpers
  NXRPG_CHECK(paths::core_nro_path(CoreKind::EasyRpg) == "sdmc:/switch/nx.rpg/cores/easyrpg.nro");
  NXRPG_CHECK(paths::core_nro_path(CoreKind::NxjsMvz) == "sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro");
  NXRPG_CHECK(paths::core_nro_path(CoreKind::MkxpZ) == "sdmc:/switch/nx.rpg/cores/mkxp-z.nro");
  NXRPG_CHECK(paths::default_rtp_path(EngineKind::Rm2k) == "sdmc:/switch/nx.rpg/rtp/rm2000");
  NXRPG_CHECK(paths::default_rtp_path(EngineKind::Rm2k3) == "sdmc:/switch/nx.rpg/rtp/rm2003");
  NXRPG_CHECK(paths::default_rtp_path(EngineKind::Rmvxa) == "sdmc:/switch/nx.rpg/rtp/rmvxa");
  NXRPG_CHECK(paths::default_rtp_path(EngineKind::Rmmv).empty());

  return nxrpg::test::finish();
}
