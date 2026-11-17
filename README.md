# lua-sandbox

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/Trysac/u949-lua-sandbox/actions/workflows/ci.yml/badge.svg)](https://github.com/Trysac/u949-lua-sandbox/actions/workflows/ci.yml)

Embed Lua 5.5 with enforced memory, instruction, call depth and output limits, a whitelisted standard library, text-only chunks and typed host function bindings

## Problem

Describe in two or three sentences the concrete problem this utility solves, who hits it, and what
they do today without it. Replace this paragraph when the first real feature lands.

## Install

A C++20 library built with CMake. Until it is published to a vcpkg registry, add this repository
at a release tag as a subdirectory (or through `FetchContent`) and link its target:

```cmake
add_subdirectory(u949-lua-sandbox)
target_link_libraries(app PRIVATE trysac::lua_sandbox)
```

Requires a C++20 compiler and CMake 3.29 or newer. No dependencies outside the C++ standard
library.

## Usage

```cpp
#include <trysac/lua_sandbox/text.hpp>

// normalize trims surrounding whitespace and lower-cases the text.
auto name = trysac::lua_sandbox::normalize("  Hello World ");  // "hello world"

// Blank input is rejected explicitly instead of returning an empty string.
trysac::lua_sandbox::normalize("   ");  // throws BlankInputError ("input is blank")
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md) and [decisions](docs/decisions/)
- [Testing](docs/TESTING.md) and [security](docs/SECURITY.md)
- [Project record](docs/PROJECT.md) and [stack](docs/STACK.md)
- [Changelog](CHANGELOG.md) and [contributing guide](CONTRIBUTING.md)

## License

[MIT](LICENSE) (c) 2026 Miguel Gomez
