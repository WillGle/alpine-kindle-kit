#!/usr/bin/env python3
"""Read-only PW3 Alpine resource audit (Phase 0).

Collects the measurements used by docs/evidence/PW3-PERFORMANCE-BASELINE.md
over the physical USB CDC-ACM console.  It never writes, remounts, restarts or
reconfigures the device.

Usage:
    python3 tools/maintenance/pw3-audit.py
    python3 tools/maintenance/pw3-audit.py --sample-seconds 300
    python3 tools/maintenance/pw3-audit.py --json out.json --raw out.txt

Only Linux 3.0.35 interfaces are used: /proc, sysfs, /proc/diskstats,
/proc/interrupts and /proc/vmstat.  PSI, zram/zswap, eBPF and cpuidle
residency counters are not present on this kernel and are not assumed.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS / "lib"))
from usb_console import Console  # noqa: E402

HZ_FALLBACK = 100
WATCHED = ("gw-system", "gw-shell", "wpa_supplicant", "tailscaled", "dropbear")


def connect(seconds: int = 30) -> Console:
    last = None
    for _ in range(8):
        try:
            return Console(seconds=seconds)
        except Exception as exc:  # TimeoutError / OSError
            last = exc
            time.sleep(3)
    raise TimeoutError(f"USB console unavailable: {last}")


def run(console: Console, script: str, seconds: int = 60) -> str:
    last = None
    for _ in range(3):
        try:
            return console.run(script, seconds=seconds)
        except (TimeoutError, EOFError) as exc:
            last = exc
            time.sleep(2)
    raise last  # type: ignore[misc]


# ── Static probes ────────────────────────────────────────────────────────────

STATIC_SCRIPT = r'''set -u
echo "##IDENT"
cat /etc/alpine-release
uname -a
cat /proc/sys/kernel/random/boot_id
cat /proc/uptime
cat /proc/cmdline
readlink /proc/1/exe
cat /etc/inittab
echo "##CPUFREQ"
echo -n "clk_tck="; getconf CLK_TCK 2>/dev/null || echo 100
for f in scaling_available_governors scaling_governor scaling_available_frequencies \
         scaling_cur_freq cpuinfo_min_freq cpuinfo_max_freq; do
  echo -n "$f="; cat /sys/devices/system/cpu/cpu0/cpufreq/$f 2>/dev/null || echo MISSING
done
echo -n "cpuidle="; ls /sys/devices/system/cpu/cpu0/cpuidle 2>/dev/null || echo MISSING
echo -n "power_state="; cat /sys/power/state 2>/dev/null || echo MISSING
echo -n "autosleep="; cat /sys/power/autosleep 2>/dev/null || echo MISSING
echo -n "wakeup_count="; cat /sys/power/wakeup_count 2>/dev/null || echo MISSING
echo -n "thermal="; ls /sys/class/thermal 2>/dev/null || echo MISSING
echo -n "swap="; cat /proc/swaps
echo "##MOUNTS"
cat /proc/mounts
echo "##MODULES"
cat /proc/modules
echo "##VMSET"
for f in dirty_writeback_centisecs dirty_expire_centisecs dirty_ratio \
         dirty_background_ratio swappiness; do
  echo -n "$f="; cat /proc/sys/vm/$f
done
echo "##MEMINFO"
cat /proc/meminfo
echo "##BUDDY"
cat /proc/buddyinfo
echo "##ZONE"; head -60 /proc/zoneinfo
echo "##OPENRC"
rc-status -a 2>&1
echo "##RUNLEVELS"
ls -l /etc/runlevels/default /etc/runlevels/boot /etc/runlevels/sysinit 2>/dev/null
echo "##FILESYSTEM"
df -k
df -i
echo "##APK"
echo -n "package_count="; grep -c '^P:' /lib/apk/db/installed
echo -n "db_bytes="; ls -l /lib/apk/db/installed | awk '{print $5}'
awk -F: '/^P:/{p=substr($0,3)} /^V:/{v=substr($0,3)} /^S:/{s=substr($0,3); print s"\t"p"-"v}' \
    /lib/apk/db/installed | sort -rn | head -30
echo "##LOGS"
ls -ln /run/gw-system.log /run/tailscaled.log /.ash_history /var/log/apk.log 2>/dev/null
cat /run/tailscaled.log 2>/dev/null | head -3
echo "##FLAGS"
head -30 /run/gw-system.log 2>/dev/null | grep -E 'GW_SHELL: starting|EPDC port init|touch node|SPIKE=|GW_SYSTEM: ready' | head -12
echo "##STATUS"
/usr/sbin/gwctl caps 2>&1 | head -3
/usr/sbin/gwctl status 2>&1 | head -3
for f in brightness actual_brightness max_brightness; do
  [ -r /sys/class/backlight/max77696-bl/$f ] && { echo -n "$f="; cat /sys/class/backlight/max77696-bl/$f; }
done
echo "##STARTTIMES"
for pid in 1 12678 12680 12757 12774; do
  [ -r /proc/$pid/stat ] || continue
  line=$(cat /proc/$pid/stat); rest=${line##*") "}; set -- $rest
  echo "pid=$pid comm=$(cat /proc/$pid/comm) start_jiffies=${20}"
done
echo "##DMESG"
dmesg | head -5
dmesg | grep -Ei 'error|fail|warn|oom|kill|reset' | tail -20
'''


STORAGE_SCRIPT = r'''set -u
echo "##DU"
du -xk / 2>/dev/null | sort -rn | head -40
du -xk /usr /opt /etc /var /gleworks /root /home 2>/dev/null | sort -rn | head -30
echo "##BIGFILES"
find / -xdev -type f -size +512k 2>/dev/null | while read f; do ls -l "$f"; done | sort -k5 -rn | head -40
echo "##FONTS"
find /usr/share/fonts /opt/koreader/fonts -type f 2>/dev/null | while read f; do ls -l "$f"; done | sort -k5 -rn | head -20
echo "##KOREADER"
du -sk /opt/koreader 2>/dev/null
du -sk /opt/koreader/* 2>/dev/null | sort -rn | head -15
'''


def snapshot_script(offset: int | None = None) -> str:
    tail = ""
    if offset is not None:
        # Only scan bytes appended since the first snapshot.  The deployed
        # diagnostic log reaches tens of MiB and a full scan would itself
        # consume significant CPU and distort the measurement.
        tail = f'''echo "##COUNTS"
tail -c +{offset + 1} $L | awk '/epdc submit/{{s++}} /epdc complete/{{c++}} /flush copy/{{f++}} \
/render handler/{{r++}} /input event/{{i++}} /status ipc_us/{{p++}} \
END{{print "submits="s+0,"completes="c+0,"flush="f+0,"renders="r+0,"inputs="i+0,"status_ipc="p+0}}'
echo "##LINES"
tail -c +{offset + 1} $L | wc -l
'''
    return r'''set -u
L=/run/gw-system.log
echo "##SNAP"
cat /proc/uptime
head -1 /proc/stat
grep -E '^ctxt |^intr |^processes ' /proc/stat
''' + tail + r'''echo "##LOG"
ls -ln $L
echo "##PROC"
for p in /proc/[0-9]*; do
  pid=${p#/proc/}
  [ -r "$p/stat" ] || continue
  line=$(cat "$p/stat" 2>/dev/null) || continue
  comm=$(cat "$p/comm" 2>/dev/null) || comm=?
  rest=${line##*") "}; set -- $rest
  vol=$(awk '/^voluntary_ctxt_switches:/{print $2}' "$p/status" 2>/dev/null)
  nvol=$(awk '/^nonvoluntary_ctxt_switches:/{print $2}' "$p/status" 2>/dev/null)
  hwm=$(awk '/^VmHWM:/{print $2}' "$p/status" 2>/dev/null)
  rss=$(awk '/^VmRSS:/{print $2}' "$p/status" 2>/dev/null)
  echo "P|$pid|$comm|${12}|${13}|${8}|${10}|${20}|${21}|${22}|$rss|$hwm|$vol|$nvol"
done
echo "##MEM"
grep -E 'MemTotal|MemFree|Buffers|^Cached|Shmem|AnonPages|Slab|Dirty|Writeback' /proc/meminfo
echo "##VM"
grep -E 'pgfault|pgmajfault|pgsteal|pgscan|allocstall|pageoutrun|nr_dirty|nr_writeback|nr_shmem|nr_free_pages' /proc/vmstat
echo "##DISK"
grep -E 'mmcblk0 |mmcblk0p4' /proc/diskstats
echo "##IRQ"
cat /proc/interrupts
echo "##NET"
grep wlan0 /proc/net/dev
echo "##BAT"
for f in capacity status voltage_now current_now charge_now online; do
  [ -r /sys/class/power_supply/max77696-battery/$f ] && { echo -n "$f="; cat /sys/class/power_supply/max77696-battery/$f; }
done
for f in online; do
  [ -r /sys/class/power_supply/max77696-charger/$f ] && { echo -n "charger_$f="; cat /sys/class/power_supply/max77696-charger/$f; }
done
echo "##END"
'''


# ── Parsing helpers ─────────────────────────────────────────────────────────

def one(pattern: str, text: str, cast=int, default=None):
    match = re.search(pattern, text, re.M)
    if not match:
        return default
    try:
        return cast(match.group(1))
    except (ValueError, TypeError):
        return default


def section(text: str, name: str) -> str:
    """Return the last output section, skipping the echoed command text."""
    marker = f"##{name}"
    start = text.rfind(marker)
    if start < 0:
        return ""
    start += len(marker)
    end = text.find("\n##", start)
    return text[start:end if end >= 0 else len(text)]


def parse_snapshot(text: str) -> dict:
    snap = section(text, "SNAP")
    cpu = [int(x) for x in re.search(r"^cpu\s+(.+)$", snap, re.M).group(1).split()]
    proc = []
    for line in section(text, "PROC").splitlines():
        if not line.startswith("P|"):
            continue
        fields = line.split("|")
        if len(fields) < 15:
            continue
        proc.append({
            "pid": int(fields[1]), "comm": fields[2],
            "utime": int(fields[3]), "stime": int(fields[4]),
            "minflt": int(fields[5]), "majflt": int(fields[6]),
            "start": int(fields[7]), "vsize": int(fields[8]), "rss_pages": int(fields[9]),
            "rss_kb": int(fields[10]) if fields[10] else None,
            "hwm_kb": int(fields[11]) if fields[11] else None,
            "voluntary": int(fields[12]) if fields[12] else 0,
            "nonvoluntary": int(fields[13]) if fields[13] else 0,
        })
    counts = dict((k, int(v)) for k, v in re.findall(r"(\w+)=(\d+)", section(text, "COUNTS")))
    log = section(text, "LOG")
    log_bytes = one(r"^\S+\s+\d+\s+\d+\s+\d+\s+(\d+)", log, int, 0) or 0
    lines = section(text, "LINES").strip().splitlines()
    log_lines = (one(r"^\s*(\d+)", lines[-1], int, 0) if lines else 0) or 0
    disk = {}
    for line in section(text, "DISK").splitlines():
        fields = line.split()
        if len(fields) >= 14:
            disk[fields[2]] = {
                "reads": int(fields[3]), "read_sectors": int(fields[5]),
                "writes": int(fields[7]), "write_sectors": int(fields[9]),
                "write_ticks": int(fields[10]),
            }
    irq = {}
    for line in section(text, "IRQ").splitlines():
        match = re.match(r"\s*(\d+):\s+(\d+)\s+\S+\s+(.+)$", line)
        if match:
            irq[match.group(3).strip()] = int(match.group(2))
    mem = dict((k, int(v)) for k, v in re.findall(r"^(\w+):\s+(\d+)", section(text, "MEM"), re.M))
    vm = dict((k, int(v)) for k, v in re.findall(r"^(\w+)\s+(\d+)$", section(text, "VM"), re.M))
    net_match = re.search(r"wlan0:\s*(\d+)\s+(\d+)", section(text, "NET"))
    net = (int(net_match.group(1)), int(net_match.group(2))) if net_match else (0, 0)
    bat = dict((k, v.strip()) for k, v in re.findall(r"^(\w+)=(.*)$", section(text, "BAT"), re.M))
    return {
        "uptime": one(r"^([\d.]+)", snap, float, 0.0),
        "cpu": cpu, "ctxt": one(r"^ctxt\s+(\d+)", snap, int, 0),
        "processes": one(r"^processes\s+(\d+)", snap, int, 0),
        "counts": counts, "log_bytes": log_bytes, "log_lines": log_lines,
        "procs": proc, "disk": disk, "irq": irq, "mem": mem, "vm": vm,
        "net_rx_bytes": net[0], "net_rx_packets": net[1], "battery": bat,
    }


def delta(a: dict, b: dict, hz: int = HZ_FALLBACK) -> dict:
    seconds = b["uptime"] - a["uptime"]
    cpu = [y - x for x, y in zip(a["cpu"], b["cpu"])]
    total = sum(cpu[:8])
    labels = ["user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal"]
    cpu_pct = {label: (100.0 * value / total if total else 0.0) for label, value in zip(labels, cpu)}
    before_pids = {proc["pid"]: proc for proc in a["procs"]}
    procs = {}
    for new in b["procs"]:
        old = before_pids.get(new["pid"])
        if old is None:
            continue
        procs[new["comm"]] = {
            "pid": new["pid"],
            "cpu_pct": 100.0 * ((new["utime"] - old["utime"]) + (new["stime"] - old["stime"])) / (hz * seconds) if seconds else 0.0,
            "minflt_delta": new["minflt"] - old["minflt"],
            "majflt_delta": new["majflt"] - old["majflt"],
            "voluntary_delta": new["voluntary"] - old["voluntary"],
            "nonvoluntary_delta": new["nonvoluntary"] - old["nonvoluntary"],
            "rss_kb": new["rss_kb"], "hwm_kb": new["hwm_kb"],
            "boot_start_s": new["start"] / hz,
        }
    disk = {}
    for name, new in b["disk"].items():
        if name not in a["disk"]:
            continue
        old = a["disk"][name]
        disk[name] = {key: new[key] - old[key] for key in new}
    irq = {k: b["irq"][k] - a["irq"].get(k, b["irq"][k]) for k in b["irq"]}
    count_delta = {k: b["counts"].get(k, 0) - a["counts"].get(k, 0) for k in b["counts"]}
    return {
        "seconds": seconds,
        "cpu_pct": cpu_pct,
        "context_switches_per_s": (b["ctxt"] - a["ctxt"]) / seconds if seconds else 0,
        "interrupts_per_s": sum(irq.values()) / seconds if seconds else 0,
        "irq_per_s": {k: v / seconds for k, v in irq.items()} if seconds else {},
        "processes": procs,
        "counts_delta": count_delta,
        "epdc_updates_per_min": 60.0 * count_delta.get("submits", 0) / seconds if seconds else 0,
        "render_cycles_per_s": count_delta.get("renders", 0) / seconds if seconds else 0,
        "log_bytes_per_s": (b["log_bytes"] - a["log_bytes"]) / seconds if seconds else 0,
        "log_lines_per_s": (b["log_lines"] - a["log_lines"]) / seconds if seconds else 0,
        "flash_write_sectors_delta": disk.get("mmcblk0p4", {}).get("write_sectors", 0),
        "flash_write_bytes_per_s": 512 * disk.get("mmcblk0p4", {}).get("write_sectors", 0) / seconds if seconds else 0,
        "memfree_delta_kb": b["mem"]["MemFree"] - a["mem"]["MemFree"],
        "shmem_delta_kb": b["mem"]["Shmem"] - a["mem"]["Shmem"],
        "pgfault_delta": b["vm"].get("pgfault", 0) - a["vm"].get("pgfault", 0),
        "pgmajfault_delta": b["vm"].get("pgmajfault", 0) - a["vm"].get("pgmajfault", 0),
        "pgsteal_delta": b["vm"].get("pgsteal_normal", 0) + b["vm"].get("pgsteal_dma", 0) - a["vm"].get("pgsteal_normal", 0) - a["vm"].get("pgsteal_dma", 0),
        "net_rx_bytes_delta": b["net_rx_bytes"] - a["net_rx_bytes"],
        "net_rx_packets_delta": b["net_rx_packets"] - a["net_rx_packets"],
        "battery": b["battery"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sample-seconds", type=int, default=90,
                        help="quiet idle interval between snapshots (default 90)")
    parser.add_argument("--full", action="store_true",
                        help="also run slow du/find storage inventory")
    parser.add_argument("--json", type=Path, default=None)
    parser.add_argument("--raw", type=Path, default=None)
    args = parser.parse_args()

    console = connect()
    raw = []
    try:
        static_text = run(console, STATIC_SCRIPT, seconds=180)
        raw.append(("static", static_text))
        storage_text = ""
        if args.full:
            storage_text = run(console, STORAGE_SCRIPT, seconds=240)
            raw.append(("storage", storage_text))
        before_text = run(console, snapshot_script(), seconds=120)
        raw.append(("before", before_text))
        before = parse_snapshot(before_text)
        time.sleep(args.sample_seconds)
        after_text = run(console, snapshot_script(before["log_bytes"]), seconds=120)
        raw.append(("after", after_text))
    finally:
        console.close()

    after = parse_snapshot(after_text)
    hz = one(r"^clk_tck=(\d+)$", section(raw[0][1], "CPUFREQ"), int, HZ_FALLBACK) or HZ_FALLBACK
    result = {
        "collected_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "sample_seconds_requested": args.sample_seconds,
        "sample_seconds_measured": after["uptime"] - before["uptime"],
        "identity": {
            "alpine": one(r"^(\d+\.\d+\.\d+)$", section(raw[0][1], "IDENT"), str),
            "kernel": one(r"^(Linux .+)$", section(raw[0][1], "IDENT"), str),
            "boot_id": one(r"([0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12})", section(raw[0][1], "IDENT"), str),
            "cmdline": one(r"^((?:rootwait|console=).+)$", section(raw[0][1], "IDENT"), str),
        },
        "cpufreq": dict(re.findall(r"^(\w+)=(.*)$", section(raw[0][1], "CPUFREQ"), re.M)),
        "mounts": section(raw[0][1], "MOUNTS").strip().splitlines(),
        "static": {
            "openrc": section(raw[0][1], "OPENRC").strip().splitlines(),
            "filesystem": section(raw[0][1], "FILESYSTEM").strip().splitlines(),
            "apk": section(raw[0][1], "APK").strip().splitlines(),
            "du": section(storage_text, "DU").strip().splitlines(),
            "bigfiles": section(storage_text, "BIGFILES").strip().splitlines(),
            "fonts": section(storage_text, "FONTS").strip().splitlines(),
            "koreader": section(storage_text, "KOREADER").strip().splitlines(),
            "logs": section(raw[0][1], "LOGS").strip().splitlines(),
            "flags": section(raw[0][1], "FLAGS").strip().splitlines(),
            "status": section(raw[0][1], "STATUS").strip().splitlines(),
            "starttimes": section(raw[0][1], "STARTTIMES").strip().splitlines(),
            "dmesg": section(raw[0][1], "DMESG").strip().splitlines(),
        },
        "hz": hz,
        "before": before,
        "after": after,
        "delta": delta(before, after, hz),
    }

    if args.raw:
        args.raw.parent.mkdir(parents=True, exist_ok=True)
        with args.raw.open("w") as stream:
            for name, text in raw:
                stream.write(f"\n######## {name}\n{text}\n")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2) + "\n")

    print(f"sample: {result['sample_seconds_measured']:.1f}s")
    print(f"cpu: " + " ".join(f"{k}={v:.1f}%" for k, v in result["delta"]["cpu_pct"].items()))
    for name in WATCHED:
        info = result["delta"]["processes"].get(name)
        if info:
            print(f"{name}: pid={info['pid']} cpu={info['cpu_pct']:.2f}% "
                  f"rss={info['rss_kb']}kB hwm={info['hwm_kb']}kB "
                  f"ctx/s={info['voluntary_delta'] / result['delta']['seconds']:.1f}")
    print(f"context switches/s: {result['delta']['context_switches_per_s']:.1f}")
    print(f"interrupts/s: {result['delta']['interrupts_per_s']:.1f}")
    for name, rate in sorted(result["delta"]["irq_per_s"].items(), key=lambda item: -item[1])[:5]:
        print(f"  irq {name}: {rate:.2f}/s")
    print(f"epdc updates/min: {result['delta']['epdc_updates_per_min']:.2f}")
    print(f"render cycles/s: {result['delta']['render_cycles_per_s']:.1f}")
    print(f"log growth: {result['delta']['log_bytes_per_s']:.0f} B/s, "
          f"{result['delta']['log_lines_per_s']:.1f} lines/s")
    print(f"flash writes: {result['delta']['flash_write_bytes_per_s']:.0f} B/s")
    print(f"shmem delta: {result['delta']['shmem_delta_kb']} kB; "
          f"pgsteal delta: {result['delta']['pgsteal_delta']}")
    print(f"wlan0 rx: +{result['delta']['net_rx_bytes_delta']} bytes, "
          f"+{result['delta']['net_rx_packets_delta']} packets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
