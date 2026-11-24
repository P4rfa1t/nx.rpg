#include "launcher/app.hpp"

#include <chrono>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

#include "launcher/services/library_service.hpp"
#include "nxrpg/json.hpp"

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
#include <switch.h>
#endif

namespace nxrpg::launcher {

BootContext LauncherApp::parse_boot_context(int argc, const char* const* argv) {
  BootContext ctx;

#if defined(__SWITCH__) || defined(NINTENDO_SWITCH)
  AppletType at = appletGetAppletType();
  if (at == AppletType_Application || at == AppletType_SystemApplication) {
    ctx.memory_mode = MemoryExecutionMode::ApplicationFullRam;
  } else {
    ctx.memory_mode = MemoryExecutionMode::AppletRestrictedRam;
  }
#else
  ctx.memory_mode = MemoryExecutionMode::HostDesktop;
#endif

  if (!argv || argc <= 1) {
    return ctx;
  }

  for (int i = 1; i < argc; ++i) {
    if (!argv[i]) continue;
    std::string_view arg(argv[i]);
    if (arg == "--from-core") {
      ctx.returned_from_core = true;
    } else if (arg == "--simulate-applet") {
      ctx.memory_mode = MemoryExecutionMode::AppletRestrictedRam;
    } else if (arg == "--sd-root" && i + 1 < argc && argv[i + 1]) {
      ctx.sd_root = argv[++i];
    } else if (arg.rfind("--sd-root=", 0) == 0 && arg.size() > 10) {
      ctx.sd_root = std::string(arg.substr(10));
    }
  }

  return ctx;
}

bool LauncherApp::finalize_active_session(
    const std::filesystem::path& sd_root,
    std::uint64_t now_epoch) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path session_file = sd_root / "runtime/active_session.json";
  if (!fs::is_regular_file(session_file, ec)) {
    return false;
  }

  std::ifstream ifs(session_file, std::ios::binary);
  if (!ifs) return false;
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();
  fs::remove(session_file, ec);

  auto parsed = JsonValue::parse(content);
  if (!parsed || !parsed->is_object()) return false;
  const auto* gid = parsed->find("gameId") ? parsed->find("gameId")->as_string() : nullptr;
  auto started = parsed->find("startedAt") ? parsed->find("startedAt")->as_int64() : std::nullopt;
  if (!gid || gid->empty() || !started || *started <= 0) return false;

  if (now_epoch == 0) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
    now_epoch = secs > 0 ? static_cast<std::uint64_t>(secs) : 0;
  }

  std::uint64_t start_u = static_cast<std::uint64_t>(*started);
  std::uint64_t elapsed = (now_epoch > start_u) ? (now_epoch - start_u) : 0;
  if (elapsed > 24ULL * 3600ULL) {
    elapsed = 0;  // Guard against RTC jumps
  }

  LibraryService lib(sd_root);
  return lib.record_play_session(*gid, now_epoch, elapsed);
}

}  // namespace nxrpg::launcher
