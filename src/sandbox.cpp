#include "trysac/lua_sandbox/sandbox.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <lua.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace trysac::lua_sandbox {

namespace {

constexpr std::size_t kMaxMessageLength = 200;
constexpr int kHookInterval = 1'000;
constexpr int kMaxReturnValues = 16;
constexpr unsigned int kLuaSeed = 0x5A17D803U;

constexpr std::array<std::string_view, 32> kReservedNames = {
    "_G",           "_VERSION", "assert",       "collectgarbage", "coroutine", "debug",    "dofile",   "error",
    "getmetatable", "io",       "ipairs",       "load",           "loadfile",  "math",     "next",     "os",
    "package",      "pairs",    "pcall",        "print",          "rawequal",  "rawget",   "rawlen",   "rawset",
    "require",      "select",   "setmetatable", "string",         "table",     "tonumber", "tostring", "type",
};

constexpr std::array<std::string_view, 17> kAllowedBaseGlobals = {
    "assert", "error",  "ipairs", "next",     "pairs",    "pcall", "xpcall",       "rawequal",     "rawget",
    "rawlen", "rawset", "select", "tonumber", "tostring", "type",  "setmetatable", "getmetatable",
};

[[noreturn]] void raise_lua_error(lua_State* state, const char* msg) {
  lua_pushstring(state, msg);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  lua_error(state);
  std::abort();
}

std::string truncate_message(std::string_view msg) {
  if (msg.size() > kMaxMessageLength) {
    return std::string(msg.substr(0, kMaxMessageLength));
  }
  return std::string(msg);
}

bool is_ascii_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }

bool is_valid_identifier(std::string_view name) {
  if (name.empty() || name.size() > 64) {
    return false;
  }
  if (!is_ascii_alpha(name.front()) && name.front() != '_') {
    return false;
  }
  for (const char c : name.substr(1)) {
    if (!is_ascii_alpha(c) && !is_ascii_digit(c) && c != '_') {
      return false;
    }
  }
  if (name == "utf8" || name == "xpcall") {
    return false;
  }
  return std::ranges::find(kReservedNames, name) == kReservedNames.end();
}

struct RunContext {
  Limits limits;
  std::size_t current_memory = 0;
  std::size_t peak_memory = 0;
  std::uint64_t instructions_used = 0;
  int call_depth = 0;
  std::string output;
  bool memory_tripped = false;
  bool instruction_tripped = false;
  bool depth_tripped = false;
  bool output_tripped = false;
  int env_ref = LUA_NOREF;
};

bool any_limit_tripped(const RunContext& ctx) {
  return ctx.memory_tripped || ctx.instruction_tripped || ctx.depth_tripped || ctx.output_tripped;
}

const char* tripped_message(const RunContext& ctx) {
  if (ctx.memory_tripped) {
    return "memory limit exceeded";
  }
  if (ctx.instruction_tripped) {
    return "instruction limit exceeded";
  }
  if (ctx.depth_tripped) {
    return "call depth limit exceeded";
  }
  return "output limit exceeded";
}

// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
void* sandbox_alloc(void* ud, void* ptr, std::size_t osize, std::size_t nsize) noexcept {
  auto* ctx = static_cast<RunContext*>(ud);
  const std::size_t old_bytes = (ptr == nullptr) ? 0 : osize;

  if (nsize == 0) {
    if (ptr != nullptr) {
      if (ctx->current_memory >= old_bytes) {
        ctx->current_memory -= old_bytes;
      } else {
        ctx->current_memory = 0;
      }
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
      std::free(ptr);
    }
    return nullptr;
  }

  if (nsize > old_bytes) {
    const std::size_t delta = nsize - old_bytes;
    if (delta > ctx->limits.memory_bytes || ctx->current_memory + delta > ctx->limits.memory_bytes) {
      ctx->memory_tripped = true;
      return nullptr;
    }
  }

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  void* new_ptr = std::realloc(ptr, nsize);
  if (new_ptr == nullptr) {
    ctx->memory_tripped = true;
    return nullptr;
  }

  if (nsize >= old_bytes) {
    ctx->current_memory += (nsize - old_bytes);
  } else {
    const std::size_t shrink = old_bytes - nsize;
    ctx->current_memory = (ctx->current_memory >= shrink) ? (ctx->current_memory - shrink) : 0;
  }
  ctx->peak_memory = std::max(ctx->current_memory, ctx->peak_memory);
  return new_ptr;
}

