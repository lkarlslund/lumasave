#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$project_dir/build-kwin"

cmake -S "$project_dir/kwin" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
    -DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins
cmake --build "$build_dir" --parallel
cmake --install "$build_dir"

# Remove helper scripts used by pre-alpha widget revisions. The current widget
# talks to its native controller over D-Bus and never launches shell polling.
rm -f "$HOME/.local/share/plasma/plasmoids/com.github.lkarlslund.lumasave/contents/code/read-status"
rm -f "$HOME/.local/share/plasma/plasmoids/com.github.lkarlslund.lumasave/contents/code/settings"

# Refresh the per-user service cache so System Settings discovers the KCM
# immediately without a session restart.
kbuildsycoca6 >/dev/null

mkdir -p "$HOME/.config/environment.d"
install -m644 "$project_dir/config/90-lumasave.conf" "$HOME/.config/environment.d/90-lumasave.conf"

mkdir -p "$HOME/.local/share/lumasave"
{
    printf 'kwin=%s\n' "$(kwin_wayland --version 2>/dev/null | head -n 1 || printf unknown)"
    printf 'source=%s\n' "$(git -C "$project_dir" describe --always --dirty 2>/dev/null || printf release-tarball)"
} >"$HOME/.local/share/lumasave/build-info"

existing_mode=$(kreadconfig6 --file kwinrc --group Effect-lumasave --key OperatingMode --default __lumasave_missing__)
if [ "$existing_mode" = __lumasave_missing__ ]; then
    kwriteconfig6 --file kwinrc --group Plugins --key lumasaveEnabled false
    kwriteconfig6 --file kwinrc --group Effect-lumasave --key Enabled false
    kwriteconfig6 --file kwinrc --group Effect-lumasave --key OperatingMode off
fi

echo "LumaSave installed for this user without changing its current mode."
echo "Log out and back in after the first install or after replacing the loaded KWin plugin."
echo "After Plasma/KWin upgrades, run ~/.local/bin/lumasave-check and rebuild if requested."
