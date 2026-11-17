# lua-sandbox

Embed Lua 5.5 in C++20 with enforced memory, instruction, call depth and output limits, a whitelisted standard library, text-only chunks and typed host function bindings.

## Problem

Applications that let users or plugins run scripts embed Lua because it is small, but a default state exposes io, os and load, a script can loop forever, allocate until the process dies or catch the error raised by a limit with pcall and keep going. Hand-rolled sandboxes often miss one of these security boundaries.

## Installation

Uses the vcpkg `lua` port (5.5). In your `CMakeLists.txt`, link against `trysac::lua_sandbox`.

## Usage

```cpp
#include <cstdint>
#include <span>
#include <variant>
#include "trysac/lua_sandbox/sandbox.hpp"

trysac::lua_sandbox::Sandbox sandbox;
sandbox.expose("add", [](std::span<const trysac::lua_sandbox::Value> args) -> trysac::lua_sandbox::Value {
  return std::get<std::int64_t>(args[0]) + std::get<std::int64_t>(args[1]);
});
auto result = sandbox.run("return add(2, 3)");
```

## API Summary

### Limits

Configurable via `struct Limits`:
- `memory_bytes` (default 16,777,216)
- `instructions` (default 10,000,000)
- `script_bytes` (default 1,048,576)
- `max_call_depth` (default 200)
- `output_bytes` (default 65,536)

### Errors & Results

Methods throw `SandboxError` (codes: `input_too_large`, `invalid_name`, `invalid_limits`) for invalid configuration. `Sandbox::run` returns `RunResult`, which contains a `Status` (`ok`, `syntax_error`, `runtime_error`, `memory_limit`, `instruction_limit`, `depth_limit`, `output_limit`) and relevant output details.

## Why not <best-known alternative>

Compared to raw embedding or other wrappers, this library provides limits that hold against `pcall` loops and a curated environment by default, combined with strictly typed host bindings.

See [docs/SECURITY.md](docs/SECURITY.md) for the threat model.