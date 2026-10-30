#!/bin/sh
# Name: GleWorks Kernel Load Batch
# Author: GleWorks
# Staging and cleanup only. No execution command exists in the supplied module.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-kexec-load-batch.sh'
fi
umask 077
base=/mnt/us/documents
name=gleworks_kexec_load
out=/mnt/us/gleworks-kexec-load-batch-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec >"$out/trial.log" 2>&1
load_attempted=0
work=
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    if [ "$load_attempted" = 1 ] && grep -q "^$name " /proc/modules; then
        cat /proc/modules > "$out/modules-before-unload.txt" || true
        cat /proc/meminfo > "$out/meminfo-before-unload.txt" || true
        dmesg > "$out/dmesg-before-unload.txt" || true
        if rmmod "$name"; then
            :
        else
            echo 'STOP: normal rmmod failed; module may remain. No forced retry.'
            result=1
        fi
    fi
    cat /proc/modules > "$out/modules-after.txt" || result=1
    cat /proc/meminfo > "$out/meminfo-after.txt" || result=1
    cat /proc/sys/kernel/tainted > "$out/tainted-after.txt" || result=1
    dmesg > "$out/dmesg-after.txt" || result=1
    if grep -q "^$name " /proc/modules; then result=1; fi
    if [ -n "$work" ]; then
        rm -f "$work/helper" "$work/device"
        rmdir "$work" || true
    fi
    cd "$out"
    for file in ./*.txt ./*helper*.log; do
        [ -f "$file" ] || continue
        sha256sum "$file"
    done > SHA256SUMS
    if [ "$result" = 0 ]; then
        echo 'COMPLETE: batch client and normal module unload returned; logs require review.' > COMPLETE.txt
    else
        echo 'INCOMPLETE: see trial/helper/kernel logs; no forced retry performed.' > INCOMPLETE.txt
    fi
    echo "Batch exit status=$result; no kernel execution, reboot or firmware write requested."
    sync
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
date -u
uname -srvm > "$out/kernel-version.txt"
for tool in insmod rmmod dmesg sha256sum gzip awk grep mknod mktemp; do
    command -v "$tool" >/dev/null
done
[ "$(uname -r)" = 3.0.35-lab126 ] || exit 1
[ "$(cat /proc/sys/kernel/modules_disabled)" = 0 ] || exit 1
[ "$(cat /proc/sys/kernel/tainted)" = 1 ] || exit 1
config_hash=$(gzip -cd /proc/config.gz | sha256sum | awk '{print $1}')
[ "$config_hash" = ee29931255d83b7f13d06e302c75a899b826431ec439bfc0a9b0c54ec6030145 ] || exit 1
if grep -q "^$name " /proc/modules; then exit 1; fi
for pair in 'gleworks_kexec_load.ko:@MODULE_HASH@' 'gleworks-load-helper:@HELPER_HASH@' 'gleworks-load-image.bin:@IMAGE_HASH@'; do
    file=${pair%%:*}
    expected=${pair#*:}
    [ -f "$base/$file" ] && [ ! -L "$base/$file" ] || exit 1
    actual=$(sha256sum "$base/$file" | awk '{print $1}')
    [ "$actual" = "$expected" ] || exit 1
    echo "$actual  $file" >> "$out/inputs.txt"
done
space=$(df -k /mnt/us | awk 'END {print $4}')
case "$space" in ''|*[!0-9]*) exit 1;; esac
[ "$space" -ge 32768 ] || { echo 'STOP: need 32 MiB free document storage'; exit 1; }
cat /proc/modules > "$out/modules-before.txt"
cat /proc/meminfo > "$out/meminfo-before.txt"
cat /proc/sys/kernel/tainted > "$out/tainted-before.txt"
dmesg > "$out/dmesg-before.txt"
free_kb=$(awk '/^MemFree:/ {print $2}' /proc/meminfo)
case "$free_kb" in ''|*[!0-9]*) exit 1;; esac
[ "$free_kb" -ge 12288 ] || { echo 'STOP: need 12 MiB currently free RAM; no module loaded'; exit 1; }
work=$(mktemp -d /tmp/gleworks-load-batch.XXXXXX)
cp "$base/gleworks-load-helper" "$work/helper"
chmod 700 "$work/helper"
[ "$(sha256sum "$work/helper" | awk '{print $1}')" = '@HELPER_HASH@' ] || exit 1
# Confirm the static ARM executable starts before loading any kernel code.
if "$work/helper" > "$out/helper-smoke.log" 2>&1; then
    echo 'STOP: unexpected smoke result'; exit 1
else
    smoke_result=$?
    [ "$smoke_result" = 1 ] || exit 1
    grep -q '^usage: batch-helper DEVICE IMAGE$' "$out/helper-smoke.log"
fi
sync
# Arm cleanup first: a signal can arrive after successful insmod but before
# the next shell statement. Actual module presence decides normal removal.
load_attempted=1
insmod "$base/gleworks_kexec_load.ko"
cat /proc/modules > "$out/modules-loaded.txt"
minor=$(awk '$2 == "gleworks_kexec_load" {print $1}' /proc/misc)
case "$minor" in ''|*[!0-9]*) exit 1;; esac
mknod "$work/device" c 10 "$minor"
chmod 600 "$work/device"
"$work/helper" "$work/device" "$base/gleworks-load-image.bin" > "$out/batch-helper.log" 2>&1
grep -q '^COMPLETE: all batch checks passed; no kernel execution requested$' "$out/batch-helper.log"
# EXIT cleanup captures the final state, unloads normally and hashes evidence.
