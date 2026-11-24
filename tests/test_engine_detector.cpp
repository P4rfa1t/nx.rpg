#include "nxrpg/engine_detector.hpp"
#include "nxrpg/encoding.hpp"
#include "test_common.hpp"

#include <regex>

using namespace nxrpg;

namespace {

// Minimal LCF binary header helper: BER length 11 + "LcfDataBase" + optional chunks
std::string make_minimal_ldb(bool is_2003, bool use_system_id_chunk = false) {
  std::string out;
  out.push_back(11);
  out.append("LcfDataBase");
  if (is_2003) {
    if (use_system_id_chunk) {
      // Chunk 22 (system): sub-chunk 10 (ldb_id) = uint32 LE 2003 (0xD3 0x07 0x00 0x00)
      out.push_back(22);
      out.push_back(7);  // subchunk id (1) + len (1) + data (4) + end (1)
      out.push_back(10);
      out.push_back(4);
      out.push_back(static_cast<char>(0xD3));
      out.push_back(static_cast<char>(0x07));
      out.push_back(0x00);
      out.push_back(0x00);
      out.push_back(0x00);
    } else {
      // Top-level chunk 29 (battlecommands, RM2003 only)
      out.push_back(29);
      out.push_back(1);
      out.push_back(0x00);
    }
  }
  out.push_back(0x00);  // End of root chunks
  return out;
}

}  // namespace

