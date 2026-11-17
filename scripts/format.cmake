# Applies clang-format to every C++ file of the repository: `cmake -P scripts/format.cmake`.
# Factory-owned file: agents must not edit it.
include("${CMAKE_CURRENT_LIST_DIR}/sources.cmake")

fabrica_cpp_sources(files)
if(files)
  fabrica_run(code clang-format -i ${files})
  if(NOT code EQUAL 0)
    message(NOTICE "format: clang-format failed (${code})")
    cmake_language(EXIT 1)
  endif()
endif()
