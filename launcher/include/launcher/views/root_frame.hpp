#pragma once

namespace nxrpg::launcher::views {

/// Primary Borealis `brls::TabFrame` hosting:
/// 1. All Games (`GameGridTab`)
/// 2. Recently Played / Favorites
/// 3. Environment & Crash Log Doctor (`DiagnosticsTab`)
/// 4. Global Settings (`config/global.json`)
class RootFrame {
 public:
  RootFrame() = default;
};

}  // namespace nxrpg::launcher::views
