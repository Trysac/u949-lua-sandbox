#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <string>

#include "trysac/lua_sandbox/text.hpp"

namespace lib = trysac::lua_sandbox;

TEST_CASE("normalize trims and lower-cases", "[text]") {
  const auto [input, want] = GENERATE(table<std::string, std::string>({
      {"  Hello World ", "hello world"},
      {"ABC", "abc"},
      {"\tx\n", "x"},
  }));
  CHECK(lib::normalize(input) == want);
}

TEST_CASE("normalize rejects blank input", "[text]") {
  const std::string input = GENERATE(std::string{}, std::string{"   "}, std::string{"\t\n"});
  CHECK_THROWS_MATCHES(static_cast<void>(lib::normalize(input)), lib::BlankInputError,
                       Catch::Matchers::Message("input is blank"));
}
