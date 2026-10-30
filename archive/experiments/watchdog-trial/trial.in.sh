#!/bin/sh
# Name: GleWorks Watchdog Trial
# Author: GleWorks
# This trial may reset Kindle. No flash, boot selector or native handoff.
set -eu
case "${1-}" in ''|--collect-only) ;; *) exit 1;; esac
if [ "$(id -u)" != 0 ]; then
    if [ "${1-}" = --collect-only ]; then
        exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-watchdog-trial.sh --collect-only'
    fi
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-watchdog-trial.sh'
fi
umask 077
base=/mnt/us/documents
name=gleworks_watchdog_guard
pending=/mnt/us/gleworks-watchdog-pending.txt
mkdir /tmp/gleworks-watchdog.lock || exit 1
out=/mnt/us/gleworks-watchdog-trial-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec >"$out/trial.log" 2>&1
load_attempted=0
prevented=0
work=
stage=initial-checks
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    if [ "$load_attempted" = 1 ] && grep -q "^$name " /proc/modules; then
        if ! rmmod "$name"; then
            echo 'STOP: normal unload failed; do not force unload.'
            result=1
        fi
    fi
    if [ "$prevented" = 1 ]; then
        lipc-set-prop com.lab126.powerd preventScreenSaver 0 || result=1
    fi
    cat /proc/modules > "$out/modules-after.txt" || result=1
    cat /proc/sys/kernel/tainted > "$out/tainted-after.txt" || result=1
    dmesg > "$out/dmesg-after.txt" || result=1
    if [ -n "$work" ]; then
        rm -f "$work/helper" "$work/reader" "$work/device"
        rmdir "$work" || true
    fi
    (cd "$out"; for file in ./*.txt ./*helper*.log; do
        [ -f "$file" ] || continue
        sha256sum "$file"
    done) > "$out/SHA256SUMS"
    rmdir /tmp/gleworks-watchdog.lock || true
    echo "Exit=$result; last stage=$stage; review evidence. Native Linux was not executed."
    sync
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
for tool in insmod rmmod dmesg sha256sum gzip awk grep mknod mktemp stat cmp basename lipc-get-prop lipc-set-prop; do
    command -v "$tool" >/dev/null
done
[ "$(uname -r)" = 3.0.35-lab126 ] || exit 1
[ "$(cat /proc/sys/kernel/tainted)" = 1 ] || exit 1
[ "$(gzip -cd /proc/config.gz | sha256sum | awk '{print $1}')" = ee29931255d83b7f13d06e302c75a899b826431ec439bfc0a9b0c54ec6030145 ] || exit 1
for pair in 'gleworks_watchdog_guard.ko:@MODULE_HASH@' 'gleworks-watchdog-helper:@HELPER_HASH@' 'gleworks-watchdog-read:@READER_HASH@'; do
    file=${pair%%:*}; expected=${pair#*:}
    [ -f "$base/$file" ] && [ ! -L "$base/$file" ] || exit 1
    [ "$(sha256sum "$base/$file" | awk '{print $1}')" = "$expected" ] || exit 1
    echo "$expected  $file" >> "$out/inputs.txt"
done
cat /proc/sys/kernel/random/boot_id > "$out/boot-id.txt"
cat /proc/uptime > "$out/uptime.txt"
cat /proc/modules > "$out/modules-before.txt"
cat /proc/meminfo > "$out/meminfo-before.txt"
dmesg > "$out/dmesg-before.txt"
awk '$2 == "/" {print}' /proc/mounts > "$out/root-mounts.txt"
grep -q '^020bc000-020bffff : imx2-wdt.0$' /proc/iomem || exit 1
work=$(mktemp -d /tmp/gleworks-watchdog.XXXXXX)
cp "$base/gleworks-watchdog-read" "$work/reader"
chmod 700 "$work/reader"
[ "$(sha256sum "$work/reader" | awk '{print $1}')" = '@READER_HASH@' ] || exit 1
"$work/reader" > "$out/registers.txt"
# A second launch after reset collects evidence only; it cannot arm another trial.
if [ -e "$pending" ]; then
    [ -f "$pending" ] && [ ! -L "$pending" ] || exit 1
    prior=$(cat "$pending")
    case "$prior" in gleworks-watchdog-trial-[0-9]*) ;; *) exit 1;; esac
    case "$prior" in *[!a-z0-9-]*) exit 1;; esac
    [ -d "/mnt/us/$prior" ] && [ ! -L "/mnt/us/$prior" ] || exit 1
    [ -f "/mnt/us/$prior/boot-id.txt" ] && [ ! -L "/mnt/us/$prior/boot-id.txt" ] || exit 1
    echo "$prior" > "$out/prior-capture.txt"
    if cmp -s "$out/boot-id.txt" "/mnt/us/$prior/boot-id.txt"; then
        echo 'UNPROVEN: boot ID unchanged; no reset demonstrated.' > "$out/RESULT.txt"
    else
        comparison=$?
        [ "$comparison" = 1 ] || { echo 'STOP: boot ID comparison failed; no reset claim.'; exit 1; }
        echo 'STOCK RETURNED: boot ID changed. Review WRSR/reset cause before claiming watchdog recovery.' > "$out/RESULT.txt"
    fi
    rm "$pending"
    exit 0
fi
if [ "${1-}" = --collect-only ]; then
    echo 'COLLECTION ONLY: no pending intention; no reset cause demonstrated.' > "$out/RESULT.txt"
    exit 0
fi
stage=watchdog-preflight
# Capture every remaining prerequisite before checking it, without opening
# the watchdog. A declined check must be diagnosable from this single run.
cat /proc/sys/kernel/modules_disabled > "$out/modules-disabled.txt"
cat /proc/misc > "$out/misc-devices.txt"
{
    ls -ld /dev/watchdog /dev/watchdog0 /dev/misc/watchdog 2>&1 || true
    stat -c '%t:%T' /dev/misc/watchdog 2>&1 || true
    lipc-get-prop com.lab126.powerd battLevel 2>&1 || true
    lipc-get-prop com.lab126.powerd preventScreenSaver 2>&1 || true
} > "$out/preflight-details.txt"
# Trace only fixed preflight commands and their non-secret observed values.
set -x
stage=modules-enabled
[ "$(cat /proc/sys/kernel/modules_disabled)" = 0 ] || exit 1
stage=root-readonly
grep -q ' / ext3 ro,' "$out/root-mounts.txt" || exit 1
stage=watchdog-node-type
[ -c /dev/misc/watchdog ] && [ ! -L /dev/misc/watchdog ] || exit 1
stage=watchdog-node-number
[ "$(stat -c '%t:%T' /dev/misc/watchdog)" = a:82 ] || exit 1
stage=watchdog-registers
grep -q '^WDOG1_WCR=0000773d$' "$out/registers.txt" || exit 1
grep -q '^WDOG1_WICR=00008004$' "$out/registers.txt" || exit 1
grep -q '^SRC_GPR9=00000000$' "$out/registers.txt" || exit 1
grep -q '^SRC_GPR10=00000000$' "$out/registers.txt" || exit 1
if grep -q "^$name " /proc/modules; then exit 1; fi
stage=free-ram
[ "$(awk '/^MemFree:/ {print $2}' /proc/meminfo)" -ge 4096 ] || exit 1
# Require a charged unit; this does not change charging or power policy.
stage=battery
battery=$(lipc-get-prop com.lab126.powerd battLevel)
case "$battery" in ''|*[!0-9]*) exit 1;; esac
[ "$battery" -ge 40 ] || { echo 'STOP: charge battery to at least 40%'; exit 1; }
stage=screen-saver-state
[ "$(lipc-get-prop com.lab126.powerd preventScreenSaver)" = 0 ] || exit 1
prevented=1
lipc-set-prop com.lab126.powerd preventScreenSaver 1
[ "$(lipc-get-prop com.lab126.powerd preventScreenSaver)" = 1 ] || exit 1
set +x
stage=helper-smoke
cp "$base/gleworks-watchdog-helper" "$work/helper"
chmod 700 "$work/helper"
[ "$(sha256sum "$work/helper" | awk '{print $1}')" = '@HELPER_HASH@' ] || exit 1
if "$work/helper" > "$out/helper-smoke.log" 2>&1; then exit 1; else
    [ "$?" = 1 ] || exit 1
    grep -q '^usage: watchdog-helper DEVICE$' "$out/helper-smoke.log"
fi
load_attempted=1
stage=module-load
insmod "$base/gleworks_watchdog_guard.ko"
minor=$(awk '$2 == "gleworks_watchdog_guard" {print $1}' /proc/misc)
case "$minor" in ''|*[!0-9]*) exit 1;; esac
mknod "$work/device" c 10 "$minor"
chmod 600 "$work/device"
# Intention is durable before arming. This marker is not proof of a reset.
basename "$out" > "$pending"
echo 'PLANNED: abort/restore selfcheck then 150s timeout trial; no ordinary reboot command.' > "$out/INTENT.txt"
(cd "$out"; sha256sum boot-id.txt registers.txt INTENT.txt inputs.txt > PRE-RESET.sha256)
sync
stage=watchdog-trial
if "$work/helper" "$work/device" > "$out/watchdog-helper.log" 2>&1; then
    echo 'Unexpected helper success; recovery unproven.' > "$out/RESULT.txt"
else
    code=$?
    echo "NO RESET: helper returned $code; recovery unproven." > "$out/RESULT.txt"
fi
rm "$pending"
exit 1
