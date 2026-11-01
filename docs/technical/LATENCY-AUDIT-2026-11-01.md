# Reader launch and runtime latency audit — 2026-11-01

User measurement: Home -> KOReader took about 3 seconds from tap to the reader
view.

## Fixed

| Finding | Effect | Fix |
|---|---|---|
| `gw-shell` kept polling its render loop after the reader handoff was accepted | up to 1 s before the shell process exited | break out of the loop immediately after `lv_timer_handler()` |
| `gw-system` only reaped children when its 1 s poll timed out | up to 1 s before the reader was started, and the same delay before Home returned after the reader exited | SIGCHLD self-pipe wakes `service_loop`; 20 ms poll remains only as a fallback while a handoff is pending |
| The four-band page repaint bypassed `g_partial_updates` | the configured full clean every 15 page snaps never ran for Settings/Notes text pages | count one page snap as one budget unit after the four bands finish |

Measured after the supervisor change: a killed `gw-shell` was replaced in about
60 ms instead of up to one second.

## Found, deliberately not changed

| Finding | Why it is left alone |
|---|---|
| `gleworks-early.sh` has a fixed `sleep 4` while binding `g_file_storage` before switching to the USB serial gadget | This is the hardware cable-session handshake that keeps the USB console usable. Replacing it with a poll needs device trials and risks the recovery path. |
| Wi-Fi scan waits a fixed 5 s before reading `wpa_cli scan_results` | It is the radio scan dwell, and the scan runs in a background child; the UI only polls its result. |
| `APP SCAN` followed by `APP USB n` re-scans the USB directory for every entry (up to 7 scans) | Each scan is a small FAT `readdir`/`lstat`; batching would need a stateful protocol change for a few milliseconds. |
| Entering Apps/Update performs one synchronous IPC round trip per row | Each round trip is local and cheap; the screen is already showing its busy state. |
| App install verifies the signature and runs `tar` synchronously inside the single `gw-system` loop | Installs are rare and the UI is visibly busy, but this does block power-button and status service for their duration. A worker child would be the correct larger change. |
| Shell startup performs three `STATUS` queries and builds every screen before the first paint | Only a few milliseconds; consolidating them would make the startup code less clear. |
| KOReader startup performs two `gwctl` subprocess calls (`status`, `caps`) before `reader.lua` | Remaining latency is dominated by KOReader/LuaJIT and opening the last book. Merging both fields into one status line is possible but saves little. |
| Bootstrap and console scripts have fixed 1–2 s sleeps | They are safety and recovery paths, not the interactive launch path. |


## Measured breakdown (device, 2026-11-01)

Probe: monotonic `/proc/uptime` marks in the reader launcher, `native.lua` and
a temporary `late` KOReader patch. Measurements were taken with `gw-system`
stopped, launching KOReader directly on `/mnt/us/documents` (the managed mode
uses the same directory).

| Stage | Baseline | After plugin trim |
|---|---:|---:|
| `exec` -> Lua start | ~10 ms | ~10 ms |
| Lua start -> early setup done | ~470 ms | ~470 ms |
| early setup -> first FileManager repaint submitted | ~2290 ms | ~1750 ms |
| `exec` -> first repaint submitted | **~2.76 s** | **~2.22 s** |

The panel then needs roughly one more update interval (~0.6 s) before the
first image is fully visible. Shell-to-reader handoff is ~60 ms, measured by
killing `gw-shell` and observing its replacement.

The ~0.54 s saving came from disabling plugins that duplicate or cannot work
in managed mode: network/cloud/news/Calibre/KOSync/OPDS/Wallabag, SSH, terminal,
text editor, HTTP inspector, external keyboard, Japanese input, QR clipboard,
and the plugin-based power management (`autodim`, `autostandby`, `autosuspend`,
`autoturn`, `autowarmth`, `keepalive`). Reading, file manager, covers,
dictionary, gestures and the GleWorks theme/home plugins remain enabled.

A diagnostic run with the whole plugin directory removed reached ~1.38 s, but
that also removed the GleWorks Return-to-Home plugin and is not a deployable
configuration.  An earlier measurement confused that diagnostic state with the
trimmed state; the table above is the corrected, plugins-present measurement.

The list lives in the KOReader settings seed and is applied to the running
device. The first user check reported a visibly faster launch, but the plugin
directory was accidentally nested during that check; after restoring it the
trimmed configuration must be timed again. The previous settings file is kept as
`/opt/koreader/settings.reader.lua.before-plugin-trim`.

## Current device build

```text
/usr/sbin/gw-shell    80160c3a0704ebd6857176564c38397b357c92adf8d3eac5f1d7d6fb63115ff4
/usr/sbin/gw-system   d1c52103d9b78a97acaa78406a4f19e07f2b945a0dbf48378a9c01017678829a
```

Rollback:

```text
/usr/sbin/gw-shell.before-audit-20261101
/usr/sbin/gw-system.before-audit-20261101
```
