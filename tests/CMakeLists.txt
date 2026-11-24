cmake_minimum_required(VERSION 3.22)
project(nxrpg_host_tests LANGUAGES CXX)

enable_testing()

if(NOT TARGET nxrpg_common)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

# Phase 1 Host Test Targets (runnable on Linux / Windows / macOS without devkitPro):
#   - test_engine_detector : verifies 7-generation precedence, MV `www/` unwrap, and renamed `execName`
#   - test_config_merger   : verifies default -> global.json -> games/<id>.json -> LaunchManifest precedence
#   - test_case_path       : verifies case-insensitive path resolution and atomic `.tmp` writes
#   - test_chainload_argv  : verifies `nxjs-mvz` omits positional argv[1] while `easyrpg`/`mkxp-z` pass `--nxrpg-manifest`
