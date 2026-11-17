#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "trysac/lua_sandbox/text.hpp"

namespace lib = trysac::lua_sandbox;

namespace {

constexpr std::string_view kWhitespace = " \t\n\v\f\r";

// A string of printable ASCII and the whitespace characters normalize trims, from generated codes.
std::string text_of(const std::vector<int>& codes) {
  std::string out;
  for (const int code : codes) {
    out.push_back(code < 6 ? kWhitespace.at(static_cast<std::size_t>(code)) : static_cast<char>(code + 26));
  }
  return out;
}

bool is_blank(std::string_view text) { return text.find_first_not_of(kWhitespace) == std::string_view::npos; }

}  // namespace

// Property tests: Catch2 generators draw 200 random strings per property; the test run fixes the seed.
TEST_CASE("normalizing twice changes nothing", "[text][property]") {
  const auto input = text_of(GENERATE(take(200, chunk(12, random(0, 100)))));
  if (!is_blank(input)) {
    const auto once = lib::normalize(input);
    CHECK(lib::normalize(once) == once);
  }
}

TEST_CASE("the output has no surrounding whitespace", "[text][property]") {
  const auto input = text_of(GENERATE(take(200, chunk(12, random(0, 100)))));
  if (!is_blank(input)) {
    const auto out = lib::normalize(input);
    REQUIRE_FALSE(out.empty());
    CHECK(kWhitespace.find(out.front()) == std::string_view::npos);
    CHECK(kWhitespace.find(out.back()) == std::string_view::npos);
  }
}
