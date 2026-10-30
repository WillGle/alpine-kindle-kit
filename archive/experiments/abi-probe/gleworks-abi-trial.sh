#!/bin/sh
# Name: GleWorks ABI Trial
# Author: GleWorks
# One inert module load/unload, with persistent evidence; no kernel handoff.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-abi-trial.sh'
fi
umask 077
name=gleworks_abi_probe
module=/mnt/us/documents/gleworks_abi_probe.ko
[ "$(uname -r)" = 3.0.35-lab126 ] || exit 1
[ "$(cat /proc/sys/kernel/modules_disabled)" = 0 ] || exit 1
# Stop if the baseline changed since the capture; never clear taint or logs.
[ "$(cat /proc/sys/kernel/tainted)" = 1 ] || exit 1
for tool in insmod rmmod dmesg sha256sum gzip awk grep; do
    command -v "$tool" >/dev/null
done
[ -f "$module" ] && [ ! -L "$module" ] || exit 1
[ "$(sha256sum "$module" | awk '{print $1}')" = 786a8c7fc58b34adfa66c4c6cc745d89c408710da0782f3067a7df8e3d2d063f ] || exit 1
[ -r /proc/config.gz ] || exit 1
# Match the known device configuration, independently of module vermagic.
config_hash=$(gzip -cd /proc/config.gz | sha256sum | awk '{print $1}')
[ "$config_hash" = ee29931255d83b7f13d06e302c75a899b826431ec439bfc0a9b0c54ec6030145 ] || exit 1
if grep -q "^$name " /proc/modules; then exit 1; fi
free_kb=$(df -k /mnt/us | awk 'END {print $4}')
case "$free_kb" in ''|*[!0-9]*) exit 1;; esac
[ "$free_kb" -ge 32768 ] || exit 1
out=/mnt/us/gleworks-abi-trial-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec >"$out/trial.log" 2>&1
ulimit -f 16384
loaded=0
cleanup() {
    if [ "$loaded" = 1 ]; then
        rmmod "$name" || return 1
        loaded=0
    fi
}
trap 'cleanup; exit 1' HUP INT TERM
date -u
uname -srvm
sha256sum "$module" > "$out/module.sha256"
cat /proc/modules > "$out/modules-before.txt"
cat /proc/sys/kernel/tainted > "$out/tainted-before.txt"
dmesg > "$out/dmesg-before.txt"
# Flush the baseline before entering kernel module code. A kernel hang cannot
# be caught or recovered by this shell; the file is evidence, not a watchdog.
sync
if insmod "$module"; then
    loaded=1
else
    dmesg > "$out/dmesg-load-failed.txt" || true
    echo 'STOP: normal insmod rejected the candidate; no forced retry.'
    exit 1
fi
cat /proc/modules > "$out/modules-loaded.txt" || true
dmesg > "$out/dmesg-loaded.txt" || true
sleep 1 || true
if ! cleanup; then
    echo 'STOP: normal unload failed; module may remain loaded. No forced retry.'
    dmesg > "$out/dmesg-unload-failed.txt" || true
    exit 1
fi
cat /proc/modules > "$out/modules-after.txt"
cat /proc/sys/kernel/tainted > "$out/tainted-after.txt"
dmesg > "$out/dmesg-after.txt"
if grep -q "^$name " /proc/modules; then exit 1; fi
grep -q 'gleworks_abi_probe: loaded; module descriptor=292 bytes' "$out/dmesg-after.txt"
grep -q 'gleworks_abi_probe: unloaded' "$out/dmesg-after.txt"
cd "$out"
sha256sum ./*.txt module.sha256 > SHA256SUMS
echo 'COMPLETE: normal load/unload returned; kernel logs still require review.' > COMPLETE.txt
echo 'No kexec, bootmode change, firmware write or reboot requested.'
sync
