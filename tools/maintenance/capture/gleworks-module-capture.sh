#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Name: GleWorks Module Capture
# Author: GleWorks
# Capture ABI evidence; never load modules or alter kernel/boot state.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-module-capture.sh'
fi
umask 077
[ "$(uname -r)" = 3.0.35-lab126 ] || exit 1
command -v sha256sum >/dev/null
command -v tar >/dev/null
free_kb=$(df -k /mnt/us | awk 'END {print $4}')
case "$free_kb" in ''|*[!0-9]*) exit 1;; esac
[ "$free_kb" -ge 131072 ] || { echo 'Need 128 MiB free'; exit 1; }
module_dir=/lib/modules/3.0.35-lab126
[ -d "$module_dir" ] || exit 1
module_kb=$(du -sk "$module_dir" | awk '{print $1}')
case "$module_kb" in ''|*[!0-9]*) exit 1;; esac
[ "$module_kb" -le 65536 ] || { echo 'Module inventory exceeds expected bound'; exit 1; }
out=/mnt/us/gleworks-module-capture-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec >"$out/capture.log" 2>&1
trap 'echo "Capture incomplete"; exit 1' HUP INT TERM
ulimit -f 131072
date -u
uname -srvm
cat /proc/version > "$out/kernel-version.txt"
cat /proc/modules > "$out/loaded-modules.txt"
cat /proc/kallsyms > "$out/kallsyms.txt"
[ -s "$out/kallsyms.txt" ] || exit 1
cat /proc/iomem > "$out/iomem.txt"
cat /proc/meminfo > "$out/meminfo.txt"
for item in modules_disabled kptr_restrict tainted; do
    if [ -r "/proc/sys/kernel/$item" ]; then
        cat "/proc/sys/kernel/$item" > "$out/$item.txt"
    fi
done
if [ -r /proc/config.gz ]; then
    cat /proc/config.gz > "$out/config.gz"
fi
# tar keeps symlinks as links; it does not follow them into other filesystems.
tar -C / -cf "$out/stock-modules.tar" lib/modules/3.0.35-lab126
[ -s "$out/stock-modules.tar" ] || exit 1
tar -tf "$out/stock-modules.tar" > "$out/archive-members.txt"
grep -q '\.ko$' "$out/archive-members.txt" || { echo 'No stock module binaries captured'; exit 1; }
cd "$out"
sha256sum ./*.txt stock-modules.tar > SHA256SUMS
if [ -f config.gz ]; then sha256sum config.gz >> SHA256SUMS; fi
sha256sum -c SHA256SUMS
echo 'COMPLETE: ABI evidence and module archive copied and hashed; compatibility not yet validated.' > COMPLETE.txt
echo 'No module loading, memory writes, boot changes, reboot or firmware operations.'
