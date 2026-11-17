# Architecture

## Purpose

`lua-sandbox` is a small, dependency-free C++20 library: Embed Lua 5.5 with enforced memory, instruction, call depth and output limits, a whitelisted standard library, text-only chunks and typed host function bindings

## Shape

```text
caller code --> include/trysac/lua_sandbox/*.hpp (namespace trysac::lua_sandbox)
                    |
                    +-- src/*.cpp: pure functions; no I/O, no global mutable state
                    +-- anonymous-namespace helpers and private headers under src/ (optional)
```

- **Public API**: every declaration of the public headers under `include/`. Code in `src/` and in
  anonymous namespaces may change without a major version.
- **Errors**: invalid input throws a `std::invalid_argument` subclass of the library (such as
  `BlankInputError`) documented with `@throws`. The library never prints or exits.
- **Side effects**: none. If a feature needs I/O, accept a `std::istream&`, a `std::span` or a small
  interface instead of opening resources, and record the choice in an ADR.

## Quality contract

`scripts/check.cmake` is the single definition of the gates; `fabrica.json` tells the factory and
CI how to run it. See [TESTING.md](TESTING.md).

## Decisions

Architecture decisions are recorded in [decisions/](decisions/) as numbered ADRs.
