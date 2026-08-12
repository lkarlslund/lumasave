#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

kwriteconfig6 --file kwinrc --group Effect-lumasave --key Enabled false
kwriteconfig6 --file kwinrc --group Effect-lumasave --key OperatingMode off
kwriteconfig6 --file kwinrc --group Plugins --key lumasaveEnabled false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect lumasave 2>/dev/null || true
rm -f "$HOME/.local/lib/qt6/plugins/kwin/effects/plugins/lumasave.so"
rm -f "$HOME/.local/lib/qt6/plugins/plasma/kcms/systemsettings/kcm_lumasave.so"
rm -f "$HOME/.local/bin/lumasave-calibrate"
rm -f "$HOME/.local/bin/lumasave-calibrate-preview"
rm -f "$HOME/.local/bin/lumasave-check"
rm -rf "$HOME/.local/lib/qml/org/kde/lumasave"
rm -f "$HOME/.local/share/applications/kcm_lumasave.desktop"
rm -rf "$HOME/.local/share/plasma/plasmoids/com.github.lkarlslund.lumasave"
rm -f "$HOME/.config/environment.d/90-lumasave.conf"
kbuildsycoca6 >/dev/null

echo "LumaSave removed for this user."
