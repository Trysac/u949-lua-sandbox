# Project record

## Objective

Embed Lua 5.5 with enforced memory, instruction, call depth and output limits, a whitelisted standard library, text-only chunks and typed host function bindings.

## Scope

- The library public API (`Sandbox`, `Limits`, `Value`, `HostFunction`, `RunResult`, `Status`, `SandboxError`).
- Enforced resource limits (memory, instructions, call depth, output size).
- Whitelisted global environment and string metatable protection.
- Typed C++-to-Lua bindings.

## Out of Scope

- Coroutines.
- LuaJIT compatibility.
- Sharing state across runs.
- Exposing complex C++ classes or tables directly.
- Lua 5.4 compatibility.

## Effort metrics

| Metric | Value |
|---|---|
| Tokens (input / output) | 3512698 / 32450 |
| Hours (human) | pending measurement |
| Cost (USD) | 1.0494 |

## Factory run u949 (measured from API usage)

| Date (UTC) | Phase | Model | Batch | Input | Output | Cache write | Cache read | Cost (USD) |
|---|---|---|---|---|---|---|---|---|
| 2026-11-17 | generation | google/gemini-4-argon | yes | 7011 | 14944 | 0 | 0 | 0.3269 |
| 2026-11-17 | fix-1 | google/gemini-3.8-flash | no | 155818 | 4774 | 0 | 738859 | 0.1902 |
| 2026-11-17 | fix-2 | google/gemini-3.8-flash | no | 113717 | 5839 | 0 | 815481 | 0.1683 |
| 2026-11-17 | fix-3 | google/gemini-3.8-flash | no | 126842 | 3327 | 0 | 803930 | 0.1679 |
| 2026-11-17 | fix-4 | google/gemini-3.8-flash | no | 40097 | 677 | 0 | 181417 | 0.0462 |
| 2026-11-17 | audit | google/gemini-3.8-flash | no | 70322 | 503 | 0 | 179425 | 0.0681 |
| 2026-11-17 | threat-model | google/gemini-3.8-flash | no | 78121 | 528 | 0 | 183222 | 0.0743 |
| 2026-11-17 | docs | google/gemini-3.1-flash-lite | no | 18436 | 1858 | 0 | 0 | 0.0074 |

Total: $1.0494. Prices: vertex line, as measured by its adapter. Providers serve the current version of each model id, so the date tells which version answered; versions, launch stages and price sources per line are in the factory's model register (docs/MODELS.md).
