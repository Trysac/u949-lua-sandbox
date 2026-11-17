#include "trysac/lua_sandbox/sandbox.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

namespace sb = trysac::lua_sandbox;

TEST_CASE("Instruction limit stops infinite loop and cannot be swallowed by pcall", "[sandbox][limits]") {
  SECTION("Direct infinite loop with instructions = 1_000_000") {
    sb::Limits limits;
    limits.instructions = 1'000'000;
    sb::Sandbox sandbox(limits);

    const sb::RunResult res = sandbox.run("while true do end");
    CHECK(res.status == sb::Status::instruction_limit);
    CHECK(res.instructions_used >= 1'000'000);
    CHECK(res.instructions_used < 1'001'000);
  }

  SECTION("pcall loop trying to swallow instruction limit") {
    sb::Limits limits;
    limits.instructions = 200'000;
    sb::Sandbox sandbox(limits);

    const sb::RunResult res = sandbox.run("while true do pcall(function() while true do end end) end");
    CHECK(res.status == sb::Status::instruction_limit);
  }

  SECTION("Instruction limit under 1000") {
    sb::Limits limits;
    limits.instructions = 500;
    sb::Sandbox sandbox(limits);
    const sb::RunResult res = sandbox.run("while true do end");
    CHECK(res.status == sb::Status::instruction_limit);
  }
}

