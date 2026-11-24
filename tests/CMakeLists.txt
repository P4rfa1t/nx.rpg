cmake_minimum_required(VERSION 3.22)
project(nxrpg_host_tests LANGUAGES CXX)

enable_testing()

if(NOT TARGET nxrpg_common)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

if(NOT TARGET nxrpg_launcher_core)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../launcher" "${CMAKE_CURRENT_BINARY_DIR}/launcher")
endif()

foreach(test_name IN ITEMS
  test_engine_detector
  test_cover_extractor
  test_config_merger
  test_case_path
  test_chainload_argv
  test_logger
)
  add_executable(${test_name} "${test_name}.cpp")
  target_link_libraries(${test_name} PRIVATE nxrpg_common)
  target_compile_features(${test_name} PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(${test_name} PRIVATE /W4 /utf-8)
  else()
    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(test_launcher_services test_launcher_services.cpp)
target_link_libraries(test_launcher_services PRIVATE nxrpg_launcher_core nxrpg_common)
target_compile_features(test_launcher_services PRIVATE cxx_std_20)
target_compile_definitions(test_launcher_services PRIVATE
  NXRPG_I18N_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../launcher/resources/i18n"
)
if(MSVC)
  target_compile_options(test_launcher_services PRIVATE /W4 /utf-8)
else()
  target_compile_options(test_launcher_services PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME test_launcher_services COMMAND test_launcher_services)
