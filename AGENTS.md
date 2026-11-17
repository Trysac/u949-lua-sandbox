# AGENTS.md

Instructions for coding agents (and humans) working on the C++ library `lua-sandbox`
(namespace `trysac::lua_sandbox`, CMake target `trysac::lua_sandbox`).

## Commands

The scripts are CMake scripts (`cmake -P`): no shell, the same on Windows and Linux. They need
CMake 3.29 or newer, Ninja, clang 23 (clang-format, clang-tidy, llvm-cov) and vcpkg (`VCPKG_ROOT`).

| Task | Command |
|---|---|
| Install dependencies and configure | `cmake -P scripts/configure.cmake` |
| Run every gate (must pass before a PR) | `cmake -P scripts/check.cmake` |
| Tests only | `cmake -P scripts/check.cmake -- tests` |
| Format | `cmake -P scripts/format.cmake` |
| Lint | `cmake -P scripts/check.cmake -- lint` (warnings and clang-tidy findings are errors) |

The gate sequence is: clang-format, the clang build with `-Wall -Wextra -Wpedantic -Werror` and
clang-tidy (every finding is an error), Semgrep (the factory's rules in `.semgrep/`), Catch2 tests
under llvm-cov with line coverage >= 80 % of the library, documentation manifest
(`docs-manifest.json`) plus Doxygen on the public headers (an undocumented declaration fails),
gitleaks, OSV over the upstream release of every vcpkg dependency, and OSV-Scanner over the
repository. It stops at the first failure.

## Folder map

| Path | Content |
|---|---|
| `include/trysac/lua_sandbox/` | The public headers (`*.hpp`): the public API. |
| `src/` | The implementation (`*.cpp`, private `*.hpp`). |
| `tests/` | Catch2 tests (`*_test.cpp`), property tests with Catch2 generators. |
| `tests/fixtures/` | Optional fixtures and golden files used by tests. |
| `scripts/` | The single definition of the quality gates (`check.cmake`) and the factory's tools. |
| `cmake/dependencies.cmake` | The CMake targets of each vcpkg dependency, written by the factory. |
| `docs/` | Architecture, decisions (ADRs), testing, security, project record, stack. |
| `fabrica.json` | Install and check commands used by the factory and CI. |
| `docs-manifest.json` | Documentation files the docs gate requires. |
| `.github/` | CI wrapper and Dependabot configuration. |

## Conventions

- C++20, formatted by clang-format (Google style, 120 columns), clean under the compiler with
  warnings as errors and clang-tidy.
- Standard library only unless an ADR in `docs/decisions/` justifies a dependency.
- Every public declaration has a Doxygen comment (`///`, with `@param`, `@return` and `@throws`);
  invalid input throws a documented `std::invalid_argument` subclass, never returns a sentinel.
- New source files need no build change: CMake globs `src/`, `include/` and `tests/`.
- Write the failing test first; tests use every literal of the acceptance items verbatim, and
  generated inputs (`GENERATE(take(n, random(...)))`) cover invariants.
- CI-only conformance tests (external tools or downloads) carry the Catch2 tag `[conformance]`;
  only CI runs them.
- Every behavior change updates `README.md` usage, `CHANGELOG.md` (Unreleased) and, if it is a
  design choice, a new ADR.
- Conventional Commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`).

## Limits

- Never push to `main`; work on a branch and open a pull request.
- Do NOT edit `.github/`, `cmake/`, `fabrica.json`, `docs-manifest.json`, `scripts/`,
  `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `Doxyfile`, `.clang-format`,
  `.clang-tidy`, `.editorconfig` or any tool configuration. They are the quality contract;
  changes to them are made by a human.
- Never lower the coverage threshold, add `NOLINT` to silence a lint, or disable a test to make a
  gate pass.
- Never commit secrets, tokens or `.env` files; gitleaks blocks them.
