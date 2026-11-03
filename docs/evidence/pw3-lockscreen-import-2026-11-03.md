---
status: current
updated: 2026-11-03
verified_build: none
supersedes: none
audience: maintainer
---

# Lock-screen image import engine (2026-11-03)

Status: **deployed and device-verified.** Drop a PNG or JPEG into
`/mnt/us/gleworks/upload/`; the Kindle converts it to the panel's raw 8 bpp
format, replaces the lock-screen image atomically, enables it, and deletes the
uploaded source.

## Why

The PW3 has no ImageMagick, ffmpeg or Python. KOReader's LuaJIT and bundled
`liblodepng` / `libturbojpeg` are the only image decoders on the device. The
engine reuses them instead of shipping a second decoder.

## Files

| Path | Purpose |
| --- | --- |
| `/usr/sbin/gw-lockscreen-import` | scans the upload folder, calls the converter, updates config, deletes the source |
| `/usr/lib/gleworks/convert-lockscreen.lua` | PNG/JPEG decode, cover-crop to 1072×1448, 8 bpp grayscale |
| `/mnt/us/gleworks/upload/` | USB-visible drop folder |
| `/mnt/us/gleworks/lockscreen.raw` | generated image, 1552256 bytes |
| `/run/gw-lockscreen-import.log` | conversion log |
| `/run/gw-lockscreen-import.loop` | per-boot loop guard |

Hashes deployed on 2026-11-03:

```text
gw-lockscreen-import       f9884773425a1e0309509bce4a70d41d78abce925507c636d1d7c7d0380db3ab
convert-lockscreen.lua     8456b51dc91e562c3e1656a26418dc6b6e940be44277224bbca1517941dc8eb9
gleworks-early.sh          6301e7cc83f839cca3c091b3c46be1f9085212be31d33087eff947485d770daa
```

`gleworks-early.sh` starts the loop at boot; the loop guard prevents a second
instance if the script is re-run.

## Behaviour

1. The loop scans `upload/` every 10 seconds.
2. A file whose mtime is younger than 3 seconds is skipped, so a USB copy is
   not decoded mid-write.
3. The converter decodes PNG (lodepng) or JPEG (TurboJPEG), cover-crops to
   1072×1448, and writes 8 bpp grayscale.
4. The shell writes to `lockscreen.raw.tmp`, checks the exact 1552256-byte
   size, syncs, and renames.
5. `lockscreen.conf` gets `use_image=1`; other keys are preserved.
6. The uploaded source is deleted. A bad file is renamed `<name>.failed` and
   kept for inspection.

## Device verification

```text
drop auto.jpg   -> within 12s: upload/ empty, log "imported auto.jpg"
                   lockscreen.raw 1552256 bytes, new hash
drop final.png  -> within 12s: upload/ empty, log "imported final.png"
                   lockscreen.raw sha256 b686197e… (matches the host conversion)
immediate run   -> file skipped (mtime < 3s)
after 4s        -> file imported and deleted
```

The PNG conversion produced the same raw SHA-256 as the host ImageMagick
pipeline (`b686197ec56c30fa57380f340ae08811527068a83b4007a92850100c9ff4c7f4`),
so the on-device decoder is pixel-identical for that input.

## Rollback

Remove `/usr/sbin/gw-lockscreen-import` and
`/usr/lib/gleworks/convert-lockscreen.lua`, and delete the loop block from
`/etc/gleworks-early.sh`. The lock screen falls back to the existing raw image
or the text-only layout.
