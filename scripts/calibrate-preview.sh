#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$project_dir/kwin" -B "$project_dir/build-kwin" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
    -DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins
cmake --build "$project_dir/build-kwin" --target lumasave-calibrate --parallel
brightness_service="org.kde.org_kde_powerdevil"
brightness_path="/org/kde/ScreenBrightness/display0"
brightness_interface="org.kde.ScreenBrightness.Display"
baseline_brightness=$(qdbus6 "$brightness_service" "$brightness_path" \
    org.freedesktop.DBus.Properties.Get "$brightness_interface" Brightness)
set +e
"$project_dir/build-kwin/bin/lumasave-calibrate"
calibration_status=$?
set -e
# A second restoration layer covers abnormal application exits where the C++
# destructor could not run. Flag 1 suppresses Plasma's brightness indicator.
qdbus6 "$brightness_service" "$brightness_path" \
    "$brightness_interface.SetBrightnessWithContext" \
    "$baseline_brightness" 1 lumasave-calibration-guard >/dev/null
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
