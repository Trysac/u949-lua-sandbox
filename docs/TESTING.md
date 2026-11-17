# Testing

## Running

```sh
cmake -P scripts/check.cmake -- tests   # Catch2 tests under llvm-cov
cmake -P scripts/check.cmake            # every gate, as CI and the factory run it
```

## Policy

- Line coverage of the library (`include/` and `src/`, measured by llvm-cov) must stay
  at or above 80 %; the gate fails below it.
- Tests exercise the public API. They assert every observable effect: returned values, thrown
  exceptions (`CHECK_THROWS_MATCHES`) and their messages.
- Property tests draw inputs with Catch2 generators (`GENERATE(take(200, random(...)))`) and
  state invariants such as idempotence or round trips; the test run fixes the random seed.
- Each bug fix starts with a failing test that reproduces it.
- Tests are deterministic: no network, no wall clock, no dependence on test order.

## Tiers

- Local tier (every gate run, coverage measured here): unit and property tests, plus conformance
  on small fixtures kept in the repository.
- CI tier: conformance that needs an external tool or a download. Tag the test case with
  `[conformance]` (`TEST_CASE("name", "[conformance]")`); the local run filters the tag out
  (`~[conformance]`) unless `FABRICA_CI=1`, while the lint gate always compiles it. The CI
  `conformance` job sets `FABRICA_CI=1` and runs the full suite. Tools are installed by the
  optional `ci/conformance-setup.sh` (bash, CI only; verify every download against a pinned
  SHA-256), and tests `SKIP()` cleanly when their tool is missing.

## Layout

Tests live in `tests/`, one `*_test.cpp` file per area, all linked into one Catch2 executable.
Fixtures and golden files go in `tests/fixtures/` (never compiled); tests open them relative to
the repository root, the working directory of the test run.
