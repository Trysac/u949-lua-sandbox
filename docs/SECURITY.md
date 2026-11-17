# Security notes

## Threat model

`lua-sandbox` is a pure library without I/O. The main risks are:

- **Supply chain**: a compromised dependency, vcpkg port or CI action. Mitigation: standard
  library only, every vcpkg port pinned by `vcpkg.json`'s `builtin-baseline` and each direct
  dependency by an exact override, OSV advisories over the upstream release of every installed
  port and OSV-Scanner over the repository in the gates, Dependabot weekly updates, actions
  pinned by commit SHA, minimal workflow permissions.
- **Leaked secrets**: tokens committed by mistake. Mitigation: `gitleaks` runs in the local gates
  and in CI; `.env` files are git-ignored.
- **Memory and injection bugs**: buffer overflows, format strings, command injection.
  Mitigation: clang-tidy (bugprone, cert, clang-analyzer and cppcoreguidelines checks) and the
  compiler with warnings as errors, and the factory's Semgrep rules
  (`.semgrep/factory-rules.yml`, `nosemgrep` ignored): no `system`/`popen`, no unbounded
  `strcpy`/`sprintf`, no non-literal format strings; fix the code instead of silencing it.
- **Untrusted input**: callers may pass hostile strings. Mitigation: validate input and throw
  explicit, documented exceptions; use `std::string`, `std::string_view` and bounds-checked
  access instead of raw buffers.

## Publishing

Releases are git tags; publishing to a vcpkg registry or ConanCenter needs a maintainer account
and a reviewed port, which are never stored in the repository.

To report a vulnerability, see [SECURITY.md](../SECURITY.md).
