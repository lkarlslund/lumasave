#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$project_dir/kwin" -B "$project_dir/build-kwin" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
    -DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins
cmake --build "$project_dir/build-kwin" --target lumasave-calibrate --parallel
effect_name="lumasave_calibration_live"
effect_dir="$HOME/.local/lib/qt6/plugins/kwin/effects/plugins"
effect_file="$effect_dir/$effect_name.so"
cleanup() {
    trap - EXIT INT TERM
    kwriteconfig6 --file kwinrc --group Effect-lumasave --key CalibrationActive false
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect "$effect_name" >/dev/null 2>&1 || true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "$effect_name" >/dev/null 2>&1 || true
    for key in CalibrationMode CalibrationActive CalibrationReductionPercent \
        CalibrationPerceivedBrightnessPercent CalibrationShadowDetailPercent \
        CalibrationHighlightProtectionPercent CalibrationColorIntensityPercent; do
        kwriteconfig6 --file kwinrc --group Effect-lumasave --key "$key" --delete ''
    done
    if [ -f "$effect_file" ]; then
        gio trash "$effect_file" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM
mkdir -p "$effect_dir"
cp "$project_dir/build-kwin/bin/kwin/effects/plugins/lumasave.so" "$effect_file"
kwriteconfig6 --file kwinrc --group Effect-lumasave --key CalibrationMode true
kwriteconfig6 --file kwinrc --group Effect-lumasave --key CalibrationActive false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect "$effect_name" >/dev/null
set +e
LUMASAVE_CALIBRATION_EFFECT="$effect_name" "$project_dir/build-kwin/bin/lumasave-calibrate"
calibration_status=$?
set -e
cleanup
if [ "$calibration_status" -eq 2 ]; then
    echo "Calibration canceled; existing LumaSave settings were not changed."
    exit 0
elif [ "$calibration_status" -ne 0 ]; then
    exit "$calibration_status"
fi

preview_config="$HOME/.config/LumaSave/Calibration.conf"
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
