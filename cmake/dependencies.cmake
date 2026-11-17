# Factory-owned file: agents must not edit it. One block per dependency of vcpkg.json, written by
# scripts/add_dependency.cmake from the port's own usage notes: its find_package call and the targets the library
# (FABRICA_LIBRARY_TARGETS) or the tests (FABRICA_TEST_TARGETS) link.
set(FABRICA_LIBRARY_TARGETS)
set(FABRICA_TEST_TARGETS)

# catch2 (tests)
find_package(Catch2 CONFIG REQUIRED)
list(APPEND FABRICA_TEST_TARGETS Catch2::Catch2WithMain)
