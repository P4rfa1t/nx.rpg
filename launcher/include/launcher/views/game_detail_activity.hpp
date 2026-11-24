#pragma once

#include "nxrpg/engine_detector.hpp"

namespace nxrpg::launcher::views {

/// Game detail drawer / activity allowing:
/// - Immediate launch (`A`)
/// - Per-game overrides (`Y` -> encoding, CPU boost, scaling, RTP override, Win32API stubs, NW.js mode)
/// - Open game save folder info & play time stats
class GameDetailActivity {
 public:
  explicit GameDetailActivity(DetectedGame game);

 private:
  DetectedGame game_;
};

}  // namespace nxrpg::launcher::views
