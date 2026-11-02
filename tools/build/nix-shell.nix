# SPDX-License-Identifier: GPL-3.0-or-later
# Reproducible host environment for the committed host artifacts.
#
#   nix-shell tools/build/nix-shell.nix
#
# It exports the variables tools/build/toolchain.sh validates.  The private
# ARM cross toolchain and LVGL source still come from the documented inputs;
# this shell only removes the ad-hoc environment-manager path guesses from the scripts.
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    gcc
    gnumake
    python3
    luajit
    openssl
    qemu
    busybox
    gmp
    libmpc
    linuxHeaders
    dejavu_fonts
    nodejs
    pkg-config
  ];

  shellHook = ''
    export LINUX_HEADERS=${pkgs.linuxHeaders}/include
    export GMP_LIB=${pkgs.gmp}/lib
    export MPC_LIB=${pkgs.libmpc}/lib
    export OPENSSL=${pkgs.openssl}/bin/openssl
    export LD_LIBRARY_PATH=${pkgs.openssl.out}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    export BUSYBOX=${pkgs.busybox}/bin/busybox
    export QEMU_ARM=${pkgs.qemu}/bin/qemu-arm
    export QEMU_SYSTEM_ARM=${pkgs.qemu}/bin/qemu-system-arm
    export FONT_REG=${pkgs.dejavu_fonts}/share/fonts/truetype/DejaVuSans.ttf
    export FONT_BOLD=${pkgs.dejavu_fonts}/share/fonts/truetype/DejaVuSans-Bold.ttf
    export FONT_SYMBOLS="$PWD/work/sources/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff"
    echo "GleWorks host environment ready."
    echo "LINUX_HEADERS=$LINUX_HEADERS"
    echo "GMP_LIB=$GMP_LIB"
    echo "MPC_LIB=$MPC_LIB"
  '';
}
