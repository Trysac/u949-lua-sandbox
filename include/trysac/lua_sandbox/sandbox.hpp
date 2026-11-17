/// @file
/// @brief Sandboxed Lua 5.5 execution engine with memory, instruction, call depth, and output limits.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// @brief The public API of the lua-sandbox library.
namespace trysac::lua_sandbox {

/// @brief Resource and execution limits for a sandboxed Lua run.
struct Limits {
  /// @brief Maximum live memory in bytes that the Lua allocator may allocate (default 16 MiB).
  std::size_t memory_bytes = 16'777'216;
  /// @brief Maximum VM instructions before execution is terminated (default 10,000,000).
  std::uint64_t instructions = 10'000'000;
  /// @brief Maximum input script size in bytes accepted by Sandbox::run (default 1 MiB).
  std::size_t script_bytes = 1'048'576;
  /// @brief Maximum Lua call stack depth before execution is terminated (default 200).
  int max_call_depth = 200;
  /// @brief Maximum bytes captured from print() before execution is terminated (default 64 KiB).
  std::size_t output_bytes = 65'536;
};

/// @brief A typed Lua value exchanged between the host and sandboxed scripts.
using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

/// @brief Signature of a host callback exposed to sandboxed Lua scripts.
using HostFunction = std::function<Value(std::span<const Value>)>;

/// @brief Outcome status of a sandboxed script execution.
enum class Status : std::uint8_t {
  /// @brief The script completed normally.
  ok,
  /// @brief The script could not be parsed or was rejected as binary bytecode.
  syntax_error,
  /// @brief The script raised an uncaught runtime error or a host function failed.
  runtime_error,
  /// @brief The script exceeded Limits::memory_bytes.
  memory_limit,
  /// @brief The script exceeded Limits::instructions.
  instruction_limit,
  /// @brief The script exceeded Limits::max_call_depth.
  depth_limit,
  /// @brief The script exceeded Limits::output_bytes.
  output_limit,
};

/// @brief Result returned by Sandbox::run.
struct RunResult {
  /// @brief Execution outcome status.
  Status status = Status::ok;
  /// @brief Up to 16 return values from the script.
  std::vector<Value> values;
  /// @brief Diagnostic message on error or limit violation (at most 200 characters).
  std::string message;
  /// @brief Approximate number of VM instructions executed.
  std::uint64_t instructions_used = 0;
  /// @brief Peak live memory in bytes allocated by the Lua state.
  std::size_t peak_memory_bytes = 0;
  /// @brief Captured output from print() calls.
  std::string output;
};

/// @brief Error thrown when Sandbox configuration or arguments are invalid.
class SandboxError : public std::invalid_argument {
 public:
  /// @brief Constructs a SandboxError with a stable machine-readable code and message.
  /// @param code The error code ('input_too_large', 'invalid_name', or 'invalid_limits').
  /// @param message A human-readable description (truncated to 200 characters).
  SandboxError(std::string code, const std::string& message);

  /// @brief Returns the machine-readable error code.
  /// @return The error code string view.
  [[nodiscard]] std::string_view code() const noexcept;

 private:
  std::string code_;
};

/// @brief Isolated Lua 5.5 execution sandbox.
class Sandbox {
 public:
  /// @brief Creates a Sandbox with the specified resource limits.
  /// @param limits The resource limits to enforce on every run.
  /// @throws SandboxError with code `invalid_limits` if any limit is zero or negative.
  explicit Sandbox(Limits limits = {});

  /// @brief Registers a host function in the global environment of subsequent runs.
  /// @param name Global identifier matching `[A-Za-z_][A-Za-z0-9_]{0,63}` and not shadowing any built-in.
  /// @param fn The host callback to invoke when Lua calls `name`.
  /// @throws SandboxError with code `invalid_name` if `name` is invalid, reserved, or fn is empty.
  void expose(std::string name, HostFunction fn);

  /// @brief Executes a Lua text chunk in a fresh, isolated Lua state.
  /// @param chunk The Lua source code to execute (text mode only).
  /// @param chunk_name The name reported in stack traces and error messages.
  /// @return A RunResult containing the status, return values, metrics, and captured output.
  /// @throws SandboxError with code `input_too_large` if `chunk.size() > limits.script_bytes`.
  [[nodiscard]] RunResult run(std::string_view chunk, std::string_view chunk_name = "script");

 private:
  Limits limits_;
  std::vector<std::pair<std::string, HostFunction>> host_functions_;
};

}  // namespace trysac::lua_sandbox
