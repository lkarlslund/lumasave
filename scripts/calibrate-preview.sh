#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
qml6 "$project_dir/tools/calibration/main.qml"

preview_config="$HOME/.config/QtProject/Qml Runtime.conf"
preview_group="Calibration-eDP-1"
read_value() {
    kreadconfig6 --file "$preview_config" --group "$preview_group" --key "$1" --default "$2"
}
kwriteconfig6 --file kwinrc --group Effect-lumasave --key PerceivedBrightnessPercent "$(read_value perceivedBrightness 100)"
kwriteconfig6 --file kwinrc --group Effect-lumasave --key ShadowDetailPercent "$(read_value shadowDetail 50)"
kwriteconfig6 --file kwinrc --group Effect-lumasave --key HighlightProtectionPercent "$(read_value highlightProtection 70)"
kwriteconfig6 --file kwinrc --group Effect-lumasave --key ColorIntensityPercent "$(read_value colorIntensity 100)"
kwriteconfig6 --file kwinrc --group Effect-lumasave --key MaxBacklightReductionPercent "$(read_value maxReduction 10)"

echo "Calibration saved for eDP-1. LumaSave remains disabled until explicitly enabled."
