#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$project_dir/build-kwin"

cmake -S "$project_dir/kwin" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
    -DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins
# Build from clean. Pacman keeps the package build date as the mtime of KWin's
# headers, so after a KWin upgrade they can look older than the generated
# moc output, and an incremental build then keeps the previous KWin's plugin
# version tag. KWin refuses to load such a plugin.
cmake --build "$build_dir" --parallel --clean-first

kwin_version=$(sed -n 's/^#define KWIN_PLUGIN_VERSION_STRING "\(.*\)"$/\1/p' /usr/include/kwin/config-kwin.h 2>/dev/null || true)
if [ -n "$kwin_version" ] &&
    ! grep -qaF "EffectPluginFactory$kwin_version" "$build_dir/bin/kwin/effects/plugins/lumasave.so"; then
    echo "Built effect does not target KWin $kwin_version; refusing to install it." >&2
    exit 1
fi

cmake --install "$build_dir"

# Remove helper scripts used by pre-alpha widget revisions. The current widget
# talks to its native controller over D-Bus and never launches shell polling.
rm -f "$HOME/.local/share/plasma/plasmoids/com.github.lkarlslund.lumasave/contents/code/read-status"
rm -f "$HOME/.local/share/plasma/plasmoids/com.github.lkarlslund.lumasave/contents/code/settings"
rm -f "$HOME/.local/lib/qml/org/kde/lumasave/lumasaveqmlplugin.so"

# Refresh the per-user service cache so System Settings discovers the KCM
# immediately without a session restart.
kbuildsycoca6 >/dev/null

mkdir -p "$HOME/.config/environment.d"
install -m644 "$project_dir/config/90-lumasave.conf" "$HOME/.config/environment.d/90-lumasave.conf"

mkdir -p "$HOME/.local/share/lumasave"
{
    printf 'kwin=%s\n' "$(kwin_wayland --version 2>/dev/null | head -n 1 || printf unknown)"
    printf 'source=%s\n' "$(git -C "$project_dir" describe --always --dirty 2>/dev/null || printf release-tarball)"
    printf 'source_dir=%s\n' "$project_dir"
} >"$HOME/.local/share/lumasave/build-info"

# KWin only loads effects built for its exact version, so rebuild this
# rootless install automatically at login and whenever KWin is upgraded.
install -Dm755 "$project_dir/scripts/lumasave-autorebuild" "$HOME/.local/bin/lumasave-autorebuild"
unit_dir="$HOME/.local/share/systemd/user"
mkdir -p "$unit_dir"
install -m644 "$project_dir/config/systemd/lumasave-rebuild.service" \
    "$project_dir/config/systemd/lumasave-rebuild.path" "$unit_dir/"
if systemctl --user daemon-reload 2>/dev/null; then
    systemctl --user enable lumasave-rebuild.service >/dev/null 2>&1 || true
    systemctl --user enable --now lumasave-rebuild.path >/dev/null 2>&1 || true
fi

existing_mode=$(kreadconfig6 --file kwinrc --group Effect-lumasave --key OperatingMode --default __lumasave_missing__)
if [ "$existing_mode" = __lumasave_missing__ ]; then
    kwriteconfig6 --file kwinrc --group Plugins --key lumasaveEnabled false
    kwriteconfig6 --file kwinrc --group Effect-lumasave --key Enabled false
    kwriteconfig6 --file kwinrc --group Effect-lumasave --key OperatingMode off
fi

echo "LumaSave installed for this user without changing its current mode."
echo "Log out and back in after the first install or after replacing the loaded KWin plugin."
echo "KWin upgrades trigger an automatic rebuild; failures are reported as notifications."
