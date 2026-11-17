# Contributing

Thanks for helping improve `lua-sandbox`.

## Workflow

1. Branch from `main`; never push to `main` directly.
2. `cmake -P scripts/configure.cmake` to install dependencies and configure the build.
3. Write a failing test, then the change.
4. `cmake -P scripts/check.cmake` must pass.
5. Update `CHANGELOG.md` under **Unreleased** and the docs the change affects.
6. Open a pull request describing the problem and the solution.

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```text
feat: add strict mode to normalize
fix: reject tab-only input
docs: document the thrown exceptions
```

`feat` bumps the minor version, `fix` the patch version, and a `BREAKING CHANGE:` footer the major
version. Releases are git tags `vMAJOR.MINOR.PATCH` matching the `VERSION` of `project()` in
`CMakeLists.txt`.

## Code of conduct

Be respectful and constructive. Maintainers may close contributions that ignore this guide.
