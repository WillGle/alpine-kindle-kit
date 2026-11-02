---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# PW3 device profile

The supported hardware profile is deliberately narrow. Values come from
`os/devices/pw3/profile.json`; do not add an override to bypass them.

| Field | Value |
| --- | --- |
| Profile name | `pw3-0kb-5.16.2.1.1` |
| Model prefix | `G090KB` |
| Device code | `0KB` |
| Firmware | `5.16.2.1.1` |
| Kernel | `3.0.35-lab126` |
| Stock init SHA256 | `ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a` |
| p1 start sector | `65536` |
| p1 size | `921600` sectors |
| p4 size | `6385664` sectors |
| Minimum image | `134217728` bytes (128 MiB) |
| Maximum image | `1073741824` bytes (1024 MiB) |
| Minimum battery | `50` percent |
| First installer hardware verified | `false` |

## Display and input

| Item | Value |
| --- | --- |
| Panel framebuffer | `1072 × 1448`, 8 bpp, stride 1088 |
| Display controller | i.MX6 EPDC, vendor waveform |
| Touch | `cyttsp4_mt` |
| Power key | `max77696-onkey` |
| Backlight | `/sys/class/backlight/max77696-bl/brightness` |

## Partition roles

| Partition | Role |
| --- | --- |
| p1 | Stock firmware, static bootstrap, `init.gleworks-stock` |
| p3 | Amazon data area restored from factory templates |
| p4 | FAT control volume at offset 8192, mounted at `/boot/gleworks-control`; `/mnt/us` points to it |
| `gleworks-alpine/system.ext3` | Alpine root image on p4 |

Other PW3 model codes, firmware versions and partition layouts are unsupported
until separately qualified. Captured vendor modules and p1 backups are private
per-device inputs.
