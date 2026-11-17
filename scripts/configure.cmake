# Installs the dependencies of vcpkg.json and configures the gates build: `cmake -P scripts/configure.cmake`.
# Factory-owned file: agents must not edit it.
include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

fabrica_configure(code)
if(NOT code EQUAL 0)
  message(NOTICE "configure: failed (${code})")
  cmake_language(EXIT 1)
endif()
