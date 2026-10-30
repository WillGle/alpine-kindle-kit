#!/bin/sh
# Execute only from the verified staging directory after safely ejecting USB.
set -eu
stage=/mnt/us/gleworks-stage
base=/mnt/us/gleworks
private=/var/local/gleworks
q=/mnt/us/gleworks-quarantine-$(date +%Y%m%d%H%M%S)-$$
die() { echo "$*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || die 'Root required.'
[ "$(uname -m)" = armv7l ] || die 'Expected PW3 ARMv7.'
grep -q 'Wario Board' /proc/cpuinfo || die 'Unexpected Kindle board.'
[ -f "$stage/READY" ] || die 'No complete staging bundle.'
[ ! -e "$base" ] && [ ! -L "$base" ] || die 'GleWorks already exists. Do not overwrite it.'
[ ! -e "$private" ] && [ ! -L "$private" ] || die 'Private state already exists. Review the previous attempt first.'
[ ! -e "$q" ] || die 'Quarantine path exists.'
[ ! -e /mnt/us/unlocked/awake.on ] && [ ! -e /mnt/us/unlocked/dash.on ] || die 'Old layer has an explicit awake/dashboard session; stop it first.'
# USB must be ejected before invoking this from the Kindle library.
options=$(awk '$2 == "/mnt/us" {print $4}' /proc/mounts)
[ -n "$options" ] || die 'User storage is not mounted.'
case ,$options, in *,ro,*) die 'User storage is read-only.' ;; esac
for proc in /proc/[0-9]*/cmdline; do
    [ -r "$proc" ] || continue
    if tr '\000' '\n' <"$proc" 2>/dev/null | grep -E '(^|/)(reader.lua|kpm)$' >/dev/null; then
        die 'Close KOReader and the package manager before installing.'
    fi
done
(cd "$stage" && sha256sum -c SHA256SUMS) || die 'Bundle checksum mismatch; nothing installed.'
# Refuse unexpected device layout. Firmware, jailbreak and root filesystem stay intact.
[ -d /mnt/us/koreader ] && [ ! -L /mnt/us/koreader ] || die 'Expected existing KOReader directory.'
for path in /mnt/us/unlocked /mnt/us/kmc/kpm/packages/ku-http /mnt/us/kmc/kpm/packages/ku-tailscale /mnt/us/kmc/kpm/packages/koreader; do
    [ -d "$path" ] && [ ! -L "$path" ] || die "Unexpected original package path: $path"
done
free=$(df -k /var/local | awk 'END {print $4}')
[ "$free" -ge 4096 ] || die 'Less than 4 MiB free for private state.'
# Acquire a migration lock; stale locks require review, never automatic deletion.
mkdir /tmp/gleworks-migration.lock 2>/dev/null || die 'Migration already running or previously interrupted.'
trap 'rmdir /tmp/gleworks-migration.lock' EXIT
trap 'exit 1' HUP INT TERM
umask 077
mkdir "$q"
export GLE_QUARANTINE="$q"
echo "$q" > /mnt/us/gleworks-last-migration.txt
# Load the new runtime before touching the old application layer.
chmod +x "$stage/koreader/luajit"
(cd "$stage/koreader" && ./luajit -e 'require("setupkoenv")' "$stage/gleworks/device/package-records.lua" --check)
mkdir "$private" "$private/tailscale"
chmod 700 "$private" "$private/tailscale"
# Both renames are on the same user partition. The original reader is retained whole.
mv /mnt/us/koreader "$q/koreader"
mv "$stage/koreader" /mnt/us/koreader
mv "$stage/gleworks" "$base"
export GLE_KINDLE_HOME="$base" GLE_KINDLE_PRIVATE="$private"
rollback_hint() {
    echo "Migration stopped. Original files are retained in $q."
    echo 'Review the error; use the reviewed rollback script before retrying.'
}
trap 'result=$?; rmdir /tmp/gleworks-migration.lock; if [ "$result" != 0 ]; then rollback_hint; fi' EXIT
chmod +x /mnt/us/koreader/luajit /mnt/us/koreader/koreader.sh /mnt/us/koreader/reader.lua "$base/bin/tailscale" "$base/bin/tailscaled"
sh "$base/scripts/kindle-services.sh" init
sh "$base/scripts/kindle-services.sh" preflight
# Carry over reader preferences only; no plugins, hooks, runtime or OTA payload.
for name in settings.reader.lua; do
    if [ -f "$q/koreader/$name" ] && [ ! -L "$q/koreader/$name" ]; then
        cp "$q/koreader/$name" "/mnt/us/koreader/$name"
    fi
