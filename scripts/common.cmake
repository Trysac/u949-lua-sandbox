# Shared helpers of the scripts in this folder (CMake script mode, `cmake -P`): no shell, the same on Windows and
# Linux. Factory-owned file: agents must not edit it.
cmake_minimum_required(VERSION 3.29)
include_guard(GLOBAL)

get_filename_component(FABRICA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# The build directory: under FABRICA_BUILD_DIR when set (the gates image keeps build output out of the mounted
# repository), else build/dev in the repository (ignored by git).
if(DEFINED ENV{FABRICA_BUILD_DIR} AND NOT "$ENV{FABRICA_BUILD_DIR}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{FABRICA_BUILD_DIR}/dev" FABRICA_BUILD)
else()
  set(FABRICA_BUILD "${FABRICA_ROOT}/build/dev")
endif()
# Never wider than 4 parallel jobs: the gates share their host with other work.
set(FABRICA_JOBS 4)

if(NOT EXISTS "${FABRICA_ROOT}/CMakeLists.txt" OR NOT EXISTS "${FABRICA_ROOT}/vcpkg.json")
  message(NOTICE "scripts: CMakeLists.txt and vcpkg.json not found next to scripts/")
  cmake_language(EXIT 2)
endif()

# Prints one line on standard output (message() writes to standard error or adds a "-- " prefix).
function(fabrica_say)
  string(JOIN " " text ${ARGN})
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${text}")
endfunction()

# Whether the environment variable is exactly "1".
function(fabrica_flag out name)
  if("$ENV{${name}}" STREQUAL "1")
    set(${out} TRUE PARENT_SCOPE)
  else()
    set(${out} FALSE PARENT_SCOPE)
  endif()
endfunction()

# Runs a command from the repository root with its output streamed; sets `out` to its exit code (or an error text).
function(fabrica_run out)
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${FABRICA_ROOT}" RESULT_VARIABLE code)
  set(${out} "${code}" PARENT_SCOPE)
endfunction()

# The full path of a program on PATH, or "" (find_program caches nothing in script mode).
function(fabrica_which out program)
  unset(found)
  find_program(found NAMES "${program}" NO_CACHE)
  if(found)
    set(${out} "${found}" PARENT_SCOPE)
  else()
    set(${out} "" PARENT_SCOPE)
  endif()
endfunction()

# Configures the gates build (CMakePresets.json "dev"): vcpkg installs the manifest's dependencies here.
function(fabrica_configure out)
  if(NOT DEFINED ENV{VCPKG_ROOT} OR NOT EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    message(NOTICE "VCPKG_ROOT does not name a vcpkg checkout: clone https://github.com/microsoft/vcpkg at the "
                   "commit of vcpkg.json's builtin-baseline, run its bootstrap script and set VCPKG_ROOT")
    set(${out} 1 PARENT_SCOPE)
    return()
  endif()
  if(ARGC GREATER 1)  # fabrica_configure(code output): also return the configure output (vcpkg's usage notes)
    execute_process(COMMAND "${CMAKE_COMMAND}" --preset dev -B "${FABRICA_BUILD}" WORKING_DIRECTORY "${FABRICA_ROOT}"
                    RESULT_VARIABLE code OUTPUT_VARIABLE text ERROR_VARIABLE text)
    message(NOTICE "${text}")
    set(${ARGV1} "${text}" PARENT_SCOPE)
  else()
    fabrica_run(code "${CMAKE_COMMAND}" --preset dev -B "${FABRICA_BUILD}")
  endif()
  set(${out} "${code}" PARENT_SCOPE)
endfunction()
