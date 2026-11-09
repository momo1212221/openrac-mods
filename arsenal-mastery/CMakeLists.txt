# SPDX-License-Identifier: MIT
# Initial native OpenRAC port of Arsenal Mastery.
openrac_add_mod(arsenal_mastery MANIFEST openrac-mod.json SOURCES src/arsenal_mastery.cpp)
target_include_directories(arsenal_mastery PRIVATE src)
set_target_properties(arsenal_mastery PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  target_compile_options(arsenal_mastery PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
  add_executable(arsenal_mastery_progression_test tests/progression_test.cpp)
  target_include_directories(arsenal_mastery_progression_test PRIVATE src)
  target_compile_features(arsenal_mastery_progression_test PRIVATE cxx_std_17)
  add_test(NAME arsenal_mastery_progression COMMAND arsenal_mastery_progression_test)
endif()
