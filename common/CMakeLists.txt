cmake_minimum_required(VERSION 3.22)
project(nxrpg_common LANGUAGES CXX)

# Dual-target static library:
# - On Nintendo Switch (NINTENDO_SWITCH / __SWITCH__): links libnx for chainload & SD helpers.
# - On Host (Linux / Windows / macOS): builds pure C++20 detector, config merger, and manifest tests.

add_library(nxrpg_common STATIC
  src/types.cpp
  src/json.cpp
  src/encoding.cpp
  src/paths.cpp
  src/engine_detector.cpp
  src/launch_manifest.cpp
  src/config_merger.cpp
  src/chainload.cpp
)

target_include_directories(nxrpg_common PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(nxrpg_common PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(nxrpg_common PRIVATE /W4 /utf-8)
else()
  target_compile_options(nxrpg_common PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NINTENDO_SWITCH)
  target_compile_definitions(nxrpg_common PUBLIC NINTENDO_SWITCH)
  target_link_libraries(nxrpg_common PUBLIC nx)
endif()
