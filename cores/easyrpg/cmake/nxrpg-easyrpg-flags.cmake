# Additional CMake cache settings layered onto EasyRPG/Player's `switch-release` preset
# Usage (Phase 1):
#   cmake --preset switch-release -C ../../cores/easyrpg/cmake/nxrpg-easyrpg-flags.cmake

set(PLAYER_BUILD_LIBLCF ON CACHE BOOL "Build liblcf from source if not preinstalled in prefix" FORCE)
set(PLAYER_ENABLE_TESTS OFF CACHE BOOL "Disable EasyRPG unit test runner for Switch cross build" FORCE)
set(PLAYER_WITH_NXRPG_MANIFEST ON CACHE BOOL "Enable --nxrpg-manifest and envSetNextLoad return hook" FORCE)
