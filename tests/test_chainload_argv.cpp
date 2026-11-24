#include "nxrpg/chainload.hpp"
#include "nxrpg/launch_manifest.hpp"
#include "nxrpg/paths.hpp"
#include "test_common.hpp"

using namespace nxrpg;

int main() {
  // CRITICAL INVARIANT: nxjs-mvz MUST NOT receive positional argv[1]
  {
    std::string argv = Chainloader::format_core_argv(
        CoreKind::NxjsMvz,
        paths::core_nro_path(CoreKind::NxjsMvz),
        paths::kLaunchManifest);
    NXRPG_CHECK(argv == "sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro");
    NXRPG_CHECK(argv.find("--nxrpg-manifest") == std::string::npos);
    NXRPG_CHECK(argv.find(' ') == std::string::npos);
  }

  // easyrpg and mkxp-z MUST receive `--nxrpg-manifest <manifest_path>`
  {
    std::string argv = Chainloader::format_core_argv(
        CoreKind::EasyRpg,
        paths::core_nro_path(CoreKind::EasyRpg),
        paths::kLaunchManifest);
    NXRPG_CHECK(argv == "sdmc:/switch/nx.rpg/cores/easyrpg.nro --nxrpg-manifest sdmc:/switch/nx.rpg/runtime/launch.json");
  }
  {
    std::string argv = Chainloader::format_core_argv(
        CoreKind::MkxpZ,
        paths::core_nro_path(CoreKind::MkxpZ),
        paths::kLaunchManifest);
    NXRPG_CHECK(argv == "sdmc:/switch/nx.rpg/cores/mkxp-z.nro --nxrpg-manifest sdmc:/switch/nx.rpg/runtime/launch.json");
  }

  // Unknown core returns empty string
  NXRPG_CHECK(Chainloader::format_core_argv(CoreKind::Unknown, "x.nro", "m.json").empty());

  // parse_manifest_arg tests
  {
    const char* argv1[] = {"easyrpg.nro", "--nxrpg-manifest", "sdmc:/switch/nx.rpg/runtime/launch.json"};
    auto parsed = parse_manifest_arg(3, argv1);
    NXRPG_CHECK(parsed.has_value());
    if (parsed) NXRPG_CHECK(*parsed == "sdmc:/switch/nx.rpg/runtime/launch.json");
  }
  {
    const char* argv2[] = {"mkxp-z.nro", "--nxrpg-manifest=custom/launch.json"};
    auto parsed = parse_manifest_arg(2, argv2);
    NXRPG_CHECK(parsed.has_value());
    if (parsed) NXRPG_CHECK(*parsed == "custom/launch.json");
  }
  {
    const char* argv3[] = {"easyrpg.nro", "--nxrpg-manifest"};
    NXRPG_CHECK(!parse_manifest_arg(2, argv3).has_value());
  }

  // Host builds return false safely for Switch envSetNextLoad wrappers
  LaunchManifest dummy;
  dummy.game.core = CoreKind::EasyRpg;
  NXRPG_CHECK(!Chainloader::schedule_core_launch(dummy));
  NXRPG_CHECK(!Chainloader::schedule_return_to_launcher(paths::kLauncherNro));

  return nxrpg::test::finish();
}
