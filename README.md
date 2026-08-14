# LumaSave

**Use less display power without making your desktop feel dim.**

The screen is often one of a laptop's largest power consumers. Turning down its
backlight saves energy, but it also makes text, photographs, and application
interfaces harder to see. LumaSave lowers the physical backlight and compensates
the picture at the same time, preserving useful contrast and readability.

The result is a display that uses a lower backlight level while still looking
closer to the brightness you chose.

## What do I gain?

- **Lower display power use.** LumaSave reduces the physical LCD backlight, not
  merely the colors drawn on screen.
- **A readable image.** A content-aware correction curve preserves detail while
  exact black stays black.
- **More useful battery time.** Saving display power can extend runtime on a
  laptop. The exact gain depends on the panel, brightness, workload, and battery.
- **No new brightness workflow.** Plasma's brightness slider and keyboard keys
  remain your normal brightness controls. LumaSave works underneath them.
- **Automatic operation.** It can work continuously, wait until you stop using
  the computer, or remain off.
- **Visible results.** The Plasma widget shows requested brightness, effective
  backlight, current reduction, and accumulated saved backlight-hours.

For example, with brightness set to 60% and LumaSave reducing the backlight by
25%, the panel runs at about 45% while the image is adjusted to retain readable
detail.

## How it feels to use

LumaSave lives in **System Settings → Display & Monitor → LumaSave** and provides
three simple modes:

- **On** continuously adapts to what is on screen. Changes are gradual, and
  moving the mouse or typing does not alter the selected reduction.
- **After inactivity** waits until you step away, then restores normal operation
  immediately when you return.
- **Off** leaves the display completely untouched.

The optional **LumaSave Status** panel widget provides quick mode buttons,
maximum reduction, battery-only operation, current status, and a shortcut to
the complete settings page.

## Designed not to get in your way

- LumaSave never replaces or permanently changes your chosen brightness.
- Backlight and image compensation move together with a smooth transition.
- Video playback and presentations that keep the screen awake inhibit LumaSave.
- Calibration lets you tune compensation for your particular laptop panel at
  25%, 50%, and 100% brightness.
- All screen analysis stays inside KWin. LumaSave does not save or upload screen
  images.
- Disabling or unloading it restores the normal image and backlight.

## Understanding “saved backlight-hours”

The widget records how much full-scale backlight exposure LumaSave has avoided:

`requested brightness × LumaSave reduction × time`

One hour at 60% brightness with a 25% reduction therefore saves 0.15
backlight-hours. This correctly accounts for both requested and effective
brightness. It is not presented as watt-hours because LCD panels have different
power curves; measuring actual battery energy requires hardware-specific data.

## Install for one user

LumaSave currently targets KDE Plasma 6.6 and newer on Wayland. It is experimental and
the alpha hardware gate currently covers only the maintainer's laptop. Read the
[compatibility and safety notes](docs/COMPATIBILITY.md) before enabling it.

```sh
git clone https://github.com/lkarlslund/lumasave.git
cd lumasave
./install.sh
```

The installation is entirely user-local and does not require `sudo`. Log out
and back in once after installation, then open **System Settings → Display &
Monitor → LumaSave**. Add **LumaSave Status** through Plasma's normal **Add
Widgets…** interface if you want panel controls and statistics.

LumaSave builds against the installed KWin because native KWin plug-ins do not
have a stable cross-version ABI. After upgrading Plasma/KWin, run:

```sh
~/.local/bin/lumasave-check
```

If it reports a mismatch, rerun `./install.sh` and log in again.

To remove LumaSave:

```sh
./scripts/uninstall-user.sh
```

## Distribution packages

GitHub Actions builds packages for Arch Linux, Fedora, openSUSE Tumbleweed, and
Ubuntu/Kubuntu 26.04. An Arch [`PKGBUILD`](PKGBUILD), Debian packaging, and an
RPM spec are included. Each package is tied to the KWin version used to build it
because native KWin plug-ins do not have a stable cross-version ABI.

## Project status

LumaSave is experimental software. The native effect currently targets KDE
Plasma 6.6 and newer, an internal SDR LCD panel, and a Wayland session. HDR, unsupported
outputs, screenshots, calibration, and relevant power-management inhibitors are
handled conservatively.

HDR is not supported. When you switch LumaSave on it warns and offers to turn
HDR off; if HDR is enabled later, LumaSave restores normal backlight operation
and reports that it is blocked.

The project consists of a native KWin effect, a display-independent Rust policy
core, a GPU compensation shader, calibration tools, and an offline simulator.
Testing and release details live in [`docs`](docs).

## License

[MIT](LICENSE)
