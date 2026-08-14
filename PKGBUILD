# Maintainer: Lars Karlslund
pkgname=lumasave
pkgver=0.1.0.alpha.1
pkgrel=1
pkgdesc='Content-adaptive LCD backlight power saving for KDE Plasma'
arch=(x86_64)
url='https://github.com/lkarlslund/lumasave'
license=(MIT)
options=(!debug)
_kwin_package_version=$(pacman -Q kwin | awk '{print $2}')
_kwin_abi_version=${_kwin_package_version%-*}
depends=("kwin=$_kwin_abi_version" libkscreen kcmutils qt6-base qt6-declarative plasma-workspace kdialog)
makedepends=(cmake extra-cmake-modules rust git)
_commit=${LUMASAVE_COMMIT:-v0.1.0-alpha.1}
source=("$pkgname::git+$url.git#commit=$_commit")
b2sums=(SKIP)

prepare() {
    cargo fetch --manifest-path "$pkgname/Cargo.toml" --locked
}

build() {
    cmake -S "$pkgname/kwin" -B build \
        -DCMAKE_BUILD_TYPE=None \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
        -DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins \
        -Wno-dev
    CARGO_NET_OFFLINE=true cmake --build build
}

check() {
    cargo test --manifest-path "$pkgname/Cargo.toml" --workspace --locked
}

package() {
    DESTDIR="$pkgdir" cmake --install build
    install -Dm644 "$pkgname/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
