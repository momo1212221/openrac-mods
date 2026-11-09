# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20)

# This file works both as a standalone mod checkout and as an add_subdirectory()
# inside an OpenRAC-mods style workspace/test harness.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(arsenal_mastery LANGUAGES C CXX)
  include(CTest)
endif()

# Standalone build: use OPENRAC_SDK=<dir>, OpenRACMod_DIR=<dir>/cmake, or the SDK pinned
# by sdk.lock after tools/fetch_sdk.py has populated .sdk/.
if(NOT COMMAND openrac_add_mod)
  if(NOT OpenRACMod_DIR)
    if(DEFINED OPENRAC_SDK)
      set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
      file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
      string(JSON sdk_version GET "${lock}" version)
      set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
      if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
        message(FATAL_ERROR
          "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
      endif()
    endif()
  endif()
  find_package(OpenRACMod 1.12 REQUIRED)
endif()

openrac_add_mod(
  arsenal_mastery
  MANIFEST openrac-mod.json
  SOURCES
    src/arsenal_mastery.cpp
    src/progression.cpp
    src/persistence.cpp
)
target_include_directories(arsenal_mastery PRIVATE src)
set_target_properties(arsenal_mastery PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  target_compile_options(arsenal_mastery PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
  add_executable(arsenal_mastery_progression_test
    tests/progression_test.cpp
    src/progression.cpp
  )
  target_include_directories(arsenal_mastery_progression_test PRIVATE src)
  set_target_properties(arsenal_mastery_progression_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(arsenal_mastery_progression_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(arsenal_mastery_progression_test PRIVATE -Wall -Wextra -UNDEBUG)
  endif()
  add_test(NAME arsenal_mastery_progression COMMAND arsenal_mastery_progression_test)

  add_executable(arsenal_mastery_persistence_test
    tests/persistence_test.cpp
    src/persistence.cpp
    src/progression.cpp
  )
  target_include_directories(arsenal_mastery_persistence_test PRIVATE src)
  set_target_properties(arsenal_mastery_persistence_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(arsenal_mastery_persistence_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(arsenal_mastery_persistence_test PRIVATE -Wall -Wextra -UNDEBUG)
  endif()
  add_test(NAME arsenal_mastery_persistence COMMAND arsenal_mastery_persistence_test)

  add_executable(arsenal_mastery_ui_style_test tests/ui_style_test.cpp)
  target_include_directories(arsenal_mastery_ui_style_test PRIVATE src)
  set_target_properties(arsenal_mastery_ui_style_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(arsenal_mastery_ui_style_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(arsenal_mastery_ui_style_test PRIVATE -Wall -Wextra -UNDEBUG)
  endif()
  add_test(NAME arsenal_mastery_ui_style COMMAND arsenal_mastery_ui_style_test)

  add_executable(arsenal_mastery_native_ui_test tests/native_ui_test.cpp)
  target_include_directories(arsenal_mastery_native_ui_test PRIVATE tests/stubs src)
  set_target_properties(arsenal_mastery_native_ui_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(arsenal_mastery_native_ui_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(arsenal_mastery_native_ui_test PRIVATE -Wall -Wextra -UNDEBUG)
  endif()
  add_test(NAME arsenal_mastery_native_ui COMMAND arsenal_mastery_native_ui_test)
endif()
