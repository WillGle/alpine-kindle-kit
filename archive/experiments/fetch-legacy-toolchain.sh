#!/bin/sh
# Download a GCC 4.9 candidate for host review; do not extract or execute it.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
stem=x86_64-gcc-4.9.4-nolibc-arm-linux-gnueabi.tar
url=https://kernel.org/pub/tools/crosstool/files/bin/x86_64/4.9.4
for path in "$base/$stem.xz" "$base/$stem.xz.partial" "$base/$stem.sign" "$base/$stem.sign.partial" "$base/$stem.xz.sha256"; do
    if [ -e "$path" ] || [ -L "$path" ]; then
        echo "Retain existing acquisition for review: $path" >&2
        exit 1
    fi
done
curl --fail --location --proto '=https' --proto-redir '=https' \
    --connect-timeout 20 --max-filesize 134217728 \
    "$url/$stem.xz" -o "$base/$stem.xz.partial"
xz -t "$base/$stem.xz.partial"
curl --fail --location --proto '=https' --proto-redir '=https' \
    --connect-timeout 20 --max-filesize 1048576 \
    "$url/$stem.sign" -o "$base/$stem.sign.partial"
mv "$base/$stem.xz.partial" "$base/$stem.xz"
mv "$base/$stem.sign.partial" "$base/$stem.sign"
(cd "$base" && sha256sum "$stem.xz") > "$base/$stem.xz.sha256"
echo 'Toolchain archive integrity checked; detached signature saved, verification pending.'
echo 'GCC 4.9.4 is a candidate, not the exact GCC 4.9.1 used by the running kernel.'
echo 'No compiler execution, module build/loading or device operation performed.'
