/// @file
/// @brief Text helpers of this library.
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

/// @brief The public API of this library.
namespace trysac::lua_sandbox {

/// @brief The input is empty or holds only whitespace; `what()` is `input is blank`.
class BlankInputError : public std::invalid_argument {
 public:
  /// @brief Creates the error with the message `input is blank`.
  BlankInputError();
};

/// @brief Trims surrounding ASCII whitespace and lower-cases the ASCII letters of `input`.
/// @param input The text to normalize.
/// @return The trimmed, lower-cased text.
/// @throws BlankInputError when nothing is left after trimming, instead of an empty string.
[[nodiscard]] std::string normalize(std::string_view input);

}  // namespace trysac::lua_sandbox
