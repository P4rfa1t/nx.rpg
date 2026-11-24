# Additional CMake cache settings layered onto EasyRPG/Player's Switch build
# Prefer preinstalled liblcf in $DEVKITPRO/portlibs/switch when available;
# otherwise pass -DPLAYER_BUILD_LIBLCF=ON.

option(NXRPG_PREBUILT_LIBLCF "Use preinstalled liblcf from portlibs/switch" ON)
if(NXRPG_PREBUILT_LIBLCF)
  set(PLAYER_BUILD_LIBLCF OFF CACHE BOOL "Use preinstalled liblcf" FORCE)
else()
  set(PLAYER_BUILD_LIBLCF ON CACHE BOOL "Build liblcf from source if not preinstalled in prefix" FORCE)
endif()
set(PLAYER_ENABLE_TESTS OFF CACHE BOOL "Disable EasyRPG unit test runner for Switch cross build" FORCE)
set(PLAYER_WITH_NXRPG_MANIFEST ON CACHE BOOL "Enable --nxrpg-manifest and envSetNextLoad return hook" FORCE)
set(CMAKE_PROJECT_EasyRPG_Player_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/nxrpg-easyrpg-post.cmake" CACHE FILEPATH "Post-project hook for Switch portlib deps" FORCE)
