#!/bin/sh
# Name: GleWorks System Inspection
# Author: GleWorks
# Reads system metadata, saves a report and displays completion; no service/settings changes.
set -u

if [ "$(id -u)" != 0 ]; then
    if [ ! -x /var/local/kmc/bin/su ]; then
        echo 'Root access is required; the existing KMC root helper was not found.' >&2
        exit 1
    fi
    # Fixed path avoids interpreting a caller-controlled script name as shell code.
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-inspect.sh'
fi

umask 077
report=/mnt/us/gleworks-inspection-$(date +%Y%m%d-%H%M%S)-$$.txt
# Refuse to overwrite an existing path, including a symlink.
set -C
exec >"$report" 2>&1
finish() {
    result=$?
    trap - EXIT
    set +e
    if [ "$result" != 0 ] && [ -f /mnt/us/gleworks/scripts/record-failure.sh ]; then
        /bin/sh /mnt/us/gleworks/scripts/record-failure.sh inspection "$result" launcher || :
    fi
    message="Inspection finished. Report: $report"
    [ "$result" = 0 ] || message="Inspection stopped. Review: $report"
    if [ -f /mnt/us/gleworks/scripts/show-message.sh ]; then
        printf '%s\n' "$message" | /bin/sh /mnt/us/gleworks/scripts/show-message.sh 30 home inspection "$result"
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
trap 'exit 1' HUP INT TERM

section() { printf '\n--- %s ---\n' "$1"; }
metadata() {
    for item do
        if [ -e "$item" ] || [ -L "$item" ]; then
            ls -ld "$item"
            if [ -f "$item" ] && [ ! -L "$item" ] && command -v sha256sum >/dev/null 2>&1; then
                sha256sum "$item"
            fi
        fi
    done
}

echo 'GleWorks read-only system inspection'
echo 'Saves this report and temporary display diagnostics. No credentials or process arguments are collected.'
date -u

section 'Kernel and CPU (serial omitted)'
uname -srvm
grep -E '^(Processor|model name|CPU architecture|CPU implementer|CPU part|Features|Hardware)[[:space:]]*:' /proc/cpuinfo
grep -E '^(MemTotal|MemAvailable|MemFree):' /proc/meminfo

section 'Filesystem layout'
awk '$2 == "/" || $2 == "/mnt/us" || $2 == "/var/local" { print $2, $3, $4 }' /proc/mounts
df -k / /mnt/us /var/local

section 'Process names and executable paths (no command arguments)'
for proc in /proc/[0-9]*; do
    [ -r "$proc/comm" ] || continue
    name=$(cat "$proc/comm" 2>/dev/null) || continue
    exe=$(readlink "$proc/exe" 2>/dev/null) || exe='unavailable'
    printf '%s %s %s\n' "${proc##*/}" "$name" "$exe"
done

section 'Local listening TCP sockets (hex address:port, state, UID)'
for table in /proc/net/tcp /proc/net/tcp6; do
    [ -r "$table" ] || continue
    echo "$table"
    awk 'NR > 1 && $4 == "0A" { print $2, $4, $8 }' "$table"
done

section 'Upstart job state'
if command -v initctl >/dev/null 2>&1; then initctl list; fi

section 'OTA executable names and hashes (nothing executed)'
metadata /usr/bin/*ota* /usr/sbin/*ota* /sbin/*ota*

section 'Startup file names and hashes'
for dir in /etc/upstart /etc/init /etc/init.d /etc/rcS.d /etc/rc.d; do
    [ -d "$dir" ] || continue
    find "$dir" -maxdepth 2 -type f -exec ls -ld '{}' \;
    if command -v sha256sum >/dev/null 2>&1; then
        find "$dir" -maxdepth 2 -type f -exec sha256sum '{}' \;
    fi
done

section 'Startup files referencing installed jailbreak or remote services (names only)'
for dir in /etc/upstart /etc/init /etc/init.d; do
    [ -d "$dir" ] || continue
    find "$dir" -maxdepth 2 -type f -exec grep -l -E 'unlocked|ku-http|ku-tailscale|tailscaled|trinh\.uk|winterbreak|jb\.sh' '{}' \;
done

section 'Known jailbreak/root helper metadata'
metadata /var/local/kmc/bin/su /var/local/kmc/bin/kpm /var/local/kmc/sbin/kpm.sh \
    /usr/bin/scriptExecutor /etc/scriptExecutorConfig.json /etc/lipc-daemon-props.conf
for dir in /var/local/kmc /var/local/system /mnt/us/usbnet /etc/dropbear; do
    [ -d "$dir" ] && ls -ld "$dir"
done

section 'SSH configuration choices (no keys or credentials)'
for config in /etc/ssh/sshd_config /etc/sshd_config; do
    [ -f "$config" ] || continue
    echo "$config"
    grep -E '^[[:space:]]*(Port|ListenAddress|PermitRootLogin|PasswordAuthentication|PubkeyAuthentication|AuthorizedKeysFile)[[:space:]]' "$config"
done
for keyfile in /root/.ssh/authorized_keys /var/root/.ssh/authorized_keys /etc/dropbear/authorized_keys /mnt/us/usbnet/etc/authorized_keys; do
    if [ -f "$keyfile" ]; then
        ls -ld "$keyfile"
        printf 'Line count: '
        wc -l <"$keyfile"
    fi
done

section 'GleWorks persistent failure journal'
echo 'Directory: /mnt/us/gleworks-logs; bounded rotation, UTC timestamps'
for journal in /mnt/us/gleworks-logs/errors.log.1 /mnt/us/gleworks-logs/errors.log; do
    if [ -f "$journal" ] && [ ! -L "$journal" ] && [ "$(stat -c %s "$journal")" -le 263168 ]; then
        awk 'NF==4 && $1 ~ /^[0-9T:Z-]+$/ && $2 ~ /^action=[A-Za-z0-9_.-]+$/ && $3 ~ /^exit=[0-9]+$/ && $4 ~ /^category=[A-Za-z0-9_.-]+$/ {print}' "$journal"
    fi
done | tail -n 100

section 'Done'
echo 'No service stop/start, firewall change, package removal, or firmware operation was performed.'
echo 'A compromised running OS can falsify these results; this is inspection, not proof of a clean baseline.'
