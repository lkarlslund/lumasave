# Contributing

LumaSave is an experimental native KWin effect. Bug reports and focused pull
requests are welcome.

## Build and validate

With Plasma 6.6 or newer development packages installed:

```sh
cargo test --workspace --locked
cmake -S kwin -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run `./scripts/validate-release.sh` for the complete automated check. Native
KWin plug-ins are ABI-sensitive: rebuild after every KWin update, and never
load a binary built for a different KWin upstream version.

## Pull requests

- Keep display changes reversible and restore the user's logical brightness.
- Add policy/math tests for behavior changes.
- Do not add screen capture persistence or telemetry.
- Update `CHANGELOG.md` for user-visible changes.
- Explain the Plasma/KWin version and hardware used for manual testing.

By contributing, you agree that your contribution is licensed under MIT.
