> **Superseded for scroll/ghost policy (2026-11-01, later).** The device now
> runs `gw-shell` `df2c9bd7…` with 6 previews/160 ms and an idle full GC16
> 1.2 s after scrolling stops. This directory remains the capture evidence for
> build `149ae024…`; see
> [`../pw3-wifi-scroll-fix-2026-11-01.md`](../pw3-wifi-scroll-fix-2026-11-01.md)
> for the final policy and Wi-Fi list fix.

# PW3 UI refresh / UX remediation — device evidence

Status: **Phase 0 complete on PW3** (2026-11-01, device clock 02:24–02:43 UTC).

This directory contains the host evidence and the on-device Phase 0 capture for
[`docs/technical/PW3-UI-REFRESH-UX-PLAN-2026-11-01.md`](../../technical/PW3-UI-REFRESH-UX-PLAN-2026-11-01.md).

## Deployed build

| Item | Value |
| --- | --- |
| `/usr/sbin/gw-shell` sha256 | `149ae0243ded075cfe16ab44e8fa3beabf9f3cf45024442177184899e5c36a05` |
| Top bar network indicator | one signal-bar widget only (no duplicate Wi-Fi glyph) |
| Settings storage | reads `/mnt/us` and shows `871 MB free of 3.0 GB` (user partition, not rootfs) |
| USB indicator | `LV_SYMBOL_USB USB` shown when `usb=host-class`; hidden for `none`/`power` |
| Build flags | `EPDC_accepted=1 touch_accepted=1`, `GW_UI_PERF=1`, `GW_UI_DIAG=1` |
| Paint policy | `/etc/gleworks/paint-policy` = `quality` (legacy) |
| Scene waveform | no `/etc/gleworks/scene-waveform`; resolved to `GC16_FAST` (3) |
| Original rollback | `/usr/sbin/gw-shell.before-refresh-20261101` = `6020c6fe4633b74ac1ad95e0e5db3ac5e0d27e54dc94a178ecc31f43314b91df` |
| Additional rollbacks | `before-cardfix`, `before-nodeepclean`, `before-swipefix` |
| Running process | `/usr/sbin/gw-shell` under `gw-system --supervise` |
| Release build (same source, perf/diag off) | `149ae0243ded075cfe16ab44e8fa3beabf9f3cf45024442177184899e5c36a05` |

`paint-policy=quality` no longer means GC16-per-scene: the shell maps every
legacy policy to the 16-level `GC16_FAST` partial. GC16 full is opt-in through
`GW_UI_SCENE_WAVEFORM=gc16` or the lock/unlock deep clean.

## Nine-action capture

Captured on build `61b78d09…` (same UI/final code, before the scroll-preview
change). Raw log: [`waveform-2026-11-01.log`](waveform-2026-11-01.log).

| # | Action | submits | `waveform=2` | `mode=1` | Result |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | bottom-nav tap | 1 | 0 | 0 | `waveform=3 mode=0 intent=scene` |
| 2 | Home card tap | 2 | 0 | 0 | two scene partials (Home then Calendar) |
| 3 | To-do checkbox | 2 | 0 | 0 | scene + row/content partial |
| 4 | Scan Wi-Fi | 13 | 0 | 0 | content/row partials only |
| 5 | Apps empty state (no user apps installed) | 1 | 0 | 0 | scene partial; Remove row N/A |
| 6 | Lock / unlock | 3 | **1** | **1** | one `intent=deep_clean reason=lock` GC16; unlock is a scene partial |
| 7 | keyboard open/close | 5 | 0 | 0 | content partials with `text=1`; no GC16 |
| 8 | modal open/close | 3 | 0 | 0 | scene partials only |
| 9 | scroll (ui-scroll) | 2 | 0 | 0 | scene + one `intent=scroll`, no GC16 |

All submits carry `merge_risk=0`; no AUTO waveform was observed.

### 20-tap flash count

Raw log: [`20-taps-2026-11-01.log`](20-taps-2026-11-01.log).

```text
submits 21   (20 scene taps + 1 clock/content update)
gc16    0
full    0
```

Screenshot after the run: [`after-20-taps.png`](after-20-taps.png).

### Lock / unlock

Raw log: [`lock-2026-11-01.log`](lock-2026-11-01.log).

```text
waveform=2 mode=1 intent=deep_clean reason=lock   (one full GC16)
waveform=3 mode=0 intent=scene                    (unlock -> Home)
GW_SHELL: ignoring stale lock event
```

Screenshot: [`after-lock.png`](after-lock.png)

USB icon on device: [`device-usb-icon.png`](device-usb-icon.png). — lock time `02:43`, label
`Locked`, full refresh is the allowed U3 cleanup point.

### Scroll preview (build `149ae024…`)

Captured after the live-preview + REAGL change. Raw log:
[`scroll-preview-reagl-2026-11-01.log`](scroll-preview-reagl-2026-11-01.log).

The diagnostic scroll command moves the real scroll object and exercises the
same intent/present path (evdev gesture injection is not available over SSH):

```text
epdc submit waveform=3 mode=0 intent=scroll reason=scroll_preview ...
epdc submit waveform=3 mode=0 intent=scroll reason=scroll_preview ...
epdc submit waveform=8 mode=0 intent=scroll reason=scroll_release ...
GW_UI_DIAG: drag dy=-600 top=600
```

A 600 ms held drag produces two live previews (`GC16_FAST`) plus one final
`REAGL` submit on release. REAGL cleans the residue without a GC16 flash, so
the scroll is both responsive and anti-ghost. The 20-tap and nine-action
logs above were captured on `61b78d09…`; this preview test on `149ae024…`. Preview throttle is 250 ms, max 4 previews;
tune with `GW_UI_SCROLL_PREVIEW_MS` and A/B waveform with
`GW_UI_SCROLL_WAVEFORM`.

## Ghost score

The framebuffer capture does not contain e-ink ghost residue, so a numeric
ghost score cannot be derived from `/dev/fb0`. What is verifiable:

- there is no GC16/black-white inversion on taps, scan, keyboard, modal or
  scroll (0 of 20 taps);
- lock performs exactly one full GC16 cleanup, which is the designated
  ghost-cleanup point;
- no partial-residue path was observed outside lock in the action capture.

Physical ghost observation (after 20 taps, before lock) remains a human visual
check on the panel; it is not machine-readable over SSH.

## How this was captured

1. Built with EPDC/touch/perf/diag enabled and deployed with rollback copies.
2. Enabled perf logging with `/run/gw-ui-perf`.
3. Drove actions with `gwctl ui-tap`, `gwctl ui-nav`, and `gwctl ui-scroll`
   through `/run/gw-shell.diag`; the shell logged every EPDC submit to
   `/run/gw-system.log`.
4. Captured the framebuffer with `tools/maintenance/capture-screen.py`.

Cleanup performed after capture:

- temporary dropbear and `/root/.ssh/authorized_keys` removed;
- intermediate rollbacks `before-cardfix`, `before-nodeepclean`,
  `before-swipefix`, `before-trayfix`, `before-preview` removed; only the original
  `/usr/sbin/gw-shell.before-refresh-20261101` remains;
- `/usr/bin/gwctl` is now a symlink to `/usr/sbin/gwctl`;
- `/tmp/nc.err` removed and `/run/gw-system.log` truncated;
- `/run/gw-ui-perf` removed; service restarted with perf logging off;
- To-dos task restored to active; device left at Home, `lock=unlocked`,
  `display=on`.

Device-facing summary and rollback command:
`/etc/gleworks/UI-REFRESH-2026-11-01.txt`.
