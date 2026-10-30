#!/bin/sh
# Name: GleWorks Rollback
# Author: GleWorks
# Restores quarantined apps but never starts their network services.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-rollback.sh'
fi
locked=no
finish() {
    result=$?
    trap - EXIT
    set +e
    [ "$locked" != yes ] || rmdir /tmp/gleworks-migration.lock
    if [ "$result" = 0 ]; then message='Rollback completed. Original apps restored; network services stopped.'
    else message='Rollback stopped for review. Original files remain in the recorded quarantine.'; fi
    helper=/mnt/us/gleworks/scripts/show-message.sh
    if [ ! -f "$helper" ] && [ "${validated_q:-no}" = yes ]; then helper=$q/new-gleworks/scripts/show-message.sh;fi
    recorder=${helper%/*}/record-failure.sh
    if [ "$result" != 0 ] && [ -f "$recorder" ]; then /bin/sh "$recorder" rollback "$result" launcher || :;fi
    if [ -f "$helper" ]; then
        printf '%s\n' "$message" | /bin/sh "$helper" 30 home rollback "$result"
    else
        printf '%s\n' "$message" | awk '
            BEGIN { print "+----------------------------------------------+"; print "| GleWorks                                     |"; print "+----------------------------------------------+"; n=0 }
            { while (length($0)>44 && n<14) { printf "| %-44s |\n",substr($0,1,44); $0=substr($0,45);n++ }
              if(n<14) { printf "| %-44s |\n",$0;n++ } }
            END { while(n++<14) print "|                                              |";print "+----------------------------------------------+" }
        ' | { row=6;while IFS= read -r line;do eips 2 "$row" "$line" || :;row=$((row+1));done; }
        sleep 30
        /usr/bin/lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home 2>/dev/null || :
        /usr/bin/xrefresh -d :0.0 2>/dev/null || :
    fi
    exit "$result"
}
trap finish EXIT
q=${1:-$(cat /mnt/us/gleworks-last-migration.txt)}
case $q in /mnt/us/gleworks-quarantine-*) ;; *) echo 'Invalid quarantine path' >&2; exit 1 ;; esac
suffix=${q#/mnt/us/gleworks-quarantine-}
case $suffix in ''|*[!0-9-]*) echo 'Invalid quarantine name' >&2; exit 1 ;; esac
[ -d "$q" ] && [ ! -L "$q" ] || exit 1
validated_q=yes
[ ! -e "$q/rolled-back" ] || { echo 'Already rolled back'; exit 1; }
mkdir /tmp/gleworks-migration.lock 2>/dev/null || { echo 'Migration lock exists'; exit 1; }
locked=yes
if [ -f /mnt/us/gleworks/scripts/kindle-services.sh ]; then
    sh /mnt/us/gleworks/scripts/kindle-services.sh http-stop
    sh /mnt/us/gleworks/scripts/kindle-services.sh vpn-stop
fi
# Both readers must be closed before moving their files.
for proc in /proc/[0-9]*/cmdline; do
    [ -r "$proc" ] || continue
    if tr '\000' '\n' <"$proc" 2>/dev/null | grep -E '(^|/)(reader.lua|kpm)$' >/dev/null; then
        echo 'Close KOReader and the package manager first.' >&2; exit 1
    fi
done
# Check all destinations first; leave new private credentials for deliberate cleanup.
for name in ku-http ku-tailscale koreader; do
    if [ -d "$q/packages/$name" ] && [ -e "/mnt/us/kmc/kpm/packages/$name" ]; then
        echo "Restore destination occupied: $name" >&2; exit 1
    fi
done
if [ -d "$q/unlocked" ] && [ -e /mnt/us/unlocked ]; then echo 'Old state destination occupied'; exit 1; fi
# Check scriptlet destinations before moving files or modifying records.
for script in "$q"/documents/*.sh; do
    [ -f "$script" ] || continue
    [ ! -e "/mnt/us/documents/${script##*/}" ] || { echo 'Original scriptlet destination occupied'; exit 1; }
done
[ ! -e "$q/new-koreader" ] && [ ! -e "$q/new-gleworks" ] || { echo 'Previous rollback was interrupted; review retained files first.'; exit 1; }
if [ -f "$q/db-removal-started" ]; then
    export GLE_QUARANTINE="$q"
    (cd /mnt/us/koreader && ./luajit -e 'require("setupkoenv")' /mnt/us/gleworks/device/package-records.lua --restore)
fi
if [ -d "$q/koreader" ]; then
    [ ! -e "$q/new-koreader" ] || exit 1
    [ ! -d /mnt/us/koreader ] || mv /mnt/us/koreader "$q/new-koreader"
    mv "$q/koreader" /mnt/us/koreader
fi
for name in ku-http ku-tailscale koreader; do
    [ ! -d "$q/packages/$name" ] || mv "$q/packages/$name" /mnt/us/kmc/kpm/packages/
done
[ ! -d "$q/unlocked" ] || mv "$q/unlocked" /mnt/us/unlocked
for script in "$q"/documents/*.sh; do
    [ -f "$script" ] || continue
    [ ! -e "/mnt/us/documents/${script##*/}" ] || { echo 'Original scriptlet destination occupied'; exit 1; }
    mv "$script" /mnt/us/documents/
done
for script in /mnt/us/documents/gleworks-*.sh; do
    [ -f "$script" ] || continue
    # Retain the executing rollback script; move other installed launchers away.
    [ "${script##*/}" = gleworks-rollback.sh ] && continue
    mkdir -p "$q/new-scriptlets"
    mv "$script" "$q/new-scriptlets/"
done
[ ! -d /mnt/us/gleworks ] || mv /mnt/us/gleworks "$q/new-gleworks"
touch "$q/rolled-back"
sync
echo 'Original app files and package records restored; no original network service started.'
echo 'New private credentials remain in /var/local/gleworks; review before removing.'