TEST_CASE("Memory limit caps allocation and peak_memory_bytes", "[sandbox][limits]") {
  sb::Limits limits;
  limits.memory_bytes = 4'194'304;
  sb::Sandbox sandbox(limits);

  SECTION("Incremental table growth") {
    const sb::RunResult res = sandbox.run("local t = {} for i = 1, 1e8 do t[i] = string.rep('x', 1024) end");
    CHECK(res.status == sb::Status::memory_limit);
    CHECK(res.peak_memory_bytes <= 4'194'304);
  }

  SECTION("Huge single allocation") {
    const sb::RunResult res = sandbox.run("return string.rep('x', 2^40)");
    CHECK(res.status == sb::Status::memory_limit);
    CHECK(res.peak_memory_bytes <= 4'194'304);
  }

  SECTION("Memory limit too small to even initialize state") {
    sb::Limits tiny_limits;
    tiny_limits.memory_bytes = 128;
    sb::Sandbox tiny_sandbox(tiny_limits);
    const sb::RunResult res = tiny_sandbox.run("return 1");
    CHECK(res.status == sb::Status::memory_limit);
    CHECK(res.peak_memory_bytes <= 128);
  }

  SECTION("Memory limit too small to finish init_sandbox_state") {
    sb::Limits small_limits;
    small_limits.memory_bytes = 8'192;
    sb::Sandbox small_sandbox(small_limits);
    const sb::RunResult res = small_sandbox.run("return 1");
    CHECK(res.status == sb::Status::memory_limit);
    CHECK(res.peak_memory_bytes <= 8'192);
  }
}

TEST_CASE("Environment whitelist, string metatable protection, and text-only load", "[sandbox][security]") {
  sb::Sandbox sandbox;

  SECTION("Forbidden modules and functions are nil") {
    const sb::RunResult res =
        sandbox.run("return os, io, package, debug, require, dofile, loadfile, collectgarbage, string.dump");
    REQUIRE(res.status == sb::Status::ok);
    REQUIRE(res.values.size() == 9);
    for (const auto& val : res.values) {
      CHECK(std::holds_alternative<std::monostate>(val));
    }
  }

  SECTION("String metatable cannot be retrieved or tampered with") {
    const sb::RunResult res = sandbox.run("return getmetatable('')");
    REQUIRE(res.status == sb::Status::ok);
    REQUIRE(res.values.size() == 1);
    REQUIRE(std::holds_alternative<bool>(res.values.at(0)));
    CHECK(std::get<bool>(res.values.at(0)) == false);
  }

  SECTION("Binary chunk starting with 0x1B is rejected by load and run") {
    const sb::RunResult res = sandbox.run("return load('\x1bLua')");
    REQUIRE(res.status == sb::Status::ok);
    REQUIRE(res.values.size() == 2);
    CHECK(std::holds_alternative<std::monostate>(res.values.at(0)));
    REQUIRE(std::holds_alternative<std::string>(res.values.at(1)));
    CHECK_THAT(std::get<std::string>(res.values.at(1)), Catch::Matchers::ContainsSubstring("binary"));

    const std::string binary_chunk("\x1bLua", 4);
    const sb::RunResult direct_res = sandbox.run(binary_chunk);
    CHECK(direct_res.status == sb::Status::syntax_error);
    CHECK_THAT(direct_res.message, Catch::Matchers::ContainsSubstring("binary"));
  }

  SECTION("load() enforces text mode, refuses reader functions, and uses sandbox env") {
    const sb::RunResult fn_load = sandbox.run("return load(function() return 'return 1' end)");
    REQUIRE(fn_load.status == sb::Status::ok);
    REQUIRE(fn_load.values.size() == 2);
    CHECK(std::holds_alternative<std::monostate>(fn_load.values.at(0)));

    const sb::RunResult mode_load = sandbox.run("return load('return 1', 'chunk', 'b')");
    REQUIRE(mode_load.status == sb::Status::ok);
    REQUIRE(mode_load.values.size() == 2);
    CHECK(std::holds_alternative<std::monostate>(mode_load.values.at(0)));

    const sb::RunResult ok_load = sandbox.run("local f = assert(load('return math.abs(-7), os')) return f()");
    REQUIRE(ok_load.status == sb::Status::ok);
    REQUIRE(ok_load.values.size() == 2);
    CHECK(std::get<std::int64_t>(ok_load.values.at(0)) == 7);
    CHECK(std::holds_alternative<std::monostate>(ok_load.values.at(1)));

    const sb::RunResult custom_env_load =
        sandbox.run("local f = assert(load('return x', 'c', 't', {x = 42})) return f()");
    REQUIRE(custom_env_load.status == sb::Status::ok);
    REQUIRE(custom_env_load.values.size() == 1);
    CHECK(std::get<std::int64_t>(custom_env_load.values.at(0)) == 42);
  }
}

TEST_CASE("Host function bindings and exception safety", "[sandbox][host]") {
  SECTION("Adding two integers and throwing from host function") {
    sb::Sandbox sandbox;
    sandbox.expose("add", [](std::span<const sb::Value> args) -> sb::Value {
      if (args.size() < 2) {
        throw std::runtime_error("missing args");
      }
      return std::get<std::int64_t>(args.front()) + std::get<std::int64_t>(args.back());
    });

    const sb::RunResult ok_res = sandbox.run("return add(2, 3)");
    REQUIRE(ok_res.status == sb::Status::ok);
    REQUIRE(ok_res.values.size() == 1);
    REQUIRE(std::holds_alternative<std::int64_t>(ok_res.values.at(0)));
    CHECK(std::get<std::int64_t>(ok_res.values.at(0)) == 5);

    sandbox.expose(
        "add", [](std::span<const sb::Value>) -> sb::Value { throw std::runtime_error("secret internal failure"); });

    const sb::RunResult err_res = sandbox.run("return add(2, 3)");
    CHECK(err_res.status == sb::Status::runtime_error);
    CHECK_THAT(err_res.message, Catch::Matchers::ContainsSubstring("host function 'add' failed"));
  }

  SECTION("All Value variant types round-trip and unsupported types fail cleanly") {
    sb::Sandbox sandbox;
    sandbox.expose("echo", [](std::span<const sb::Value> args) -> sb::Value {
      if (args.empty()) {
        return std::monostate{};
      }
      return args.front();
    });

    const sb::RunResult res = sandbox.run("return echo(nil), echo(true), echo(3.5), echo('hi'), {1}, function() end");
    REQUIRE(res.status == sb::Status::ok);
    REQUIRE(res.values.size() == 6);
    CHECK(std::holds_alternative<std::monostate>(res.values.at(0)));
    CHECK(std::get<bool>(res.values.at(1)) == true);
    CHECK(std::get<double>(res.values.at(2)) == 3.5);
    CHECK(std::get<std::string>(res.values.at(3)) == "hi");
    CHECK(std::get<std::string>(res.values.at(4)).starts_with("table:"));
    CHECK(std::get<std::string>(res.values.at(5)).starts_with("function:"));

    const sb::RunResult bad_arg = sandbox.run("return echo({})");
    CHECK(bad_arg.status == sb::Status::runtime_error);
    CHECK_THAT(bad_arg.message, Catch::Matchers::ContainsSubstring("unsupported argument type"));
  }
}

TEST_CASE("Call depth limit, syntax error, and runtime error", "[sandbox][errors]") {
  sb::Limits limits;
  limits.max_call_depth = 200;
  sb::Sandbox sandbox(limits);

  SECTION("Deep non-tail recursion trips depth_limit") {
    const sb::RunResult res = sandbox.run("local function f() return 1 + f() end return f()");
    CHECK(res.status == sb::Status::depth_limit);
  }

  SECTION("Syntax error returns Status::syntax_error") {
    const sb::RunResult res = sandbox.run("return (");
    CHECK(res.status == sb::Status::syntax_error);
  }

  SECTION("Runtime error returns Status::runtime_error ending in boom") {
    const sb::RunResult res = sandbox.run("error('boom')");
    CHECK(res.status == sb::Status::runtime_error);
    CHECK_THAT(res.message, Catch::Matchers::EndsWith("boom"));
  }

  SECTION("Long runtime error message is capped at 200 characters") {
    const sb::RunResult res = sandbox.run("error(string.rep('a', 500))");
    CHECK(res.status == sb::Status::runtime_error);
    CHECK(res.message.size() <= 200);
  }
}

TEST_CASE("Print formatting and output_limit truncation", "[sandbox][output]") {
  sb::Limits limits;
  limits.output_bytes = 65'536;
  sb::Sandbox sandbox(limits);

  SECTION("print('a', 1) appends tab-separated line") {
    const sb::RunResult res = sandbox.run("print(\"a\", 1)");
    CHECK(res.status == sb::Status::ok);
    CHECK(res.output == "a\t1\n");
  }

  SECTION("Exceeding output_bytes 65_536 gives Status::output_limit and truncates") {
    const sb::RunResult res = sandbox.run("for i = 1, 1000 do print(string.rep('z', 100)) end");
    CHECK(res.status == sb::Status::output_limit);
    CHECK(res.output.size() == 65'536);
  }
}

TEST_CASE("Input validation, identifier validation, and state isolation", "[sandbox][validation]") {
  SECTION("Chunk of 1_048_577 bytes throws input_too_large") {
    sb::Sandbox sandbox;
    const std::string huge(1'048'577, ' ');
    try {
      static_cast<void>(sandbox.run(huge));
      FAIL("Expected SandboxError");
    } catch (const sb::SandboxError& err) {
      CHECK(err.code() == "input_too_large");
    }
  }

  SECTION("Invalid host function names or shadowing throws invalid_name") {
    sb::Sandbox sandbox;
    const auto dummy = [](std::span<const sb::Value>) -> sb::Value { return std::monostate{}; };

    const std::string bad_name =
        GENERATE(std::string{"os"}, std::string{"print"}, std::string{"load"}, std::string{"utf8"},
                 std::string{"xpcall"}, std::string{""}, std::string{"1abc"}, std::string{"a-b"}, std::string(65, 'a'));
    try {
      sandbox.expose(bad_name, dummy);
      FAIL("Expected SandboxError for " << bad_name);
    } catch (const sb::SandboxError& err) {
      CHECK(err.code() == "invalid_name");
    }
  }

  SECTION("Invalid limits throw invalid_limits") {
    sb::Limits bad;
    bad.memory_bytes = 0;
    try {
      const sb::Sandbox s(bad);
      FAIL("Expected SandboxError");
    } catch (const sb::SandboxError& err) {
      CHECK(err.code() == "invalid_limits");
    }
  }

  SECTION("Two runs on one Sandbox never share globals") {
    sb::Sandbox sandbox;
    const sb::RunResult r1 = sandbox.run("shared_secret = 99 return shared_secret");
    REQUIRE(r1.status == sb::Status::ok);
    REQUIRE(r1.values.size() == 1);
    CHECK(std::get<std::int64_t>(r1.values.at(0)) == 99);

    const sb::RunResult r2 = sandbox.run("return shared_secret");
    REQUIRE(r2.status == sb::Status::ok);
    REQUIRE(r2.values.size() == 1);
    CHECK(std::holds_alternative<std::monostate>(r2.values.at(0)));
  }
}

TEST_CASE("Property: arithmetic evaluation matches C++ across random integers", "[sandbox][property]") {
  const auto a = GENERATE(take(10, random(-10'000, 10'000)));
  const auto b = GENERATE(take(5, random(-10'000, 10'000)));
  sb::Sandbox sandbox;
  const std::string script = "return " + std::to_string(a) + " + " + std::to_string(b);
  const sb::RunResult res = sandbox.run(script);
  REQUIRE(res.status == sb::Status::ok);
  REQUIRE(res.values.size() == 1);
  CHECK(std::get<std::int64_t>(res.values.at(0)) == static_cast<std::int64_t>(a) + static_cast<std::int64_t>(b));
}
