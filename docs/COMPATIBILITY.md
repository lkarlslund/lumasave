# Compatibility and safety

## Supported alpha configuration

The first alpha is validated on one laptop: the maintainer's current internal
LCD panel, running current Arch Linux/CachyOS packages, KDE Plasma 6.7,
KWin 6.7, and a Wayland session. Other panels and distributions are unvalidated.

The Arch package pins the KWin upstream version it was built against while
allowing distribution packaging revisions of that same version.
Rootless installations record that version; `lumasave-check` detects upgrades
that require a rebuild.

## HDR

HDR is not supported. LumaSave checks only when the user switches to **On** or
**After inactivity**, then offers to disable HDR. Merely opening settings or
leaving LumaSave Off does nothing. If HDR is enabled later while LumaSave is
active, the effect immediately restores the normal backlight and reports a
blocked state.

## Other safeguards

- KDE's separate inactive-screen dimmer is detected at activation and can be
  disabled without changing screen-off or suspend policy.
- Playback and presentation inhibitors prevent activation.
- Calibration owns display changes only while its dialog is open and restores
  the previous mode and brightness on Save, Cancel, or process termination.
- Analysis is downsampled in memory. No image, histogram, or content is saved
  or transmitted.
