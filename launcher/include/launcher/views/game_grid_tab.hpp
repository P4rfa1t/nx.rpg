#pragma once

#include "nxrpg/types.hpp"

namespace nxrpg::launcher::views {

/// Controller- and touch-friendly Borealis recycling grid displaying game cards
/// with engine badges (`2K` / `2K3` / `XP` / `VX` / `VXA` / `MV` / `MZ`) and filter chips.
class GameGridTab {
 public:
  void set_engine_filter(EngineKind filter_or_unknown_for_all);
};

}  // namespace nxrpg::launcher::views
