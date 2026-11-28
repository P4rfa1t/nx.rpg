cmake_minimum_required(VERSION 3.22)

option(NXRPG_DESKTOP_PREVIEW "Build Launcher for desktop UI preview (GLFW/SDL2 backend)" OFF)
set(NXRPG_BOREALIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/borealis" CACHE PATH "Path to Borealis checkout")

if(EXISTS "${NXRPG_BOREALIS_DIR}/library/CMakeLists.txt")
  set(BOREALIS_LIBRARY "${NXRPG_BOREALIS_DIR}/library")
  include("${BOREALIS_LIBRARY}/cmake/commonOption.cmake")
  option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
  cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
  cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
  cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)
  if(NXRPG_DESKTOP_PREVIEW AND NOT PLATFORM_SWITCH)
    set(PLATFORM_DESKTOP ON CACHE BOOL "" FORCE)
  endif()
  if(PLATFORM_SWITCH OR PLATFORM_DESKTOP)
    include("${BOREALIS_LIBRARY}/cmake/toolchain.cmake")
  endif()
endif()

project(nxrpg_launcher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared contracts & detector library
if(NOT TARGET nxrpg_common)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

# Headless launcher services & state management (compiles on Host and Switch)
add_library(nxrpg_launcher_core STATIC
  src/boot_context.cpp
  src/services/config_service.cpp
  src/services/i18n_service.cpp
  src/services/library_service.cpp
  src/services/core_dispatcher.cpp
  src/services/environment_doctor.cpp
)

target_include_directories(nxrpg_launcher_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(nxrpg_launcher_core PUBLIC nxrpg_common)
target_compile_features(nxrpg_launcher_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(nxrpg_launcher_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(nxrpg_launcher_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NINTENDO_SWITCH OR PLATFORM_SWITCH)
  target_compile_definitions(nxrpg_launcher_core PUBLIC NINTENDO_SWITCH)
endif()

# Full Borealis GUI executable (built when Borealis checkout is available and target platform selected)
if(EXISTS "${NXRPG_BOREALIS_DIR}/library/CMakeLists.txt" AND (PLATFORM_SWITCH OR PLATFORM_DESKTOP))
  set(BRLS_RESOURCES_DIR "${NXRPG_BOREALIS_DIR}")
  add_subdirectory("${NXRPG_BOREALIS_DIR}/library" "${CMAKE_CURRENT_BINARY_DIR}/borealis")

  find_package(Threads REQUIRED)
  list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

  set(LAUNCHER_GUI_SRCS
    src/main.cpp
    src/app.cpp
    src/views/root_frame.cpp
    src/views/game_grid_tab.cpp
    src/views/game_detail_activity.cpp
    src/views/diagnostics_tab.cpp
    src/views/transfer_tab.cpp
    src/services/transfer_service.cpp
  )
  if(PLATFORM_SWITCH)
    list(APPEND LAUNCHER_GUI_SRCS
      "${NXRPG_BOREALIS_DIR}/library/lib/platforms/switch/switch_wrapper.c"
      src/transfer/ftpd.c
      src/transfer/mtp_core.c
      src/transfer/mtp_usb.c
    )
  endif()

  add_executable(nxrpg_launcher ${LAUNCHER_GUI_SRCS})

  target_link_libraries(nxrpg_launcher PRIVATE
    nxrpg_launcher_core
    nxrpg_common
    borealis
    ${APP_PLATFORM_LIB}
  )
  target_compile_features(nxrpg_launcher PRIVATE cxx_std_20)
  target_compile_options(nxrpg_launcher PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
  target_compile_definitions(nxrpg_launcher PRIVATE
    NXRPG_HAS_BOREALIS=1
    NXRPG_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
  )
  if(APP_PLATFORM_LINK_OPTION)
    target_link_options(nxrpg_launcher PRIVATE ${APP_PLATFORM_LINK_OPTION})
  endif()

  if(PLATFORM_SWITCH)
    set_target_properties(nxrpg_launcher PROPERTIES OUTPUT_NAME "nx.rpg" SUFFIX ".elf")
    set(ROMFS_STAGE "${CMAKE_CURRENT_BINARY_DIR}/romfs_stage")
    if(USE_DEKO3D)
      gen_dksh("${ROMFS_STAGE}/shaders")
    endif()
    add_custom_target(nx.rpg.nro ALL DEPENDS nxrpg_launcher
      COMMAND "${NX_NACPTOOL_EXE}" --create "nx.rpg" "nx.rpg contributors" "${PROJECT_VERSION}" "${CMAKE_CURRENT_BINARY_DIR}/nx.rpg.nacp" --titleid=016e787270670000
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${NXRPG_BOREALIS_DIR}/resources" "${ROMFS_STAGE}"
      COMMAND "${CMAKE_COMMAND}" -E remove_directory "${ROMFS_STAGE}/font"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n" "${ROMFS_STAGE}/nxrpg_i18n"
      COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/transfer/LICENSE.rpynx" "${ROMFS_STAGE}/LICENSE.rpynx"
      COMMAND "${NX_ELF2NRO_EXE}" "${CMAKE_CURRENT_BINARY_DIR}/nx.rpg.elf" "${CMAKE_CURRENT_BINARY_DIR}/nx.rpg.nro"
              "--icon=${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.jpg"
              "--nacp=${CMAKE_CURRENT_BINARY_DIR}/nx.rpg.nacp"
              "--romfsdir=${ROMFS_STAGE}"
    )
  endif()
endif()
