# Stack

## Used

| Tool | Why |
|---|---|
| C++20 + CMake presets + Ninja | The standard C++ build; `CMakePresets.json` holds the one gates configuration. |
| clang (LLVM 23) | One toolchain for the compiler (`-Wall -Wextra -Wpedantic -Werror`), clang-format, clang-tidy and llvm-cov. |
| clang-format | Google style at 120 columns, applied and checked on every C++ file. |
| clang-tidy | A curated check set (bugprone, cert, clang-analyzer, cppcoreguidelines, modernize, performance, readability); every finding is an error. |
| Catch2 v3 | Test cases, matchers and generators (the property tests) in one framework. |
| llvm-cov | Source-based line coverage of the library; the gate reads its JSON summary. |
| Doxygen | Fails on any undocumented public declaration; generates no output. |
| vcpkg (manifest mode) | `builtin-baseline` pins every port, an exact override pins each direct dependency, CMake finds them through the toolchain file. |
| Semgrep CE + OSV | The factory's own rules; OSV advisories over each port's upstream release (git tag) and over the repository. |
| gitleaks | Secret scanning locally and in CI. |
| GitHub Actions + Dependabot | Thin CI wrapper over the local gates; weekly updates of the vcpkg baseline. |

## Discarded

| Tool | Why not |
|---|---|
| Conan | Needs a Python install and a lockfile step; vcpkg is CMake-native and its baseline already pins every version. Neither package manager is an OSV ecosystem. |
| GoogleTest + RapidCheck | Catch2 covers examples, matchers and generated inputs with one dependency. |
| gcovr / GCC | llvm-cov reads clang's source-based coverage directly, with no Python tool. |
| libFuzzer | Needs a fuzzing harness per API; not part of the local gates. |
| Shell scripts for gates | Not portable to Windows; the gates are a CMake script in `scripts/check.cmake`. |
