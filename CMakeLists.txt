# Factory-owned file: agents must not edit it. The build of this library and its tests; scripts/check.cmake runs
# the gates on it. Sources are found by globbing, so new files under src/, include/ and tests/ need no edit here;
# dependencies are wired in cmake/dependencies.cmake by the factory's dependency tool.
cmake_minimum_required(VERSION 3.29)

# The release manager sets this version (a literal x.y.z, never a variable).
project(lua_sandbox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FABRICA_COVERAGE "Instrument the library for llvm-cov (clang)" OFF)
set(FABRICA_WARNINGS -Wall -Wextra -Wpedantic -Werror)

include(cmake/dependencies.cmake)

file(GLOB_RECURSE FABRICA_LIBRARY_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE FABRICA_PUBLIC_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.hpp")
add_library(lua_sandbox ${FABRICA_LIBRARY_SOURCES} ${FABRICA_PUBLIC_HEADERS})
add_library(trysac::lua_sandbox ALIAS lua_sandbox)
target_include_directories(
  lua_sandbox PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
)
target_compile_features(lua_sandbox PUBLIC cxx_std_20)
target_compile_options(lua_sandbox PRIVATE ${FABRICA_WARNINGS})
target_link_libraries(lua_sandbox PUBLIC ${FABRICA_LIBRARY_TARGETS})
if(FABRICA_COVERAGE)
  target_compile_options(lua_sandbox PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()

# Tests: every tests/**/*.cpp except fixtures, in one Catch2 executable. FABRICA_CI=1 at configure time also runs the
# CI-only conformance tier (test cases tagged [conformance]); the local gates and coverage skip it.
enable_testing()
file(GLOB_RECURSE FABRICA_TEST_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/*.cpp")
list(FILTER FABRICA_TEST_SOURCES EXCLUDE REGEX "/tests/fixtures/")
add_executable(unit_tests ${FABRICA_TEST_SOURCES})
target_compile_options(unit_tests PRIVATE ${FABRICA_WARNINGS})
target_link_libraries(unit_tests PRIVATE lua_sandbox ${FABRICA_TEST_TARGETS})
if(FABRICA_COVERAGE)
  target_link_options(unit_tests PRIVATE -fprofile-instr-generate)
endif()
if("$ENV{FABRICA_CI}" STREQUAL "1")
  set(FABRICA_TEST_FILTER)
else()
  set(FABRICA_TEST_FILTER "~[conformance]")
endif()
# A fixed seed keeps the generated property inputs the same on every run.
add_test(NAME unit_tests COMMAND unit_tests --rng-seed 20260101 ${FABRICA_TEST_FILTER})
set_tests_properties(
  unit_tests PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                        ENVIRONMENT "LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/coverage/%p.profraw"
)
