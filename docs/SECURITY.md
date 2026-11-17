# Security & Threat Model

## Defended Threats

- **CPU exhaustion**: Handled by `LUA_MASKCOUNT` hooks. Once the instruction limit triggers, the hook enters a sticky state, ensuring every subsequent VM instruction raises an error so `pcall` loops cannot continue.
- **Memory exhaustion**: A custom `lua_Alloc` tracks live bytes and refuses growth exceeding `Limits::memory_bytes`.
- **Deep recursion**: `LUA_MASKCALL` and `LUA_MASKRET` track call depth and terminate if `Limits::max_call_depth` is exceeded.
- **Sandbox breakouts**: `io`, `os`, `package`, `debug`, `require`, `dofile`, `loadfile`, `collectgarbage`, and `coroutine` are removed. The string metatable is locked (`__metatable = false`).
- **Bytecode loading**: `load()` and the main entry point force mode `"t"` and reject function readers, preventing arbitrary bytecode execution.
- **Output flooding**: Custom `print` implementation truncates output at `Limits::output_bytes`.
- **State leaking**: Each `run()` call creates and destroys a fresh `lua_State`.

## Not Defended

- **Blocking host functions**: If an exposed host function performs expensive I/O or CPU work, it will block the VM; wall-clock timeouts are the host's responsibility.
- **Hardware side channels**: No mitigations for timing or cache-based side-channel attacks.
- **Upstream Lua vulnerabilities**: Vulnerabilities within the base Lua 5.5 interpreter.