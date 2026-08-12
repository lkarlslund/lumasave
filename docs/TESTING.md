# Maintainer test protocol

Automated checks validate Rust policy behavior, saved-exposure arithmetic,
native compilation, installation layout, metadata, shell syntax, and QML.

Before publishing an alpha, test on the supported laptop:

1. Start Off and confirm brightness keys and Plasma's slider behave normally.
2. Enable On in SDR; accept the KDE-dimmer prompt and confirm HDR is untouched.
3. Enable HDR, then enable LumaSave; test both Cancel and Disable HDR.
4. While LumaSave is active, enable HDR and confirm immediate restoration plus
   a blocked status in the widget.
5. Exercise On and After inactivity on bright, dark, mixed, and video content.
   Mouse movement must not materially alter On mode.
6. Confirm a playback inhibitor prevents After inactivity activation.
7. Run calibration at 25%, 50%, and 100%; test A/B/Auto and bypass; verify Save
   and Cancel restore the prior brightness and mode.
8. Compare widget requested/effective brightness with the physical backlight
   and verify saved hours equal requested brightness × reduction × elapsed time.
9. Take a screenshot while active and verify it is not baked with compensation.
10. Disable and unload LumaSave; confirm the original image and brightness.

Record the installed `kwin`, `plasma-workspace`, kernel, panel identity, and any
failures in the release notes. This one-laptop protocol is the alpha hardware
gate; it is not a general hardware compatibility claim.

