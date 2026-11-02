---
diataxis: reference
status: draft
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Lock screen contract

Status: **device trial deployed; KOReader menu export pending.** The shell
renders the OS lock screen with and without a raw image on the PW3. KOReader
only writes the image and config; it does not draw the lock screen.

## Files

```text
/mnt/us/gleworks/lockscreen.conf   user settings, regular file
/mnt/us/gleworks/lockscreen.raw    1072 x 1448, 8 bpp, row-major
```

The path is fixed. The shell never accepts an arbitrary image path from the
config.

## Config

```text
version=1
use_image=1
image=/mnt/us/gleworks/lockscreen.raw
show_clock=1
show_date=1
message=
```

- `use_image`: `1` shows the raw image; `0` uses the plain white lock screen.
- `image`: informational; the shell always reads the fixed path.
- `show_clock`, `show_date`: `0` hides that line.
- `message`: one line, at most 159 bytes; empty hides it.
- Unknown keys are ignored. A missing or malformed config falls back to clock +
  date, no image, no message.

## Raw image

- Exactly `1072 * 1448 = 1552256` bytes.
- One byte per pixel, row-major, no padding.
- The shell rejects a different size or a symlink.
- The image is cached by modification time; a lock does not re-read an
  unchanged file.

## KOReader plugin

`gw-lockscreen.koplugin` is active only when `GW_MANAGED_READER=1`.

Menu: **Gleworks lock screen**

| Item | Behaviour |
| --- | --- |
| Use current sleep screen | Render the effective KOReader sleep screen to raw and enable it |
| Show clock | Toggle `show_clock` |
| Show date | Toggle `show_date` |
| Set message... | Edit `message` |
| Text only | Set `use_image=0` and remove the raw file |

The plugin reads the existing KOReader sleep-screen settings, so cover, custom
image and random-image modes are honoured. `bookstatus` and `readingprogress`
are rendered as widgets when available; otherwise the plugin falls back to text
only.

## Shell behaviour

- On lock, `gw-shell` reads the config and applies the image before the full
  GC16 lock refresh.
- While locked, it polls the config modification time once per second and
  reloads on change.
- Clock, date, message and `Tap to unlock` sit on a white card over the image.
- The whole screen remains the unlock target; the card and image are not
  clickable.
- If the image is missing, unreadable or the wrong size, the shell falls back to
  the plain lock screen.

## Security and privacy

- No network access and no shell execution.
- The raw file is read with `O_NOFOLLOW` and an exact size check.
- `/mnt/us` is FAT: the image and message are readable when the device is
  mounted over USB. Do not put secrets in the message.
- The plugin writes only the fixed paths above.

## Acceptance

- Host: lock geometry and text checks pass.
- Device: lock/unlock with an image, image removal fallback, message wrapping,
  and clock/date toggles.
- No full refresh outside the existing lock policy.
