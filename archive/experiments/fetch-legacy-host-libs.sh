#!/bin/sh
# Download host-only compiler dependencies. No build or device access.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/abi-probe/host-inputs
mkdir -p "$base"
fetch() {
    name=$1
    url=$2
    if [ -e "$base/$name" ] || [ -e "$base/$name.partial" ]; then
        echo "Retain existing input for review: $base/$name" >&2
        exit 1
    fi
    curl --fail --location --proto '=https' --proto-redir '=https' \
        --connect-timeout 20 --max-filesize 16777216 \
        "$url" -o "$base/$name.partial"
    mv "$base/$name.partial" "$base/$name"
}
fetch mpfr-3.1.6.tar.xz https://www.mpfr.org/mpfr-3.1.6/mpfr-3.1.6.tar.xz
xz -t "$base/mpfr-3.1.6.tar.xz"
fetch mpfr-3.1.6.tar.xz.asc https://www.mpfr.org/mpfr-3.1.6/mpfr-3.1.6.tar.xz.asc
fetch mpc-1.0.3.tar.gz https://ftp.gnu.org/gnu/mpc/mpc-1.0.3.tar.gz
gzip -t "$base/mpc-1.0.3.tar.gz"
fetch mpc-1.0.3.tar.gz.sig https://ftp.gnu.org/gnu/mpc/mpc-1.0.3.tar.gz.sig
(cd "$base" && sha256sum mpfr-3.1.6.tar.xz mpc-1.0.3.tar.gz) > "$base/SHA256SUMS"
echo 'Host dependency sources downloaded; signatures saved, verification pending.'
echo 'No source execution, module build/loading or device change performed.'
