#include "trysac/lua_sandbox/text.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace trysac::lua_sandbox {

namespace {

constexpr std::string_view kWhitespace = " \t\n\v\f\r";

char to_lower_ascii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

}  // namespace

BlankInputError::BlankInputError() : std::invalid_argument("input is blank") {}

std::string normalize(std::string_view input) {
  const auto first = input.find_first_not_of(kWhitespace);
  if (first == std::string_view::npos) {
    throw BlankInputError();
  }
  const auto last = input.find_last_not_of(kWhitespace);
  std::string out(input.substr(first, last - first + 1));
  std::ranges::transform(out, out.begin(), to_lower_ascii);
  return out;
}

}  // namespace trysac::lua_sandbox
