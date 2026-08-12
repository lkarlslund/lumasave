#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUMASAVE_BUILD_DIR:-"$project_dir/build-validation"}
stage_dir=${LUMASAVE_STAGE_DIR:-"$build_dir/stage"}

cargo test --manifest-path "$project_dir/Cargo.toml" --workspace --locked
cmake -S "$project_dir/kwin" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
    -DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins \
    -Wno-dev
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

DESTDIR="$stage_dir" cmake --install "$build_dir"
test -f "$stage_dir/usr/lib/qt6/plugins/kwin/effects/plugins/lumasave.so"
test -f "$stage_dir/usr/lib/qt6/plugins/plasma/kcms/systemsettings/kcm_lumasave.so"
test -f "$stage_dir/usr/share/plasma/plasmoids/com.github.lkarlslund.lumasave/contents/ui/main.qml"
test -f "$stage_dir/usr/lib/qt6/qml/org/kde/lumasave/qmldir"
! ldd "$stage_dir/usr/lib/qt6/qml/org/kde/lumasave/lumasaveqmlplugin.so" | grep -q 'not found'

for file in "$project_dir"/install.sh "$project_dir"/scripts/*.sh "$project_dir"/scripts/lumasave-*; do
    sh -n "$file"
done
for file in "$project_dir"/kwin/metadata.json "$project_dir"/kcm/kcm_lumasave.json "$project_dir"/plasmoid/metadata.json; do
    python -m json.tool "$file" >/dev/null
done
desktop-file-validate "$project_dir/kcm/kcm_lumasave.desktop"
/usr/lib/qt6/bin/qmllint -I "$build_dir" -I /usr/lib/qt6/qml \
    "$project_dir/plasmoid/contents/ui/main.qml" \
    "$project_dir/tools/calibration/main.qml"

if git -C "$project_dir" grep -nE '/home/|BEGIN (RSA |OPENSSH )?PRIVATE KEY|gh[pousr]_[A-Za-z0-9_]{20,}' \
    -- . ':!scripts/validate-release.sh'; then
    echo "Release audit found a local path or credential-like content." >&2
    exit 1
fi

echo "LumaSave release validation passed."
