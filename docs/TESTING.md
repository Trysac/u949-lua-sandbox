# Testing

## Tests

- `tests/sandbox_test.cpp`: Exercises resource limits (memory, instruction, call depth), environment security (whitelist, metatable), host function bindings, and basic API errors.
- `tests/text_test.cpp`: Validates the `normalize` function (trimming, lowercasing, blank input handling).
- `tests/text_properties_test.cpp`: Property-based testing for `normalize` using Catch2 generators to verify idempotence and whitespace stripping.

## Policy

- Tests cover every public API method and ensure observable effects (exceptions, status codes, return values).
- Line coverage must remain at or above 80%.
- Tests are deterministic and independent of test order.