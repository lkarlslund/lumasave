# LumaSave

LumaSave explores content-adaptive backlight power saving for Linux desktops.
It lowers an LCD's physical backlight while applying a synchronized luminance
curve intended to retain useful visual contrast.

The project is deliberately split into a display-independent Rust policy core,
an offline simulator, and compositor integrations. The first compositor target
is KDE Plasma's KWin because KWin already owns both the output color pipeline
and the physical brightness device.

## Status

Experimental. The simulator is usable; live display integration remains off by
default until its safety and power measurements are validated.

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
- Restore an uncompensated image before returning the backlight to its baseline.
- Disable for HDR, color calibration, screen capture and unsupported outputs.
- Rate-limit changes and use hysteresis to prevent visible pumping.
- Keep all desktop image data inside the compositor; export histograms only.

## License

MIT
