---
status: current
updated: 2026-11-03
verified_build: 990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069
supersedes: none
audience: maintainer
---

# Settings time/device and lock-screen image (2026-11-03)

Status: **device-verified on the PW3 trial shell.**

## Scope

- Settings gains **Device name**, **Time zone**, **Date and time** and
  **Sync now** rows.
- Date/time is entered manually as `YYYY-MM-DD HH:MM` and converted with the
  selected time zone. Network sync uses the existing `TIME SYNC` path.
- Device name is persisted in `/var/lib/gleworks/device.conf` and applied with
  `sethostname`.
- Time zones map region names to POSIX TZ strings because the image has no
  `tzdata` package. Supported examples: `UTC`, `Asia/Ho_Chi_Minh`,
  `Asia/Tokyo`, `Europe/London`, `Europe/Paris`, `America/New_York`,
  `America/Chicago`, `America/Los_Angeles`, `Australia/Sydney`,
  `Pacific/Auckland`.
- The lock-screen image was replaced with `joke-bear-study.jpg` through the
  existing upload engine.

## Build

| Binary | SHA-256 |
| --- | --- |
| `gw_shell` | `990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069` |
| `gw_system` | `db2b54185ce3eeeec744786e8a86e66fa42dc7b7dfcf341ff45344f6f9556fd0` |

Boot ID `6986b82c-62b7-4ef5-ae98-390e55cf339f` was unchanged.

## Device results

| Check | Result |
| --- | --- |
| Settings rows render | **PASS** — device name, time zone, date/time, sync |
| Device name editor opens with current value | **PASS** |
| Time-zone editor opens with current value | **PASS** |
| Save time zone (`Asia/Ho_Chi_Minh`) | **PASS** — `TIME GET` returned UTC+7 and the status clock updated |
| Manual date/time save | **PASS** — time was rewritten from `2026-11-03 19:50` and persisted |
| Device clock correction | **PASS** — device clock was several hours ahead; manual set corrected it |
| Lock-screen image import | **PASS** — `lockscreen.raw` 1,552,256 bytes; lock preview matched the uploaded image |
| Host checks | **CHECK PASSED** |

The image upload was copied over the LAN into
`/mnt/us/gleworks/upload/`; the on-device loop imported it within one cycle and
deleted the source. The resulting lock screen is shown in
[`screenshots/lock-image.png`](screenshots/lock-image.png).

## Not proven

- Time synchronization through `ntpd` on a live network path.
- Automatic timezone detection; the zone is selected manually.
- DST transition behavior on every mapped region.
- Real finger typing in the modal; the modal path was exercised with injected
  touch and the on-screen keyboard was not used for arbitrary text.

## Screenshots

- [`settings-device-time.png`](screenshots/settings-device-time.png)
- [`device-name-modal.png`](screenshots/device-name-modal.png)
- [`timezone-modal.png`](screenshots/timezone-modal.png)
- [`settings-correct-time.png`](screenshots/settings-correct-time.png)
- [`lock-image.png`](screenshots/lock-image.png)
