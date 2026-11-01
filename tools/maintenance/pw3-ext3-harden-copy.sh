#!/bin/sh
# Harden a COPY of an ext3 PW3 root image. Never edits the source image and
# refuses to run on a device node or a mounted filesystem.
#
# Usage: pw3-ext3-harden-copy.sh SOURCE.ext3 OUTPUT.ext3
set -eu
src=${1:?usage: $0 SOURCE.ext3 OUTPUT.ext3}
out=${2:?usage: $0 SOURCE.ext3 OUTPUT.ext3}
[ -f "$src" ] || { echo "not a regular file: $src" >&2; exit 2; }
[ "$src" != "$out" ] || { echo "source and output must differ" >&2; exit 2; }
[ ! -e "$out" ] || { echo "refusing to overwrite: $out" >&2; exit 2; }
case "$src" in /dev/*) echo "refusing device node source" >&2; exit 2;; esac
# Do not operate on a mounted image/device.
for m in /proc/mounts /etc/mtab; do
    [ -r "$m" ] || continue
    awk -v p="$src" '$1==p || $2==p {found=1} END{exit found?0:1}' "$m" &&
        { echo "source is mounted: $src" >&2; exit 2; }
done

command -v tune2fs >/dev/null 2>&1 || { echo "tune2fs missing" >&2; exit 2; }
command -v e2fsck >/dev/null 2>&1 || { echo "e2fsck missing" >&2; exit 2; }

cp --reflink=auto "$src" "$out"
chmod 600 "$out"
echo "source_sha256=$(sha256sum "$src" | awk '{print $1}')"
echo "copy_before=$(sha256sum "$out" | awk '{print $1}')"
tune2fs -o journal_data_ordered -e remount-ro "$out"
e2fsck -fn "$out"
tune2fs -l "$out" | grep -Ei 'Filesystem features|Default mount options|Errors behavior|Filesystem state|Block size|Journal inode'
echo "copy_after=$(sha256sum "$out" | awk '{print $1}')"
