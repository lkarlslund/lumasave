#!/bin/sh
# SPDX-License-Identifier: MIT
# Rootless source installer for LumaSave.
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

missing=
for command in cmake cargo c++ kbuildsycoca6 kwriteconfig6 qdbus6; do
    if ! command -v "$command" >/dev/null 2>&1; then
        missing="$missing $command"
    fi
done
if [ -n "$missing" ]; then
    echo "Missing build/runtime commands:$missing" >&2
    echo "Install Qt 6, KDE Frameworks 6 development files, KWin development files, CMake, and Rust." >&2
    exit 1
fi

exec "$project_dir/scripts/install-user.sh"
