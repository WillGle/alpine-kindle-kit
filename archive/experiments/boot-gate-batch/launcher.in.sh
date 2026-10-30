#!/bin/sh
# Name: GleWorks Boot Gate Batch
# Author: GleWorks
# Existing checks first; bounded watchdog reset trial last. No kernel handoff.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-boot-gate-batch.sh'
fi
umask 077
base=/mnt/us/documents
pending=/mnt/us/gleworks-boot-gate-pending.txt
watchdog_pending=/mnt/us/gleworks-watchdog-pending.txt
mkdir /tmp/gleworks-boot-gate.lock || exit 1
out=/mnt/us/gleworks-boot-gate-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec > "$out/batch.log" 2>&1
cleanup() {
    code=$?
    trap - EXIT HUP INT TERM
    cat /proc/modules > "$out/modules-after.txt" || true
    cat /proc/meminfo > "$out/meminfo-after.txt" || true
    (cd "$out"; for f in ./*.txt; do
        [ -f "$f" ] || continue
        sha256sum "$f"
    done) > "$out/SHA256SUMS"
    rmdir /tmp/gleworks-boot-gate.lock || true
    echo "Batch exit=$code. No native execution interface exists in this package."
    sync
    exit "$code"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
cd "$base"
# Embedded expected hashes prevent mixing launchers and module/helper builds.
@HASH_CHECKS@
cat /proc/sys/kernel/random/boot_id > "$out/boot-id.txt"
cat /proc/modules > "$out/modules-before.txt"
cat /proc/meminfo > "$out/meminfo-before.txt"
cat /proc/interrupts > "$out/interrupts.txt"
cat /proc/uptime > "$out/uptime.txt"
for file in /proc/cmdline /proc/iomem /proc/mounts /proc/buddyinfo /proc/vmstat \
    /sys/power/state /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor \
    /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq \
    /sys/class/thermal/thermal_zone*/type /sys/class/thermal/thermal_zone*/temp; do
    [ -f "$file" ] && [ -r "$file" ] || continue
    echo "$file"
    cat "$file" || true
done > "$out/hardware-state.txt"
echo 'NO-GO native execution: handoff is unimplemented; watchdog timeout/return needs review.' > "$out/DECISION.txt"
# Any pending trial selects collection only, even if a reset interrupted the
# wrapper before the watchdog child wrote its own intention marker.
if [ -e "$pending" ] || [ -e "$watchdog_pending" ]; then
    echo 'COLLECTION ONLY: never rerun staging or arm a reset on this branch.'
    if [ -e "$watchdog_pending" ]; then
        if sh "$base/gleworks-watchdog-trial.sh" --collect-only; then
            echo '0' > "$out/collection-exit.txt"
        else
            code=$?
            echo "$code" > "$out/collection-exit.txt"
            exit "$code"
        fi
    else
        echo 'Interrupted without watchdog intention marker; reset cause unproven.' > "$out/collection-note.txt"
    fi
    rm -f "$pending"
    exit 0
fi
# Do not add an active experiment on top of an unfinished earlier one.
if grep -q '^gleworks_.* ' /proc/modules; then
    echo 'STOP: a GleWorks module is already loaded; no active trial started.'
    exit 1
fi
echo 'STEP 1: readonly state captured.'
echo 'STEP 2: normal RAM load/fault/ownership/checksum/cleanup batch.'
if sh "$base/gleworks-boot-readiness.sh"; then
    echo '0' > "$out/staging-exit.txt"
else
    code=$?
    echo "$code" > "$out/staging-exit.txt"
    echo 'Staging declined/failed; preserve child evidence and continue only if cleanup is complete.'
fi
printf '%s\n' /mnt/us/gleworks-boot-readiness-* > "$out/staging-captures.txt"
if grep -q '^gleworks_.* ' /proc/modules; then
    echo 'STOP: module remains after staging; watchdog trial blocked.'
    exit 1
fi
echo 'STEP 3: watchdog abort check then bounded runnable reset candidate.'
basename "$out" > "$pending"
sync
if sh "$base/gleworks-watchdog-trial.sh"; then
    echo '0' > "$out/watchdog-exit.txt"
else
    code=$?
    echo "$code" > "$out/watchdog-exit.txt"
fi
printf '%s\n' /mnt/us/gleworks-watchdog-trial-* > "$out/watchdog-captures.txt"
# Reaching here means the child returned in this boot, not a demonstrated reset.
rm -f "$pending"
echo 'NO-GO: no watchdog reset observed in this run; inspect both child captures.' > "$out/DECISION.txt"
exit 1
