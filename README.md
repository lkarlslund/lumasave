# LumaSave

LumaSave explores content-adaptive backlight power saving for Linux desktops.
It lowers an LCD's physical backlight while applying a synchronized luminance
curve intended to retain useful visual contrast.

The project is deliberately split into a display-independent Rust policy core,
an offline simulator, and compositor integrations. The first compositor target
is KDE Plasma's KWin because KWin already owns both the output color pipeline
and the physical brightness device.

The native KWin plugin is intentionally thin: it performs the one-shot GPU
downsample and applies the GLSL curve, while the 64-bin histogram decision is
made by the same Rust core used by the simulator.

## Status

Experimental. The simulator and native KWin effect build successfully on Plasma
6.7. The effect remains conservative by default while its visual quality and
power measurements are validated.

## User-local installation

```sh
./scripts/install-user.sh
```

No system files or administrator privileges are needed. Log out and back in
once after the first installation so KWin inherits the local Qt plugin path.
Later rebuilds replace the plugin under `~/.local`.

To remove it:

```sh
./scripts/uninstall-user.sh
```

## Simulator

```sh
cargo run --release -- analyze screenshot.png \
  --config config/default.toml \
  --max-reduction 35 \
  --output compensated.png
```

`--max-reduction` is a user-facing percentage (0–75). It overrides the
configuration for that run. The KDE settings page uses the same percentage
scale and defaults to 35%.

The output image contains the pixel compensation that should be displayed while
the physical backlight is multiplied by the reported scale. A normal screenshot
cannot simulate the physical backlight itself.

## Safety principles

- Take no screenshots or histograms during active use. Sample exactly once
  after the configured input-idle delay.
- Respect KWin and PowerDevil idle inhibitors, so video playback, presentations
  and similar keep-awake workloads never activate LumaSave.
- Reject an apparently idle desktop that is still repainting continuously.
- On resumed input, deactivate immediately without sampling the screen.
- Never override the user's brightness setting; apply a reversible multiplier.
- Adjust the physical KWin brightness device directly and silently. Plasma's
  slider and hotkeys remain the baseline even when changed while active.
- Restore an uncompensated image before returning the backlight to its baseline.
- Disable for HDR, color calibration, screen capture and unsupported outputs.
- Rate-limit changes and use hysteresis to prevent visible pumping.
- Keep all desktop image data inside the compositor; export histograms only.
- Use a self-contained SDR sRGB/linear-light shader; do not depend on private
  color-management uniforms that KWin does not populate for external effects.

## License

MIT
