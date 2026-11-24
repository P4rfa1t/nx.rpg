cmake_minimum_required(VERSION 3.22)
project(nxrpg_common LANGUAGES CXX)

# Dual-target static library:
# - On Nintendo Switch (NINTENDO_SWITCH / __SWITCH__): links libnx for chainload & SD helpers.
# - On Host (Linux / Windows / macOS): builds pure C++20 detector, config merger, and manifest tests.

add_library(nxrpg_common INTERFACE)

target_include_directories(nxrpg_common INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(nxrpg_common INTERFACE cxx_std_20)

# Phase 1 Note:
# Once source files are added under common/src/*.cpp, switch INTERFACE -> STATIC
# and conditionally link `nx` when `NINTENDO_SWITCH` is defined.
