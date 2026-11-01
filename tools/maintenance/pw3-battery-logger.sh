#!/bin/sh
# PW3 battery-only logger. Run this before unplugging USB; it writes only to
# tmpfs so the measurement does not add flash writes. After reconnecting USB,
# copy the finished TSV and metadata to the host before rebooting.
#
# Usage: BATTERY_TEST=A BATTERY_NOTE="home idle wifi off" \
#        pw3-battery-logger.sh [interval_seconds] [duration_seconds]
# Default: 60 s interval, 24 h duration, at most 1440 rows.
#
# Units from the stock Lab126 max77696 driver, not generic power_supply units:
#   voltage_now  mV
#   current_now  mA
#   charge_now   mAh
#   charge_full  mAh
#   temp         degrees Fahrenheit
set -eu
bb=/bin/busybox
interval=${1:-60}
duration=${2:-86400}
out=/tmp/pw3-battery-log.tsv
meta=/tmp/pw3-battery-meta.txt
events=/tmp/pw3-battery-events.log
lock=/run/gw-battery-logger.pid

case "$interval" in ''|*[!0-9]*) exit 2;; esac
case "$duration" in ''|*[!0-9]*) exit 2;; esac
[ "$interval" -ge 10 ] || { echo "interval must be >= 10 s" >&2; exit 2; }

test_name=${BATTERY_TEST:-unlabeled}
note=${BATTERY_NOTE:-}
# Keep the human metadata one-line and shell/TSV safe.
test_name=$($bb printf '%s' "$test_name" | $bb tr '\t\r\n' '___' | $bb cut -c1-40)
note=$($bb printf '%s' "$note" | $bb tr '\t\r\n' '___' | $bb cut -c1-120)
boot_id=$($bb cat /proc/sys/kernel/random/boot_id)

# One logger only.
if [ -f "$lock" ]; then
    pid=$($bb cat "$lock")
    if [ -n "$pid" ] && [ -r "/proc/$pid/cmdline" ] &&
       $bb tr '\000' ' ' < "/proc/$pid/cmdline" | $bb grep -q 'pw3-battery-logger'; then
        echo "already running: $pid" >&2
        exit 1
    fi
fi
echo $$ > "$lock"
trap 'rm -f "$lock"' EXIT INT TERM

ps_field() { # pid field
    [ -r "/proc/$1/status" ] || return 0
    $bb awk -v k="$2:" '$1==k {print $2}' "/proc/$1/status"
}
batt() {
    d=/sys/class/power_supply/max77696-battery
    for f in status capacity voltage_now current_now charge_now charge_full charge_full_design temp cycle_count; do
        value=$($bb cat "$d/$f" 2>/dev/null || printf '?')
        printf '%s=%s\t' "$f" "$value"
    done
}
power_sig() {
    connected=$($bb awk '/max77696_charger_get_connected/{print $2}' /proc/asession 2>/dev/null || echo ?)
    host=$($bb awk '/max77696_uic_is_usbhost/{print $2}' /proc/asession 2>/dev/null || echo ?)
    charger=$($bb cat /sys/class/power_supply/max77696-charger/online 2>/dev/null || echo ?)
    uic=$($bb cat /sys/class/power_supply/max77696-uic/present 2>/dev/null || echo ?)
    printf 'usb_connected=%s usb_host=%s charger_online=%s uic_present=%s' \
        "${connected:-?}" "${host:-?}" "${charger:-?}" "${uic:-?}"
}
power_cols() {
    connected=$($bb awk '/max77696_charger_get_connected/{print $2}' /proc/asession 2>/dev/null || echo ?)
    host=$($bb awk '/max77696_uic_is_usbhost/{print $2}' /proc/asession 2>/dev/null || echo ?)
    charger=$($bb cat /sys/class/power_supply/max77696-charger/online 2>/dev/null || echo ?)
    uic=$($bb cat /sys/class/power_supply/max77696-uic/present 2>/dev/null || echo ?)
    printf 'usb_connected=%s\tusb_host=%s\tcharger_online=%s\tuic_present=%s' \
        "${connected:-?}" "${host:-?}" "${charger:-?}" "${uic:-?}"
}
cpu_line() { $bb head -1 /proc/stat; }
net_state() {
    st=disconnected
    $bb grep -q 'wlan0' /proc/net/dev && st=present
    [ -r /sys/class/net/wlan0/operstate ] && st=$($bb cat /sys/class/net/wlan0/operstate)
    dhcp=$($bb cat /run/gw-udhcpc.pid 2>/dev/null || echo 0)
    case "$dhcp" in ''|*[!0-9]*) dhcp=0;; esac
    [ -r "/proc/$dhcp/cmdline" ] || dhcp=0
    printf 'wlan=%s\twpa=%s\tudhcpc=%s\ttailscale=%s' \
        "$st" "$(pidof wpa_supplicant || echo 0)" "$dhcp" \
        "$(pidof tailscaled || echo 0)"
}

