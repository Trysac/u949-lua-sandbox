# The repository's C++ files (library, public headers and tests; fixtures excluded), relative to its root.
# Factory-owned file: agents must not edit it.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

function(fabrica_cpp_sources out)
  file(GLOB_RECURSE files RELATIVE "${FABRICA_ROOT}" LIST_DIRECTORIES false
       "${FABRICA_ROOT}/include/*.hpp" "${FABRICA_ROOT}/src/*.cpp" "${FABRICA_ROOT}/src/*.hpp"
       "${FABRICA_ROOT}/tests/*.cpp" "${FABRICA_ROOT}/tests/*.hpp")
  list(FILTER files EXCLUDE REGEX "^tests/fixtures/")
  list(SORT files)
  set(${out} ${files} PARENT_SCOPE)
endfunction()
