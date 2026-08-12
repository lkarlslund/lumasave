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

# Refresh the per-user service cache so System Settings discovers the KCM
# immediately without a session restart.
kbuildsycoca6 >/dev/null

mkdir -p "$HOME/.config/environment.d"
ln -sfn "$project_dir/config/90-lumasave.conf" "$HOME/.config/environment.d/90-lumasave.conf"

kwriteconfig6 --file kwinrc --group Plugins --key lumasaveEnabled false
kwriteconfig6 --file kwinrc --group Effect-lumasave --key Enabled false

echo "LumaSave installed disabled for this user. Log out and back in once so KWin discovers it."