{
    echo "boot_id=$boot_id"
    echo "test=$test_name"
    echo "note=$note"
    echo "interval=$interval"
    echo "duration=$duration"
    echo "start_epoch=$(date +%s)"
    echo "start_uptime=$($bb cut -d' ' -f1 /proc/uptime)"
    echo "start_power=$(power_sig)"
} > "$meta"

printf 'epoch\tuptime\tboot_id\ttest\tnote' > "$out"
printf '\tbatt_status\tcapacity\tvoltage_now\tcurrent_now\tcharge_now\tcharge_full\tcharge_full_design\ttemp\tcycle_count' >> "$out"
printf '\tcpu_user\tcpu_nice\tcpu_sys\tcpu_idle\tcpu_iowait\tcpu_irq\tcpu_softirq' >> "$out"
printf '\tctxt\tintr\tload1' >> "$out"
printf '\twlan\twpa_pid\tudhcpc_pid\ttailscale_pid' >> "$out"
printf '\tfrontlight\tgovernor\treader_rss_kb\treader_hwm_kb' >> "$out"
printf '\tusb_connected\tusb_host\tcharger_online\tuic_present\tpower_change' >> "$out"
printf '\n' >> "$out"

start=$($bb cut -d' ' -f1 /proc/uptime | $bb cut -d. -f1)
rows=0
prev_power=$(power_sig)
saw_disconnected=0
end_reason=completed
while :; do
    now=$($bb cut -d' ' -f1 /proc/uptime | $bb cut -d. -f1)
    elapsed=$((now - start))
    if [ "$elapsed" -gt "$duration" ]; then end_reason=duration; break; fi
    [ "$rows" -ge 1440 ] && { end_reason=max_rows; break; }

    current_power=$(power_sig)
    if [ "$current_power" = "$prev_power" ]; then
        changed=0
    else
        changed=1
        printf '%s changed: %s -> %s\n' "$(date +%s)" "$prev_power" "$current_power" >> "$events"
    fi
    prev_power=$current_power

    # Conservative stop criteria. Thresholds are in the driver's native units.
    cap=$($bb cat /sys/class/power_supply/max77696-battery/capacity 2>/dev/null || echo 0)
    temp_f=$($bb cat /sys/class/power_supply/max77696-battery/temp 2>/dev/null || echo 0)
    if [ "$cap" -le 30 ]; then
        printf '%s STOP capacity=%s <= 30%%\n' "$(date +%s)" "$cap" >> "$events"
        end_reason=low_capacity; break
    fi
    if [ "$temp_f" -ge 104 ]; then
        printf '%s STOP temp_f=%s >= 104F\n' "$(date +%s)" "$temp_f" >> "$events"
        end_reason=high_temperature; break
    fi

    # USB reconnect after a battery-only interval ends the session cleanly.
    connected=$($bb awk '/max77696_charger_get_connected/{print $2}' /proc/asession 2>/dev/null || echo ?)
    [ "$connected" = 0 ] && saw_disconnected=1
    if [ "$saw_disconnected" = 1 ] && [ "$connected" = 1 ]; then
        printf '%s STOP usb_reconnected\n' "$(date +%s)" >> "$events"
        end_reason=usb_reconnected; break
    fi

    printf '%s\t%s\t%s\t%s\t%s\t' \
        "$(date +%s)" "$($bb cut -d' ' -f1 /proc/uptime)" \
        "$boot_id" "$test_name" "$note" >> "$out"
    batt >> "$out"
    set -- $(cpu_line)
    printf 'cpu_user=%s\tcpu_nice=%s\tcpu_sys=%s\tcpu_idle=%s\tcpu_iowait=%s\tcpu_irq=%s\tcpu_softirq=%s\t' \
        "$2" "$3" "$4" "$5" "$6" "$7" "$8" >> "$out"
    printf 'ctxt=%s\tintr=%s\tload1=%s\t' \
        "$($bb awk '/^ctxt /{print $2}' /proc/stat)" \
        "$($bb awk '/^intr /{print $2}' /proc/stat)" \
        "$(cat /proc/loadavg | $bb cut -d' ' -f1)" >> "$out"
    net_state >> "$out"
    printf '\t%s\t%s\t' \
        "$(cat /sys/class/backlight/max77696-bl/brightness 2>/dev/null || echo '?')" \
        "$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo '?')" >> "$out"
    rp=$(pidof luajit || true)
    if [ -n "$rp" ]; then
        printf '%s\t%s\t' "$(ps_field "$rp" VmRSS)" "$(ps_field "$rp" VmHWM)" >> "$out"
    else
        printf '0\t0\t' >> "$out"
    fi
    power_cols >> "$out"
    printf '\t%s\n' "$changed" >> "$out"
    rows=$((rows + 1))
    # tmpfs only. Do not issue a global sync: it could flush unrelated root
    # filesystem writes and defeat the no-unnecessary-flash-write goal.
    sleep "$interval"
done
{
    echo "end_epoch=$(date +%s)"
    echo "end_uptime=$($bb cut -d' ' -f1 /proc/uptime)"
    echo "rows=$rows"
    echo "end_reason=$end_reason"
    echo "end_power=$(power_sig)"
} >> "$meta"
echo "complete: $out rows=$rows" >&2
