---
diataxis: reference
status: current
updated: 2026-11-03
verified_build: none
supersedes: lock-screen-host-2026-11-02
audience: maintainer
---

# Sleep and lock screen contract

Status: **KOReader's own sleep screen is authoritative while reading.** The
custom sleep-screen export plugin was removed on 2026-11-03. The OS shell keeps
a small text lock screen for the Home screen only.

## While KOReader is foreground

1. The physical power button belongs to `gw-system` (exclusive evdev grab).
2. `gw-system` darkens the frontlight and broadcasts `POWER LOCK`.
3. `gwctl watch` writes `locked` to `/run/gw-lock-state`.
4. `gw-home.koplugin` polls that file once per second and calls KOReader's
   `Screensaver:setup()` + `Screensaver:show()`.
5. `gw-system` suspends the kernel after its delay.

On wake:

1. `gw-system` restores the frontlight and broadcasts `POWER WAKE`.
2. `gwctl watch` writes `awake`.
3. `gw-home.koplugin` calls `Screensaver:close()` and repaints, returning to the
   page that was being read.

KOReader's sleep-screen mode (cover, last page, custom image, random image,
message) is configured entirely in KOReader. If the mode is `disable`, KOReader
shows nothing and the last page simply stays on the panel with the frontlight
off.

## While the shell is foreground

`gw-shell` shows its own text lock screen. The fixed config is:

```text
/mnt/us/gleworks/lockscreen.conf
```

```text
version=1
use_image=0
image=/mnt/us/gleworks/lockscreen.raw
show_panel=1
show_clock=1
show_date=1
message=
```

- `show_panel`: `0` hides the whole white card (clock, date, message and
  "Tap to unlock"); the image or plain background remains.
- `show_clock`, `show_date`: `0` hides that line.
- `message`: one line, at most 159 bytes; empty hides it.
- `use_image`: retained for compatibility. No component writes
  `lockscreen.raw` after the export plugin was removed, so it normally stays
  `0`.
- Unknown keys are ignored. A missing or malformed config falls back to clock +
  date, no message.

The shell reads the config with a regular-file check and never accepts an
arbitrary path. While locked it polls the config mtime once per second.
Settings → Lock screen exposes toggles for the card, clock and date and a
"Clear message" action; each writes this same file atomically.

## Image import engine

`gw-lockscreen-import` polls `/mnt/us/gleworks/upload/` every 10 seconds.
`convert-lockscreen.lua` decodes PNG/JPEG through KOReader's LuaJIT and bundled
libpng/TurboJPEG, cover-crops to 1072×1448, writes raw 8 bpp, sets
`use_image=1`, and deletes the source. Bad files are renamed `*.failed`.

## Security and privacy

- No network access and no shell execution from the lock screen.
- `/mnt/us` is FAT: the config message is readable when the device is mounted
  over USB. Do not put secrets in the message.
- The reader plugin only reads `/run/gw-lock-state` and calls KOReader's own
  screensaver; it does not write to `/mnt/us`.

## Acceptance

- Host: lock geometry and text checks pass; Lua syntax check passes.
- Device: power-button lock while reading shows the configured KOReader sleep
  screen; power-button wake returns to the page.
- Device: shell lock screen still shows clock/date/message at Home.
- No full refresh outside the existing lock policy.