done
sh "$base/scripts/kindle-services.sh" reader-profile
# Prove the new service binds; port conflicts abort before old files are removed.
# If the original HTTP service is active, stop its verified executable first.
old_awake=0
stop_old() {
    expected=$1
    for proc in /proc/[0-9]*; do
        [ "$(readlink "$proc/exe" 2>/dev/null)" = "$expected" ] || continue
        old_awake=1
        pid=${proc##*/}; kill "$pid"
        i=0
        while kill -0 "$pid" 2>/dev/null && [ "$i" -lt 10 ]; do sleep 1; i=$((i+1)); done
        kill -0 "$pid" 2>/dev/null && die "Old service did not exit: $expected"
    done
}
stop_old /mnt/us/kmc/kpm/packages/ku-http/bin/ku-httpd
stop_old /mnt/us/unlocked/tailscale/bin/tailscaled
if [ "$old_awake" = 1 ]; then
    lipc-set-prop com.lab126.powerd preventScreenSaver 0
fi
# A stale original socket alone is harmless after its owning process has exited.
if [ -S /tmp/ku-tailscaled.sock ]; then rm /tmp/ku-tailscaled.sock; fi
sh "$base/scripts/kindle-services.sh" http-start
sh "$base/scripts/kindle-services.sh" http-stop
# VPN startup must succeed before cutover; fresh enrollment is a later user action.
sh "$base/scripts/kindle-services.sh" vpn-start
sh "$base/scripts/kindle-services.sh" vpn-stop
(cd /mnt/us/koreader && ./luajit -e 'require("setupkoenv")' "$base/device/package-records.lua" --remove)
# SQL rollback uses the snapshot and durable mutation marker, including a crash
# immediately after COMMIT. No hash of the entire live database is required.
# Keep the complete original app state for rollback; it contains private VPN keys.
mkdir "$q/packages" "$q/documents"
mv /mnt/us/kmc/kpm/packages/ku-http "$q/packages/ku-http"
mv /mnt/us/kmc/kpm/packages/ku-tailscale "$q/packages/ku-tailscale"
mv /mnt/us/kmc/kpm/packages/koreader "$q/packages/koreader"
mv /mnt/us/unlocked "$q/unlocked"
for name in ku-setup.sh ku-http.sh ku-http-settings.sh ku-tailscale.sh KOReader.sh; do
    if [ -e "/mnt/us/documents/$name" ]; then mv "/mnt/us/documents/$name" "$q/documents/$name"; fi
done
# Remove exactly the port rule installed by the old HTTP launcher, if present.
if command -v iptables >/dev/null 2>&1; then
    while iptables -C INPUT -p tcp --dport 8080 -j ACCEPT 2>/dev/null; do
        iptables -D INPUT -p tcp --dport 8080 -j ACCEPT
done
fi
for script in "$base"/scriptlets/gleworks-*.sh; do
    cp "$script" /mnt/us/documents/
done
cp "$base/scripts/rollback.sh" /mnt/us/documents/gleworks-rollback.sh
printf '%s\n' "$q" > "$private/quarantine"
sync
echo 'Installation and old-layer quarantine completed. All our network services are stopped.'
echo 'Next: test Reader, enroll VPN, check tailnet policy, then test Transfer from your computer.'
echo 'Quarantine is on USB and contains old credentials; protect it and remove it after acceptance.'
