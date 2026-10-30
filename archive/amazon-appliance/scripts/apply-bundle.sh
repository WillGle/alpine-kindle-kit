#!/bin/sh
# One explicit batch update; no downloader, boot hook or change to private credentials.
set -eu
base=/mnt/us/gleworks
stage=/mnt/us/gleworks-updates
runtime=/tmp/gleworks
q=/mnt/us/gleworks-app-backup-$(date +%Y%m%d%H%M%S)-$$
die() { echo "$*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || die 'Root required.'
for proc in /proc/[0-9]*/cmdline; do
    [ -r "$proc" ] || continue
    if tr '\000' '\n' <"$proc" 2>/dev/null | grep -E '(^|/)reader.lua$' >/dev/null; then die 'Close KOReader before updating.'; fi
done
umask 077
[ ! -L "$runtime" ] || die 'Invalid runtime directory.'
[ -d "$runtime" ] || mkdir "$runtime"
[ "$(stat -c %u "$runtime")" = 0 ] || die 'Runtime directory is not owned by root.'
chmod 700 "$runtime"
[ ! -L "$base" ] && [ ! -L /mnt/us/koreader ] && [ ! -L "$stage" ] || die 'Linked application path needs review.'
mkdir "$runtime/update.lock" 2>/dev/null || die 'An update is running or was interrupted; inspect its record before retrying.'
record_update_failure() {
    [ "$1" != 0 ] || return 0
    recorder=$base/scripts/record-failure.sh
    [ -f "$recorder" ] || recorder=$q/gleworks/scripts/record-failure.sh
    [ ! -f "$recorder" ] || /bin/sh "$recorder" update-apps "$1" activation || :
}
trap 'code=$?;trap - EXIT;record_update_failure "$code";rmdir "$runtime/update.lock" || :;exit "$code"' EXIT
trap 'exit 1' HUP INT TERM
if [ -f "$base/scripts/kindle-services.sh" ]; then
    sh "$base/scripts/kindle-services.sh" http-stop
    sh "$base/scripts/kindle-services.sh" vpn-stop
else [ "${1:-}" = --restore ] || die 'Active controls missing; restore the interrupted update first.';fi
personal_reader() {
    source_reader=$1;target_reader=$2
    for name in settings history scripts screenshots styletweaks fonts/host data/dict data/tessdata; do
        [ ! -L "$source_reader/$name" ] || die 'Linked personal directory needs review.'
        if [ -d "$source_reader/$name" ]; then
            mkdir -p "$target_reader/$name"
            cp -a "$source_reader/$name/." "$target_reader/$name/"
        fi
    done
    for file in "$source_reader"/settings.reader.lua* "$source_reader"/defaults.persistent.lua; do
        [ -f "$file" ] && [ ! -L "$file" ] || continue
        cp -p "$file" "$target_reader/"
    done
}
restore() {
    restore_q=$1
    [ ! -L "$restore_q" ] && [ ! -L "$restore_q/gleworks" ] && [ ! -L "$restore_q/koreader" ] || die 'Linked backup needs review.'
    # Before app-layer rename, the original app layer is still in its active path.
    if [ ! -d "$restore_q/gleworks" ] && [ -d "$base" ]; then
        if [ -d "$restore_q/koreader" ] && [ ! -e /mnt/us/koreader ]; then
            mv "$restore_q/koreader" /mnt/us/koreader
        fi
        [ -d /mnt/us/koreader ] && [ ! -d "$restore_q/koreader" ] || die 'Partial recovery needs review.'
        rm -f "$stage/READY"
        touch "$restore_q/restored";sync
        echo 'Original applications retained; restage the update before retrying.';return
    fi
    [ -d "$restore_q/gleworks" ] && [ -d "$restore_q/koreader" ] || die 'Incomplete previous application backup.'
    [ ! -e "$restore_q/replaced-gleworks" ] && [ ! -e "$restore_q/replaced-koreader" ] || die 'A previous restore needs review.'
    if [ -d /mnt/us/koreader ]; then personal_reader /mnt/us/koreader "$restore_q/koreader";fi
    for name in data files; do
        if [ -d "$base/$name" ]; then
            [ ! -e "$restore_q/gleworks/$name" ] || die 'Personal data destination occupied.'
            mv "$base/$name" "$restore_q/gleworks/$name"
        fi
    done
    if [ -d "$base" ]; then mv "$base" "$restore_q/replaced-gleworks";fi
    if [ -d /mnt/us/koreader ]; then mv /mnt/us/koreader "$restore_q/replaced-koreader";fi
    mv "$restore_q/gleworks" "$base"
    mv "$restore_q/koreader" /mnt/us/koreader
    for script in "$restore_q"/documents/*.sh; do [ ! -f "$script" ] || cp "$script" /mnt/us/documents/; done
    rm -f "$stage/READY"
    touch "$restore_q/restored"
    sync
    echo 'Previous appliance restored. Personal data retained; network services stopped.'
}
if [ "${1:-}" = --restore ]; then
    restore_q=$(cat /mnt/us/gleworks-last-app-update.txt)
    suffix=${restore_q#/mnt/us/gleworks-app-backup-}
    case "$restore_q:$suffix" in /mnt/us/gleworks-app-backup-*:*[!0-9-]*) die 'Unsafe restore record.' ;; esac
    [ "$suffix" != "$restore_q" ] && [ -n "$suffix" ] || die 'Unsafe restore record.'
    restore "$restore_q";exit 0
fi
if [ -f /mnt/us/gleworks-last-app-update.txt ]; then
    previous=$(cat /mnt/us/gleworks-last-app-update.txt)
    case "$previous" in /mnt/us/gleworks-app-backup-*) ;; *) die 'Invalid previous update record.' ;; esac
    [ -f "$previous/completed" ] || [ -f "$previous/restored" ] || die 'A previous update was interrupted; recover it before another update.'
fi
[ -f "$stage/READY" ] || die 'No verified update staged. Prepare and copy a complete update over USB first.'
export GLE_KINDLE_HOME="$base" GLE_KINDLE_PRIVATE=/var/local/gleworks
(cd /mnt/us/koreader && ./luajit -e 'require("setupkoenv")' "$base/device/validate-update.lua" --record)
(cd "$stage" && sha256sum -c SHA256SUMS) || die 'Update checksum mismatch.'
# Validation rejects extra files and links before executing this verified runtime.
chmod +x "$stage/koreader/luajit" "$stage/koreader/reader.lua" "$stage/koreader/koreader.sh" "$stage/gleworks/bin/tailscale" "$stage/gleworks/bin/tailscaled"
(cd "$stage/koreader" && ./luajit -e 'require("setupkoenv"); require("socket"); require("libs/libkoreader-lfs")')
"$stage/gleworks/bin/tailscale" version
"$stage/gleworks/bin/tailscaled" --version
free=$(df -k /mnt/us | awk 'END {print $4}')
reader_size=$(du -sk /mnt/us/koreader | awk '{print $1}')
[ "$free" -gt "$((reader_size+65536))" ] || die 'Insufficient free space for personal settings and recovery.'
[ ! -e "$q" ] || die 'Backup path occupied.'
mkdir "$q" "$q/documents"
printf '%s\n' "$q" > /mnt/us/gleworks-last-app-update.txt
for script in /mnt/us/documents/gleworks-*.sh; do [ ! -f "$script" ] || cp "$script" "$q/documents/"; done
# Restore originals on a handled failure. Power loss still needs the retained record.
activated=no
finish() {
    result=$?
    record_update_failure "$result"
    trap - EXIT
    if [ "$result" != 0 ] && [ "$activated" = no ]; then
        set +e
        recovered=yes
        if [ -d "$q/gleworks" ]; then
            for name in data files; do
                if [ -d "$base/$name" ] && [ ! -e "$q/gleworks/$name" ]; then
                    mv "$base/$name" "$q/gleworks/$name" || recovered=no
                fi
            done
            if [ -d "$base" ]; then mv "$base" "$q/failed-gleworks" || recovered=no;fi
            mv "$q/gleworks" "$base" || recovered=no
        fi
        if [ -d "$q/koreader" ]; then
            if [ -d /mnt/us/koreader ]; then mv /mnt/us/koreader "$q/failed-koreader" || recovered=no;fi
            mv "$q/koreader" /mnt/us/koreader || recovered=no
        fi
        for script in "$q"/documents/*.sh; do [ ! -f "$script" ] || cp "$script" /mnt/us/documents/ || recovered=no;done
        rm -f "$stage/READY"
        if [ "$recovered" = yes ]; then
            touch "$q/restored"
            echo 'Update failed; original programs restored. Restage a complete package before retrying.'
        else echo "Recovery incomplete. Keep all files and inspect $q before using the appliance.";fi
        sync
    fi
    rmdir "$runtime/update.lock"
    exit "$result"
}
trap finish EXIT
personal_reader /mnt/us/koreader "$stage/koreader"
# Retain every prior program file; data/files move with the active appliance.
mv /mnt/us/koreader "$q/koreader"
mv "$base" "$q/gleworks"
mv "$stage/koreader" /mnt/us/koreader
mv "$stage/gleworks" "$base"
for name in data files; do
    [ ! -e "$base/$name" ] || die 'Update package occupied personal data path.'
    if [ -d "$q/gleworks/$name" ]; then mv "$q/gleworks/$name" "$base/$name";fi
done
sh "$base/scripts/kindle-services.sh" preflight
for script in "$base"/scriptlets/gleworks-*.sh; do cp "$script" /mnt/us/documents/;done
cp "$base/scripts/rollback.sh" /mnt/us/documents/gleworks-rollback.sh
cp "$base/scripts/gleworks-inspect.sh" /mnt/us/documents/gleworks-inspect.sh
cp "$stage/gleworks-install.sh" /mnt/us/documents/gleworks-install.sh
# Use the newly active validator on the original manifest to record managed hashes.
# The tree moved, so copy the already verified manifest instead of validating it again.
cp "$stage/SHA256SUMS" "$base/installed-update.sha256"
cp "$runtime/update-release.json" "$base/appliance-release.json"
rm "$stage/READY"
touch "$q/completed"
activated=yes
sync
echo 'All managed apps updated. Reader/terminal, planner, VPN and Transfer are one revision.'
echo 'Personal calendar, todos, books, preferences and private credentials were retained.'
echo "Previous version: $q"
