#!/bin/sh
# Name: GleWorks Waveform Capture
# Author: GleWorks
# Read panel evidence; write only new files on user storage.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-waveform-capture.sh'
fi
umask 077
command -v sha256sum >/dev/null
command -v cmp >/dev/null
[ -r /proc/eink/waveform/data ] || { echo 'Waveform endpoint missing'; exit 1; }
free_kb=$(df -k /mnt/us | awk 'END {print $4}')
case "$free_kb" in ''|*[!0-9]*) exit 1;; esac
[ "$free_kb" -ge 65536 ] || { echo 'Need 64 MiB free'; exit 1; }
out=/mnt/us/gleworks-waveform-capture-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec >"$out/capture.log" 2>&1
trap 'echo "Capture incomplete"; exit 1' HUP INT TERM
date -u
uname -srvm
# Old proc handlers can return short reads; cat continues until EOF.
# Bound output size in case installed firmware exposes different semantics.
ulimit -f 16384
capture() {
    src=$1
    name=$2
    cat "$src" > "$out/$name"
    [ -s "$out/$name" ] || { echo "Empty capture: $name"; exit 1; }
    cat "$src" > "$out/$name.second"
    cmp "$out/$name" "$out/$name.second"
    wc -c "$out/$name"
}
capture /proc/eink/waveform/data active-waveform.bin
if [ -r /proc/eink/panel/waveform/data ]; then
    capture /proc/eink/panel/waveform/data panel-waveform.bin
else
    echo 'Panel waveform endpoint missing; active waveform only.'
fi
for group in waveform panel/waveform; do
    prefix=$(echo "$group" | tr / -)
    for item in info version human_version embedded_checksum computed_checksum source; do
        src=/proc/eink/$group/$item
        if [ -r "$src" ]; then
            cat "$src" > "$out/$prefix-$item.txt"
        fi
    done
done
# This endpoint is decoded panel metadata, not a raw SPI-flash backup.
if [ -r /proc/eink/panel/data ]; then
    cat /proc/eink/panel/data > "$out/panel-decoded.txt"
fi
cd "$out"
sha256sum ./*.bin ./*.second > SHA256SUMS
for metadata in ./*.txt; do
    [ -f "$metadata" ] || continue
    sha256sum "$metadata" >> SHA256SUMS
done
sha256sum -c SHA256SUMS
echo 'COMPLETE: available waveform bytes captured twice identically and hashed; format and vendor checksums still require host review.' > COMPLETE.txt
echo 'No panel writes, idme, reboot or boot changes. Not a full panel-flash backup.'
