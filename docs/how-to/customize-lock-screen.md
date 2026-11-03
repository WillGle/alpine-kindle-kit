---
diataxis: how-to
status: current
updated: 2026-11-03
verified_build: none
supersedes: none
audience: user
---

# Use KOReader's sleep screen

Status: **current.** KOReader's built-in sleep screen is used while reading.
There is no export step and no custom sleep-screen plugin.

Contract: [`../reference/lock-screen.md`](../reference/lock-screen.md).

## 1. Choose the sleep screen in KOReader

Open KOReader → **File manager** or **Reader** menu → **Screen** →
**Sleep screen** and choose one of:

- last page;
- book cover;
- custom image;
- random image from a folder;
- reading progress or book status, if available.

This is exactly what the panel shows when the device sleeps. KOReader's default
is `disable`; choose a mode explicitly if you want an image.

## 2. Lock and wake

Press the physical power button while reading:

- the frontlight turns off;
- KOReader paints the configured sleep screen;
- `gw-system` suspends the device.

Press the power button again:

- the frontlight returns;
- KOReader dismisses its sleep screen and shows the page you were reading.

There is no separate export or "apply" action.

## 3. Upload an image (automatic)

Drop a PNG or JPEG into:

```text
/mnt/us/gleworks/upload/
```

The on-device engine (`gw-lockscreen-import` + `convert-lockscreen.lua`) runs
every 10 seconds:

- decodes the image with KOReader's LuaJIT and bundled libs;
- cover-crops it to 1072×1448 and converts it to 8 bpp grayscale;
- atomically replaces `/mnt/us/gleworks/lockscreen.raw`;
- sets `use_image=1` in `lockscreen.conf`;
- deletes the uploaded source so the FAT partition does not keep two copies.

A file that cannot be decoded or has the wrong size is renamed
`<name>.failed` and kept for inspection. The log is
`/run/gw-lockscreen-import.log`.

## 4. Home screen text (optional)

When KOReader is not running, `gw-shell` shows a small text lock screen. Edit
`/mnt/us/gleworks/lockscreen.conf`:

```text
version=1
use_image=0
show_panel=1
show_clock=1
show_date=1
message=
```

- `show_panel=0` hides the whole card, leaving only the image.
- `show_clock=0` / `show_date=0` hides that line.
- `message` is one line under the date.
- The file is re-read while the lock screen is visible.

## 5. Verify on the device

```sh
gwctl lock      # darkens and locks
gwctl unlock    # wakes and unlocks the shell
```

While KOReader is foreground, use the physical power button instead: `gwctl`
lock is handled by `gw-system` and mirrored through `/run/gw-lock-state`.

## 6. Troubleshooting

| Symptom | Check |
| --- | --- |
| No sleep image | KOReader → Screen → Sleep screen is not `disable` |
| Sleep screen stays after wake | `cat /run/gw-lock-state` must become `awake`; `gwctl watch` must be running |
| Home lock screen has no text | `lockscreen.conf` exists and is a regular file |
| Old export menu still appears | Remove `/opt/koreader/plugins/gw-lockscreen.koplugin` from the device |

## Limits

- The plugin path is managed by the reader image; a standalone KOReader is not
  changed.
- `/mnt/us` is FAT, so the Home message is readable over USB. Do not put
  secrets in it.
