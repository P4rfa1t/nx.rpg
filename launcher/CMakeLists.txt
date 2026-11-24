cmake_minimum_required(VERSION 3.22)
project(nxrpg_launcher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Phase 1 Build Skeleton:
# Supports two build modes:
#   1. Nintendo Switch (`cmake --preset switch-release`) -> outputs `nx.rpg.nro`
#   2. Desktop Preview (`cmake -B build/host-launcher -DNXRPG_DESKTOP_PREVIEW=ON`) -> GLFW/SDL2 Borealis window

option(NXRPG_DESKTOP_PREVIEW "Build Launcher for desktop UI preview (GLFW/SDL2 backend)" OFF)

# Shared contracts & detector header interface
if(NOT TARGET nxrpg_common)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

# Interface target exposing Launcher's include tree during Phase 0/1 scaffolding
add_library(nxrpg_launcher_headers INTERFACE)
target_include_directories(nxrpg_launcher_headers INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(nxrpg_launcher_headers INTERFACE nxrpg_common)

# Planned Phase 1 executable target:
# - Link third_party/borealis (`BRLS_RESOURCES="romfs:/"` on Switch, `"./launcher/resources/"` on Desktop)
# - Generate NACP (`nx.rpg`, Author `nx.rpg contributors`, TitleID `016e787270670000`)
# - Package `nx.rpg.nro` with embedded `launcher/resources/` RomFS and `assets/icon.jpg`
