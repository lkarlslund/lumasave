# Release checklist

- [ ] `./scripts/validate-release.sh` passes from a clean checkout.
- [ ] The supported-laptop protocol in `docs/TESTING.md` passes.
- [ ] Metadata and changelog use the intended version.
- [ ] Arch artifact depends on the build-time KWin upstream version, without
      pinning an Arch/CachyOS package release suffix.
- [ ] Rootless install, `lumasave-check`, and uninstall are tested.
- [ ] No machine paths, credentials, captured screens, or debug-only settings
      are tracked.
- [ ] Tag `v0.1.0-alpha.1`, verify the CI artifact, and publish release notes
      with the tested package versions and one-laptop hardware limitation.

CI validates every commit. The rolling `nightly` prerelease is replaced only
after a successful build, no more than once every four hours; an hourly schedule
also ensures a fresh successful commit is published at least once per day.
