#!/bin/sh
# Device-side KOReader launcher for Phase 2 workload measurements.
#
# This uses the established reader runtime and libraries but deliberately does
# not set GW_MANAGED_READER, so it can be launched while gw-system is stopped.
# Pass -d for KOReader debug timing lines (loading/rendering took).
set -eu
debug=
if [ "${1:-}" = "-d" ]; then debug=-d; shift; fi
[ $# -eq 1 ] || { echo "usage: $0 [-d] DOCUMENT" >&2; exit 2; }
[ -r "$1" ] || { echo "not readable: $1" >&2; exit 2; }
compat=/opt/reader-compat
reader=/opt/koreader
export LD_LIBRARY_PATH="$compat:$reader/libs"
export HOME=/root LANG=C.UTF-8 LC_ALL=C.UTF-8
unset GW_MANAGED_READER
cd "$reader"
exec "$compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" \
    ./luajit native.lua $debug "$1"
