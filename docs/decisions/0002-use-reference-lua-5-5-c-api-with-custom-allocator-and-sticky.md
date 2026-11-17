# Use reference Lua 5.5 C API with custom allocator and sticky VM hooks

Status: accepted (generated, pending human review)

## Context

Applications embedding Lua for user scripts need deterministic caps on memory, instructions, call depth, and output, as well as protection against pcall loops swallowing limit errors and C longjmp unwinding past C++ destructors.

## Decision

Depend on the vcpkg lua 5.5 C interpreter and wrap it directly via the Lua 5.5 C API in a fresh lua_State per run, using a custom lua_Alloc for hard memory caps, sticky single-stepping lua_Hook flags to defeat pcall loops, and inner-scoped C++ frames inside C trampolines before raising lua_error.

## Consequences

Every script run is completely isolated and bounded in memory, CPU instructions, stack depth, and output without leaking C++ resources on Lua errors, at the cost of creating a fresh state per execution.
