#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Worker is copied to tmpfs before execution. Session expires after one hour.
set -eu
umask 077
work=$1
out=$2
host=$3
port=2222
server=
input_rule=0
output_rule=0
input_chain=0
chain=GWALP$$
stop_owned_sessions() {
    # Freeze only processes descended from this private executable while taking
    # a process-tree snapshot. Start times guard against PID reuse on cleanup.
    owned=" "
    : > "$work/owned-processes"
    for statfile in /proc/[0-9]*/stat; do
        pid=${statfile#/proc/}; pid=${pid%/stat}
        [ "$(readlink "/proc/$pid/exe" 2>/dev/null || true)" = "$work/dropbear" ] || continue
        kill -STOP "$pid" 2>/dev/null || continue
        owned="$owned$pid "
    done
    changed=1
    while [ "$changed" = 1 ]; do
        changed=0
        for statfile in /proc/[0-9]*/stat; do
            statline=$(cat "$statfile" 2>/dev/null) || continue
            pid=${statfile#/proc/}; pid=${pid%/stat}
            rest=${statline##*) }
            set -f
            set -- $rest
            set +f
            parent=$2
            case "$owned" in *" $pid "*) continue;; esac
            case "$owned" in *" $parent "*)
                kill -STOP "$pid" 2>/dev/null || continue
                owned="$owned$pid "
                changed=1
            ;; esac
        done
    done
    for pid in $owned; do
        statline=$(cat "/proc/$pid/stat" 2>/dev/null) || continue
        rest=${statline##*) }
        set -f
        set -- $rest
        set +f
        shift 19
        printf '%s %s\n' "$pid" "$1" >> "$work/owned-processes"
    done
    while read -r pid started; do
        statline=$(cat "/proc/$pid/stat" 2>/dev/null) || continue
        rest=${statline##*) }
        set -f
        set -- $rest
        set +f
        shift 19
        [ "$1" = "$started" ] || continue
        kill -TERM "$pid" 2>/dev/null || true
        kill -CONT "$pid" 2>/dev/null || true
    done < "$work/owned-processes"
    sleep 2
    while read -r pid started; do
        statline=$(cat "/proc/$pid/stat" 2>/dev/null) || continue
        rest=${statline##*) }
        set -f
        set -- $rest
        set +f
        shift 19
        [ "$1" = "$started" ] || continue
        kill -KILL "$pid" 2>/dev/null || true
    done < "$work/owned-processes"
}
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    if [ -n "$server" ]; then
        stop_owned_sessions
        wait "$server" 2>/dev/null || true
    fi
    if [ "$input_rule" = 1 ]; then
        iptables -D INPUT -i wlan0 -d "$address/32" -p tcp --dport "$port" -j "$chain" || true
    fi
    if [ "$input_chain" = 1 ]; then
        iptables -F "$chain" || true
        iptables -X "$chain" || true
    fi
    if [ "$output_rule" = 1 ]; then
        iptables -D OUTPUT -o wlan0 -d "$host/32" -s "$address/32" -p tcp --sport "$port" -j ACCEPT || true
    fi
    # Do not reopen /mnt/us after handoff; USB may own it by now.
    rm -f "$work/server-key" "$work/authorized_keys"
    : > "$work/failed.txt"
    rmdir /tmp/gleworks-alpine-session.lock || true
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
cd "$work"
if [ "${GLEWORKS_CONTROL_ONLY:-0}" != 1 ]; then
uname -srvm > "$out/kernel.txt"
readlink /proc/1/exe > "$out/pid1-exe.txt"
cat /proc/1/status > "$out/pid1-status.txt"
cat /proc/1/mountinfo > "$out/pid1-mountinfo.txt"
cat /proc/sys/kernel/random/boot_id > "$out/boot-id.txt"
initctl version > "$out/upstart-version.txt" 2>&1 || true
for source in /sbin/init /sbin/init.exe /sbin/telinit /etc/upstart/usbnet-autostart.conf /etc/upstart/sshd.conf /etc/upstart/usbnetd.conf /lib/ld-linux.so.3 /lib/libc.so.6 /lib/libutil.so.1; do
    [ -f "$source" ] || continue
    cp -L "$source" "$out/$(echo "$source" | tr / _)"
done
# Only an ABI diagnostic: Upstart remains PID1; this is explicitly not boot proof.
root=$work/alpine
mkdir -p "$root/bin" "$root/lib" "$root/etc"
cp busybox "$root/bin/"
cp ld-musl-armhf.so.1 "$root/lib/"
cp alpine-release "$root/etc/"
chmod 755 "$root/bin/busybox" "$root/lib/ld-musl-armhf.so.1"
ln -s ld-musl-armhf.so.1 "$root/lib/libc.musl-armv7.so.1"
if chroot "$root" /bin/busybox sh -c '
    test "$(/bin/busybox cat /etc/alpine-release)" = 3.24.3 || exit 1
    /bin/busybox uname -r
    /bin/busybox printf "ALPINE_ABI_ONLY release=3.24.3 pid=%s\n" "$$"
    /bin/busybox true
' > "$out/alpine-abi.txt" 2>&1; then
    echo 'PASS: Alpine BusyBox/musl executes on stock kernel. Not independent boot.' > "$out/alpine-abi-result.txt"
else
    echo 'FAIL: Alpine userspace ABI diagnostic; see alpine-abi.txt.' > "$out/alpine-abi-result.txt"
fi
if [ -f "$work/gleworks-alpine-stage.sh" ]; then
    if sh "$work/gleworks-alpine-stage.sh" > "$out/stage-launch.txt" 2>&1; then
        echo 'Returning stage finished; inspect its separate capture.' >> "$out/stage-launch.txt"
    else
        echo 'Returning stage declined/failed; no automatic retry.' >> "$out/stage-launch.txt"
    fi
fi
fi
./dropbear -h > "$out/dropbear-help.txt" 2>&1 || true
grep -q 'Disable password logins' "$out/dropbear-help.txt"
grep -q 'Directory containing authorized_keys' "$out/dropbear-help.txt"
# Use already saved Wi-Fi credentials; no registration or credential capture.
lipc-set-prop com.lab126.cmd wirelessEnable 1 || true
lipc-set-prop com.lab126.wifid enable 1 || true
address=
attempt=0
while [ "$attempt" -lt 30 ]; do
    address=$(ifconfig wlan0 | sed -n 's/.*inet addr:\([0-9.]*\).*/\1/p')
    [ -z "$address" ] || break
    attempt=$((attempt+1))
    sleep 1
done
case "$address" in ''|*[!0-9.]*) echo 'No Wi-Fi address; SSH not started.' > "$out/CONTROL.txt"; exit 1;; esac
[ "${address%.*}" = "${host%.*}" ] || { echo 'Wi-Fi is on another subnet; SSH not started.' > "$out/CONTROL.txt"; exit 1; }
mkdir keys
cp authorized_keys keys/
chmod 700 keys
chmod 600 keys/authorized_keys
# Keep the existing firewall policy; allow this host only, in RAM, until cleanup.
iptables -N "$chain"
input_chain=1
iptables -A "$chain" -s "$host/32" -j ACCEPT
iptables -A "$chain" -j DROP
iptables -I INPUT 1 -i wlan0 -d "$address/32" -p tcp --dport "$port" -j "$chain"
input_rule=1
iptables -I OUTPUT 1 -o wlan0 -d "$host/32" -s "$address/32" -p tcp --sport "$port" -j ACCEPT
output_rule=1
./dropbear -F -E -s -j -k -m -T 3 -K 30 -I 900 -M 3600 -r "$work/server-key" -D "$work/keys" -p "$address:$port" -P "$work/server.pid" > "$work/ssh.log" 2>&1 &
server=$!
sleep 2
kill -0 "$server"
printf 'SSH_READY address=%s port=%s lifetime_seconds=3600 auth=publickey_only\n' "$address" "$port" > "$out/CONTROL.txt"
echo 'NO_NATIVE_BOOT: Upstart is still PID1; no bootloader/kernel/rootfs write.' >> "$out/CONTROL.txt"
cp "$work/ssh.log" "$out/ssh-start.txt"
(cd "$out" && for file in ./*; do [ "$file" = ./SHA256SUMS ] || sha256sum "$file"; done > SHA256SUMS)
sync
printf 'SSH %s:%s (temporary, host key pinned)\n' "$address" "$port" > "$work/ready.txt"
# Reading the host's existing SSH banner advertises our source address without
# opening a new host firewall port or authenticating to/changing the host.
if curl --version 2>/dev/null | grep -qi telnet; then
    curl --silent --connect-timeout 3 --max-time 12 "telnet://$host:2222" < /dev/null > "$work/host-banner.txt" 2>&1 || true
fi
sleep 3600
