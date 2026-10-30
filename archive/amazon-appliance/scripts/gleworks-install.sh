#!/bin/sh
# Name: GleWorks Install
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-install.sh'
fi
umask 077
# This log contains installation metadata, never the HTTP token or VPN login.
logdir=/mnt/us/gleworks-logs
[ ! -L "$logdir" ] || exit 1
[ -d "$logdir" ] || mkdir "$logdir"
log=$logdir/install-$(date +%Y%m%d-%H%M%S)-$$.log
set -C
result=0
if /bin/sh /mnt/us/gleworks-stage/gleworks/scripts/install.sh >"$log" 2>&1; then
    message='Install completed. Open GleWorks Reader; then VPN Login and Transfer.'
else result=$?;message="Install stopped. Read $log before retrying.";fi
recorder=/mnt/us/gleworks/scripts/record-failure.sh
[ -f "$recorder" ] || recorder=/mnt/us/gleworks-stage/gleworks/scripts/record-failure.sh
if [ "$result" != 0 ] && [ -f "$recorder" ]; then /bin/sh "$recorder" install "$result" launcher || :;fi
if [ -f /mnt/us/gleworks/scripts/show-message.sh ]; then
    printf '%s\n' "$message" | /bin/sh /mnt/us/gleworks/scripts/show-message.sh 30 home install "$result"
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