// NOLINTNEXTLINE(misc-const-correctness)
void arm_single_step_trap(lua_State* state) noexcept;

// NOLINTNEXTLINE(misc-const-correctness)
void sandbox_hook(lua_State* state, lua_Debug* ar) {
  void* ud = nullptr;
  lua_getallocf(state, &ud);
  auto* ctx = static_cast<RunContext*>(ud);
  const int event = ar->event;

  if (any_limit_tripped(*ctx)) {
    if (event == LUA_HOOKCOUNT) {
      ctx->instructions_used += 1;
    }
    arm_single_step_trap(state);
    raise_lua_error(state, tripped_message(*ctx));
  }

  if (event == LUA_HOOKCOUNT) {
    ctx->instructions_used += static_cast<std::uint64_t>(kHookInterval);
    if (ctx->instructions_used >= ctx->limits.instructions) {
      ctx->instruction_tripped = true;
      arm_single_step_trap(state);
      raise_lua_error(state, "instruction limit exceeded");
    }
  } else if (event == LUA_HOOKCALL) {
    ctx->call_depth += 1;
    if (ctx->call_depth > ctx->limits.max_call_depth) {
      ctx->depth_tripped = true;
      arm_single_step_trap(state);
      raise_lua_error(state, "call depth limit exceeded");
    }
  } else if (event == LUA_HOOKRET) {
    if (ctx->call_depth > 0) {
      ctx->call_depth -= 1;
    }
  }
}

