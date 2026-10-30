#!/bin/sh
# Private host dependencies for the legacy cross compiler; no device access.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PATH=/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin:/nix/store/7y7p3y9fwvfwvwd2lhvl5iksi50vvgqv-llvm-binutils-wrapper-22.0.0-rocm/bin:$PATH
export PATH
CC=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin/clang
export CC
gmp_headers=/nix/store/1ll57z8pdgj3r5y450yas9n2053dyvmd-gmp-6.3.0-dev
gmp_libs=/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0
(cd "$base/host-inputs" && sha256sum -c SHA256SUMS)
mkdir -p "$base/host-build/mpfr" "$base/host-build/mpc"
cd "$base/host-build/mpfr"
sh "$base/host-sources/mpfr-3.1.6/configure" --prefix="$base/host-prefix" \
    --with-gmp-include="$gmp_headers/include" --with-gmp-lib="$gmp_libs/lib" \
    --enable-shared --disable-static > configure.log 2>&1
make -j4 > build.log 2>&1
make -j4 check > check.log 2>&1
make install > install.log 2>&1
cd "$base/host-build/mpc"
sh "$base/host-sources/mpc-1.0.3/configure" --prefix="$base/host-prefix" \
    --with-gmp-include="$gmp_headers/include" --with-gmp-lib="$gmp_libs/lib" \
    --with-mpfr="$base/host-prefix" \
    --enable-shared --disable-static > configure.log 2>&1
make -j4 > build.log 2>&1
make -j4 check > check.log 2>&1
make install > install.log 2>&1
echo 'Private host libraries built and checked; no device access.'
