#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Regenerate the checked-in Vietnamese-capable LVGL fonts.
#
# Requires node/npm (uses lv_font_conv) and DejaVu Sans.  DejaVu is only used
# as an input asset; the generated C files live in this directory and are
# committed, so normal builds need neither node nor the font package.
set -eu
cd "$(dirname "$0")"

FONT_REG=${FONT_REG:-/nix/store/349bajb6xdfd4mw3in7bs8jkpmhx1w96-dejavu-fonts-2.37/share/fonts/truetype/DejaVuSans.ttf}
FONT_BOLD=${FONT_BOLD:-/nix/store/349bajb6xdfd4mw3in7bs8jkpmhx1w96-dejavu-fonts-2.37/share/fonts/truetype/DejaVuSans-Bold.ttf}
FONT_SYMBOLS=${FONT_SYMBOLS:-/home/will/dev/kindle/work/sources/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff}
RANGES="0x20-0x7E,0xA0-0x24F,0x1EA0-0x1EF9,0x2013-0x2026,0x0300-0x0323"
# Exact LVGL symbol set from src/font/lv_symbol_def.h (FontAwesome codepoints).
SYMBOLS="0xF001,0xF008,0xF00B,0xF00C,0xF00D,0xF011,0xF012,0xF013,0xF015,0xF019,0xF01C,0xF021,0xF023,0xF026,0xF027,0xF028,0xF02D,0xF03E,0xF043,0xF048,0xF04B,0xF04C,0xF04D,0xF051,0xF052,0xF053,0xF054,0xF067,0xF068,0xF06E,0xF070,0xF071,0xF073,0xF074,0xF077,0xF078,0xF079,0xF07B,0xF093,0xF095,0xF0AE,0xF0C4,0xF0C5,0xF0C7,0xF0C8,0xF0C9,0xF0E0,0xF0E7,0xF0EA,0xF0F3,0xF11C,0xF124,0xF14A,0xF158,0xF1EB,0xF240,0xF241,0xF242,0xF243,0xF244,0xF287,0xF293,0xF2ED,0xF304,0xF55A,0xF7C2,0xF8A2"

gen() {
    name=$1
    font=$2
    size=$3
    npx --yes lv_font_conv --font "$font" --size "$size" --bpp 4 \
        --format lvgl --no-compress \
        --range "$RANGES" \
        --font "$FONT_SYMBOLS" --range "$SYMBOLS" \
        --lv-include lvgl/lvgl.h \
        --lv-font-name "$name" \
        -o "gw_font_${name#gw_font_}.c"
}

gen gw_font_24 "$FONT_REG" 24
gen gw_font_24_bold "$FONT_BOLD" 24
gen gw_font_28 "$FONT_REG" 28
gen gw_font_32 "$FONT_REG" 32
gen gw_font_32_bold "$FONT_BOLD" 32
gen gw_font_36 "$FONT_REG" 36
gen gw_font_36_bold "$FONT_BOLD" 36
gen gw_font_44_bold "$FONT_BOLD" 44
gen gw_font_72_bold "$FONT_BOLD" 72