// NOLINTNEXTLINE(misc-const-correctness)
void arm_single_step_trap(lua_State* state) noexcept {
  lua_sethook(state, &sandbox_hook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

int sandbox_print(lua_State* state) {
  void* ud = nullptr;
  lua_getallocf(state, &ud);
  auto* ctx = static_cast<RunContext*>(ud);

  if (any_limit_tripped(*ctx)) {
    arm_single_step_trap(state);
    raise_lua_error(state, tripped_message(*ctx));
  }

  const int nargs = lua_gettop(state);
  bool overflow = false;
  {
    std::string line;
    for (int i = 1; i <= nargs; ++i) {
      std::size_t len = 0;
      const char* str = luaL_tolstring(state, i, &len);
      if (str == nullptr) {
        raise_lua_error(state, "print conversion failed");
      }
      if (i > 1) {
        line.push_back('\t');
      }
      line.append(str, len);
      lua_pop(state, 1);
    }
    line.push_back('\n');

    const std::size_t remaining =
        (ctx->limits.output_bytes > ctx->output.size()) ? (ctx->limits.output_bytes - ctx->output.size()) : 0;
    if (line.size() > remaining) {
      ctx->output.append(line.data(), remaining);
      ctx->output_tripped = true;
      overflow = true;
    } else {
      ctx->output.append(line);
    }
  }

  if (overflow) {
    arm_single_step_trap(state);
    raise_lua_error(state, "output limit exceeded");
  }
  return 0;
}

int sandbox_load(lua_State* state) {
  void* ud = nullptr;
  lua_getallocf(state, &ud);
  const auto* ctx = static_cast<const RunContext*>(ud);

  if (any_limit_tripped(*ctx)) {
    arm_single_step_trap(state);
    raise_lua_error(state, tripped_message(*ctx));
  }

  const int chunk_type = lua_type(state, 1);
  if (chunk_type != LUA_TSTRING && chunk_type != LUA_TNUMBER) {
    luaL_pushfail(state);
    lua_pushliteral(state, "load of function chunks is not permitted in sandbox");
    return 2;
  }

  std::size_t chunk_len = 0;
  const char* chunk_str = lua_tolstring(state, 1, &chunk_len);
  if (chunk_len > ctx->limits.script_bytes) {
    luaL_pushfail(state);
    lua_pushliteral(state, "chunk exceeds script_bytes limit");
    return 2;
  }

  const char* chunk_name = luaL_optstring(state, 2, chunk_str);
  const char* mode = luaL_optstring(state, 3, "t");
  if (std::string_view(mode) != "t") {
    luaL_pushfail(state);
    lua_pushliteral(state, "only text mode ('t') is permitted in sandbox");
    return 2;
  }

  const int status = luaL_loadbufferx(state, chunk_str, chunk_len, chunk_name, "t");
  if (status != LUA_OK) {
    luaL_pushfail(state);
    lua_insert(state, -2);
    return 2;
  }

  if (!lua_isnoneornil(state, 4)) {
    lua_pushvalue(state, 4);
  } else {
    lua_rawgeti(state, LUA_REGISTRYINDEX, ctx->env_ref);
  }
  const char* upvalue_name = lua_setupvalue(state, -2, 1);
  if (upvalue_name == nullptr) {
    lua_pop(state, 1);
  }
  return 1;
}

struct HostBinding {
  std::string name;
  HostFunction fn;
};

int host_trampoline(lua_State* state) {
  void* ud = nullptr;
  lua_getallocf(state, &ud);
  const auto* ctx = static_cast<const RunContext*>(ud);

  if (any_limit_tripped(*ctx)) {
    arm_single_step_trap(state);
    raise_lua_error(state, tripped_message(*ctx));
  }

  const auto* binding = static_cast<const HostBinding*>(lua_touserdata(state, lua_upvalueindex(1)));
  const int nargs = lua_gettop(state);

  enum class TrampolineOutcome : std::uint8_t { ok, bad_arg, host_failed };
  TrampolineOutcome outcome = TrampolineOutcome::ok;
  std::string error_msg;

  int pushed = 0;
  {
    std::vector<Value> args;
    try {
      args.reserve(static_cast<std::size_t>(nargs));
      for (int i = 1; i <= nargs; ++i) {
        const int t = lua_type(state, i);
        if (t == LUA_TNIL) {
          args.emplace_back(std::monostate{});
        } else if (t == LUA_TBOOLEAN) {
          args.emplace_back(lua_toboolean(state, i) != 0);
        } else if (t == LUA_TNUMBER) {
          if (lua_isinteger(state, i) != 0) {
            args.emplace_back(static_cast<std::int64_t>(lua_tointeger(state, i)));
          } else {
            args.emplace_back(static_cast<double>(lua_tonumber(state, i)));
          }
        } else if (t == LUA_TSTRING) {
          std::size_t len = 0;
          const char* s = lua_tolstring(state, i, &len);
          args.emplace_back(std::string(s, len));
        } else {
          outcome = TrampolineOutcome::bad_arg;
          break;
        }
      }

      if (outcome == TrampolineOutcome::ok) {
        Value ret = binding->fn(std::span<const Value>(args));
        if (std::holds_alternative<std::monostate>(ret)) {
          lua_pushnil(state);
          pushed = 1;
        } else if (const auto* b = std::get_if<bool>(&ret)) {
          lua_pushboolean(state, *b ? 1 : 0);
          pushed = 1;
        } else if (const auto* i = std::get_if<std::int64_t>(&ret)) {
          lua_pushinteger(state, static_cast<lua_Integer>(*i));
          pushed = 1;
        } else if (const auto* d = std::get_if<double>(&ret)) {
          lua_pushnumber(state, static_cast<lua_Number>(*d));
          pushed = 1;
        } else if (const auto* s = std::get_if<std::string>(&ret)) {
          lua_pushlstring(state, s->data(), s->size());
          pushed = 1;
        }
      }
    } catch (...) {
      outcome = TrampolineOutcome::host_failed;
      error_msg = "host function '" + binding->name + "' failed";
    }
  }

  if (outcome == TrampolineOutcome::bad_arg) {
    raise_lua_error(state, "unsupported argument type");
  }
  if (outcome == TrampolineOutcome::host_failed) {
    raise_lua_error(state, error_msg.c_str());
  }
  return pushed;
}

void copy_library_table(lua_State* state, int env_idx, const char* name, lua_CFunction openf, bool strip_dump) {
  luaL_requiref(state, name, openf, 0);
  lua_newtable(state);
  lua_pushnil(state);
  while (lua_next(state, -3) != 0) {
    if (strip_dump && lua_type(state, -2) == LUA_TSTRING) {
      const char* key = lua_tostring(state, -2);
      if (key != nullptr && std::string_view(key) == "dump") {
        lua_pop(state, 1);
        continue;
      }
    }
    lua_pushvalue(state, -2);
    lua_insert(state, -2);
    lua_rawset(state, -4);
  }
  lua_setfield(state, env_idx, name);
  lua_pop(state, 1);
}

int init_sandbox_state(lua_State* state) {
  void* ud = nullptr;
  lua_getallocf(state, &ud);
  auto* ctx = static_cast<RunContext*>(ud);
  auto* bindings = static_cast<std::vector<HostBinding>*>(lua_touserdata(state, 1));

  luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
  lua_pop(state, 1);

  lua_newtable(state);
  const int env_idx = lua_gettop(state);

  lua_pushglobaltable(state);
  const int base_idx = lua_gettop(state);

  for (const std::string_view key : kAllowedBaseGlobals) {
    lua_pushlstring(state, key.data(), key.size());
    lua_rawget(state, base_idx);
    lua_pushlstring(state, key.data(), key.size());
    lua_insert(state, -2);
    lua_rawset(state, env_idx);
  }
  lua_pop(state, 1);

  copy_library_table(state, env_idx, LUA_TABLIBNAME, luaopen_table, false);
  copy_library_table(state, env_idx, LUA_STRLIBNAME, luaopen_string, true);
  copy_library_table(state, env_idx, LUA_MATHLIBNAME, luaopen_math, false);
  copy_library_table(state, env_idx, LUA_UTF8LIBNAME, luaopen_utf8, false);

  lua_pushliteral(state, "");
  if (lua_getmetatable(state, -1) != 0) {
    lua_pushboolean(state, 0);
    lua_setfield(state, -2, "__metatable");
    lua_pop(state, 1);
  }
  lua_pop(state, 1);

  lua_pushcfunction(state, &sandbox_print);
  lua_setfield(state, env_idx, "print");

  lua_pushcfunction(state, &sandbox_load);
  lua_setfield(state, env_idx, "load");

  lua_pushvalue(state, env_idx);
  lua_setfield(state, env_idx, "_G");

  for (auto& binding : *bindings) {
    lua_pushlightuserdata(state, &binding);
    lua_pushcclosure(state, &host_trampoline, 1);
    lua_setfield(state, env_idx, binding.name.c_str());
  }

  ctx->env_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  return 0;
}

Value lua_to_value(lua_State* state, int idx) {
  const int t = lua_type(state, idx);
  if (t == LUA_TNIL || t == LUA_TNONE) {
    return std::monostate{};
  }
  if (t == LUA_TBOOLEAN) {
    return lua_toboolean(state, idx) != 0;
  }
  if (t == LUA_TNUMBER) {
    if (lua_isinteger(state, idx) != 0) {
      return static_cast<std::int64_t>(lua_tointeger(state, idx));
    }
    return static_cast<double>(lua_tonumber(state, idx));
  }
  if (t == LUA_TSTRING) {
    std::size_t len = 0;
    const char* s = lua_tolstring(state, idx, &len);
    return std::string(s, len);
  }
  std::size_t len = 0;
  const char* s = luaL_tolstring(state, idx, &len);
  std::string out(s != nullptr ? s : "", len);
  lua_pop(state, 1);
  return out;
}

int extract_return_values(lua_State* state) {
  auto* out = static_cast<std::vector<Value>*>(lua_touserdata(state, 1));
  const int count = lua_tointeger(state, 2);
  for (int i = 1; i <= count; ++i) {
    out->push_back(lua_to_value(state, 2 + i));
  }
  return 0;
}

}  // namespace

SandboxError::SandboxError(std::string code, const std::string& message)
    : std::invalid_argument(truncate_message(message)), code_(std::move(code)) {}

std::string_view SandboxError::code() const noexcept { return code_; }

Sandbox::Sandbox(Limits limits) : limits_(limits) {
  if (limits_.memory_bytes == 0 || limits_.instructions == 0 || limits_.script_bytes == 0 ||
      limits_.max_call_depth <= 0 || limits_.output_bytes == 0) {
    throw SandboxError("invalid_limits", "all sandbox limits must be strictly positive");
  }
}

void Sandbox::expose(std::string name, HostFunction fn) {
  if (!is_valid_identifier(name) || !fn) {
    throw SandboxError("invalid_name", "host function name is invalid or shadows a sandbox built-in");
  }
  for (auto& existing : host_functions_) {
    if (existing.first == name) {
      existing.second = std::move(fn);
      return;
    }
  }
  host_functions_.emplace_back(std::move(name), std::move(fn));
}

RunResult Sandbox::run(std::string_view chunk, std::string_view chunk_name) {
  if (chunk.size() > limits_.script_bytes) {
    throw SandboxError("input_too_large", "script chunk size exceeds script_bytes limit");
  }

  RunContext ctx;
  ctx.limits = limits_;

  std::vector<HostBinding> bindings;
  bindings.reserve(host_functions_.size());
  for (const auto& [name, fn] : host_functions_) {
    bindings.push_back(HostBinding{.name = name, .fn = fn});
  }

  lua_State* state = lua_newstate(&sandbox_alloc, &ctx, kLuaSeed);
  if (state == nullptr) {
    RunResult res;
    res.status = Status::memory_limit;
    res.message = "memory limit exceeded";
    res.peak_memory_bytes = ctx.peak_memory;
    return res;
  }

  lua_pushcfunction(state, &init_sandbox_state);
  lua_pushlightuserdata(state, &bindings);
  if (lua_pcall(state, 1, 0, 0) != LUA_OK) {
    RunResult res;
    res.status = ctx.memory_tripped ? Status::memory_limit : Status::runtime_error;
    const char* err = lua_tostring(state, -1);
    res.message = truncate_message(err != nullptr ? err : "sandbox initialization failed");
    res.peak_memory_bytes = ctx.peak_memory;
    lua_close(state);
    return res;
  }

  std::string formatted_name;
  formatted_name.reserve(chunk_name.size() + 1);
  formatted_name.push_back('=');
  formatted_name.append(chunk_name);

  const int load_status = luaL_loadbufferx(state, chunk.data(), chunk.size(), formatted_name.c_str(), "t");
  if (load_status != LUA_OK) {
    RunResult res;
    if (ctx.memory_tripped || load_status == LUA_ERRMEM) {
      res.status = Status::memory_limit;
      res.message = "memory limit exceeded";
    } else {
      res.status = Status::syntax_error;
      const char* err = lua_tostring(state, -1);
      res.message = truncate_message(err != nullptr ? err : "syntax error");
    }
    res.peak_memory_bytes = ctx.peak_memory;
    lua_close(state);
    return res;
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, ctx.env_ref);
  if (lua_setupvalue(state, -2, 1) == nullptr) {
    lua_pop(state, 1);
  }

  const int initial_step = (limits_.instructions < static_cast<std::uint64_t>(kHookInterval))
                               ? static_cast<int>(limits_.instructions)
                               : kHookInterval;
  lua_sethook(state, &sandbox_hook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, initial_step);

  const int base_top = lua_gettop(state) - 1;
  const int call_status = lua_pcall(state, 0, LUA_MULTRET, 0);
  lua_sethook(state, nullptr, 0, 0);

  RunResult result;
  result.instructions_used = ctx.instructions_used;
  result.peak_memory_bytes = ctx.peak_memory;
  result.output = std::move(ctx.output);

  if (ctx.memory_tripped || call_status == LUA_ERRMEM) {
    result.status = Status::memory_limit;
    result.message = "memory limit exceeded";
  } else if (ctx.instruction_tripped) {
    result.status = Status::instruction_limit;
    result.message = "instruction limit exceeded";
  } else if (ctx.depth_tripped) {
    result.status = Status::depth_limit;
    result.message = "call depth limit exceeded";
  } else if (ctx.output_tripped) {
    result.status = Status::output_limit;
    result.message = "output limit exceeded";
  } else if (call_status != LUA_OK) {
    result.status = Status::runtime_error;
    const char* err = lua_tostring(state, -1);
    result.message = truncate_message(err != nullptr ? err : "runtime error");
  } else {
    result.status = Status::ok;
    const int nresults = std::min(lua_gettop(state) - base_top, kMaxReturnValues);
    if (nresults > 0) {
      result.values.reserve(static_cast<std::size_t>(nresults));
      lua_pushcfunction(state, &extract_return_values);
      lua_pushlightuserdata(state, &result.values);
      lua_pushinteger(state, nresults);
      for (int i = 1; i <= nresults; ++i) {
        lua_pushvalue(state, base_top + i);
      }
      if (lua_pcall(state, 2 + nresults, 0, 0) != LUA_OK) {
        result.values.clear();
        if (ctx.memory_tripped) {
          result.status = Status::memory_limit;
          result.message = "memory limit exceeded";
        } else {
          result.status = Status::runtime_error;
          const char* err = lua_tostring(state, -1);
          result.message = truncate_message(err != nullptr ? err : "return value conversion failed");
        }
      }
    }
  }

  result.peak_memory_bytes = ctx.peak_memory;
  lua_close(state);
  return result;
}

}  // namespace trysac::lua_sandbox
