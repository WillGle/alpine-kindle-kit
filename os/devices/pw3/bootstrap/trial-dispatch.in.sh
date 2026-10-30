#!/bin/sh
# Sync the consumed request before experimental code; this is not power-loss proof.
trial=/gleworks-native-trial
made_proc=
if [ "$$" -eq 1 ] && [ -f "$trial/try.once" ]; then
    if [ ! -d /proc/1 ]; then
        /bin/mount -t proc proc /proc && made_proc=1
    fi
    if [ -d /proc/1 ] && /bin/mount -o remount,rw /; then
        if /bin/mv "$trial/try.once" "$trial/try.used" && /bin/sync &&
           [ ! -e "$trial/try.once" ] && [ -f "$trial/try.used" ]; then
            if /bin/mount -o remount,ro /; then
                if [ -n "$made_proc" ]; then /bin/umount /proc; made_proc=; fi
                if [ -x "$trial/bootstrap" ]; then
                    echo GW_P1_TRIAL_DISPATCH
                    if [ "$(/bin/cat "$trial/try.used")" = gleworks-usb-rescue-repair-v1 ]; then
                        exec "$trial/bootstrap" --rescue-trial
                    fi
                    exec "$trial/bootstrap" --trial
                fi
            fi
        fi
        /bin/mount -o remount,ro /
    fi
    if [ -n "$made_proc" ]; then /bin/umount /proc; fi
fi
echo GW_P1_STOCK_FALLBACK
exec /sbin/init.gleworks-stock "$@"