int main() {
  nxrpg::test::TempDir root("engine_detector");
  const auto games_dir = root.path() / "games";

  // 1. 01_rmmz_basic
  {
    const auto d = games_dir / "01_rmmz_basic";
    nxrpg::test::write_bytes(d / "index.html", "<html></html>");
    nxrpg::test::write_bytes(d / "js/rmmz_core.js", "// mz");
    nxrpg::test::write_bytes(d / "data/System.json", R"({"gameTitle":"MZ Test"})");

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rmmz);
      NXRPG_CHECK(det->core == CoreKind::NxjsMvz);
      NXRPG_CHECK(rgss_version_for_engine(det->engine) == 0);
      NXRPG_CHECK(det->title == "MZ Test");
      NXRPG_CHECK(det->content_root == det->game_path);
      NXRPG_CHECK(!det->has_encrypted_archive);
    }
  }

  // 2. 02_rmmv_desktop_www (with uppercase/mixed paths & encryption marker)
  {
    const auto d = games_dir / "02_rmmv_desktop_www";
    nxrpg::test::write_bytes(d / "Game.exe", "MZ");
    nxrpg::test::write_bytes(d / "WWW/JS/RPG_CORE.JS", "// mv");
    nxrpg::test::write_bytes(
        d / "WWW/data/System.json",
        R"({"gameTitle":"\\C[1]MV Desktop","hasEncryptedImages":true,"encryptionKey":"abc"})");

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rmmv);
      NXRPG_CHECK(det->core == CoreKind::NxjsMvz);
      NXRPG_CHECK(det->title == "MV Desktop");
      NXRPG_CHECK(det->has_encrypted_archive);
      NXRPG_CHECK(det->content_root.size() > det->game_path.size());
      NXRPG_CHECK(det->content_root.ends_with("/www") || det->content_root.ends_with("/WWW"));
    }
  }

  // 3. 03_rmvxa_renamed_archive (Shift_JIS "テスト" = 0x83 0x65 0x83 0x58 0x83 0x67)
  {
    const auto d = games_dir / "03_rmvxa_renamed_archive";
    const std::string sjis_title = "\x83\x65\x83\x58\x83\x67";
    nxrpg::test::write_bytes(
        d / "Touhou.ini",
        "[Game]\r\nLibrary=System\\RGSS301.dll\r\nTitle=" + sjis_title + "\r\nRTP=RPGVXAce\r\n");
    nxrpg::test::write_bytes(d / "Touhou.rgss3a", std::string("RGSSAD\0\x03", 8));

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rmvxa);
      NXRPG_CHECK(det->core == CoreKind::MkxpZ);
      NXRPG_CHECK(rgss_version_for_engine(det->engine) == 3);
      NXRPG_CHECK(det->exec_name == "Touhou");
      NXRPG_CHECK(det->has_encrypted_archive);
      NXRPG_CHECK(det->needs_rtp);
      NXRPG_CHECK(det->detected_encoding == TextEncoding::ShiftJis);
      NXRPG_CHECK(det->title == "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88");  // UTF-8 "テスト"
    }
  }

  // 4. 04_rmvx_standard
  {
    const auto d = games_dir / "04_rmvx_standard";
    nxrpg::test::write_bytes(d / "Game.ini", "[Game]\r\nLibrary=RGSS202E.dll\r\nTitle=VX Quest\r\n");
    nxrpg::test::write_bytes(d / "Data/Scripts.rvdata", "dummy");

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rmvx);
      NXRPG_CHECK(det->core == CoreKind::MkxpZ);
      NXRPG_CHECK(rgss_version_for_engine(det->engine) == 2);
      NXRPG_CHECK(det->exec_name == "Game");
      NXRPG_CHECK(!det->has_encrypted_archive);
      NXRPG_CHECK(!det->needs_rtp);
    }
  }

  // 5. 05_rmxp_gbk_title (GBK "测试游戏" = B2 E2 CA D4 D3 CE CF B7)
  {
    const auto d = games_dir / "05_rmxp_gbk_title";
    const std::string gbk_title = "\xB2\xE2\xCA\xD4\xD3\xCE\xCF\xB7";
    nxrpg::test::write_bytes(
        d / "Game.ini",
        "[Game]\r\nLibrary=RGSS104E.dll\r\nRTP1=Standard\r\nTitle=" + gbk_title + "\r\n");
    nxrpg::test::write_bytes(d / "Game.rgssad", std::string("RGSSAD\0\x01", 8));

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rmxp);
      NXRPG_CHECK(det->core == CoreKind::MkxpZ);
      NXRPG_CHECK(rgss_version_for_engine(det->engine) == 1);
      NXRPG_CHECK(det->detected_encoding == TextEncoding::Gbk);
      NXRPG_CHECK(det->title == "\xE6\xB5\x8B\xE8\xAF\x95\xE6\xB8\xB8\xE6\x88\x8F");  // UTF-8 "测试游戏"
      NXRPG_CHECK(det->has_encrypted_archive);
      NXRPG_CHECK(det->needs_rtp);
    }
  }

  // 6. 06_rm2k3_ldb (both Battle2/ and LDB-only 2003 chunk check)
  {
    const auto d = games_dir / "06_rm2k3_ldb";
    nxrpg::test::write_bytes(d / "RPG_RT.ldb", make_minimal_ldb(true, true));
    nxrpg::test::write_bytes(d / "RPG_RT.lmt", "\x0A" "LcfMapTree");
    std::filesystem::create_directories(d / "Battle2");

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rm2k3);
      NXRPG_CHECK(det->core == CoreKind::EasyRpg);
      NXRPG_CHECK(det->needs_rtp);
    }

    // Also test LDB chunk-22/subchunk-10 ldb_id=2003 without Battle2/ directory
    const auto d_ldb_only = games_dir / "06b_rm2k3_ldb_only";
    nxrpg::test::write_bytes(d_ldb_only / "RPG_RT.ldb", make_minimal_ldb(true, true));
    nxrpg::test::write_bytes(d_ldb_only / "RPG_RT.lmt", "\x0A" "LcfMapTree");
    auto det_only = EngineDetector::inspect_directory(d_ldb_only);
    NXRPG_CHECK(det_only.has_value());
    if (det_only) {
      NXRPG_CHECK(det_only->engine == EngineKind::Rm2k3);
    }
  }

  // 7. 07_rm2k_ldb
  {
    const auto d = games_dir / "07_rm2k_ldb";
    nxrpg::test::write_bytes(d / "RPG_RT.ldb", make_minimal_ldb(false));
    nxrpg::test::write_bytes(d / "RPG_RT.lmt", "\x0A" "LcfMapTree");
    nxrpg::test::write_bytes(d / "RPG_RT.ini", "[RPG_RT]\r\nGameTitle=RM2K Sample\r\nFullPackageFlag=1\r\n");

    auto det = EngineDetector::inspect_directory(d);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rm2k);
      NXRPG_CHECK(det->core == CoreKind::EasyRpg);
      NXRPG_CHECK(det->title == "RM2K Sample");
      NXRPG_CHECK(!det->needs_rtp);
    }
  }

  // 8. 1-level nested wrapper unwrap (games/WrappedGame/WrappedGame/RPG_RT.ldb)
  {
    const auto outer = games_dir / "08_wrapped";
    const auto inner = outer / "WrappedInner";
    nxrpg::test::write_bytes(inner / "RPG_RT.ldb", make_minimal_ldb(false));
    nxrpg::test::write_bytes(inner / "RPG_RT.lmt", "\x0A" "LcfMapTree");

    auto det = EngineDetector::inspect_directory(outer);
    NXRPG_CHECK(det.has_value());
    if (det) {
      NXRPG_CHECK(det->engine == EngineKind::Rm2k);
      NXRPG_CHECK(det->game_path.ends_with("WrappedInner"));
    }
  }

  // 9. Library scan depth & non-duplication check (should find all 9 games above, without recursing into www/)
  {
    auto list = EngineDetector::scan_library(games_dir, ScanOptions{.max_depth = 2});
    NXRPG_CHECK(list.size() == 9);
  }

  // 10. Deterministic game.id checks
  {
    std::string id1 = EngineDetector::compute_game_id("Action/Ib Remake");
    std::string id2 = EngineDetector::compute_game_id("Action\\Ib Remake\\");
    std::string id3 = EngineDetector::compute_game_id("Horror/Ib Remake");
    std::string id_cjk = EngineDetector::compute_game_id("\xE4\xB8\x9C\xE6\x96\xB9");

    NXRPG_CHECK(id1 == id2);
    NXRPG_CHECK(id1 != id3);
    std::regex id_re("^[a-zA-Z0-9._-]+$");
    NXRPG_CHECK(std::regex_match(id1, id_re));
    NXRPG_CHECK(std::regex_match(id_cjk, id_re));
    NXRPG_CHECK(id_cjk.rfind("game-", 0) == 0);
  }

  return nxrpg::test::finish();
}
