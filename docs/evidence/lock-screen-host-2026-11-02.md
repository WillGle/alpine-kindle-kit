---
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Lock screen customisation — host checks (2026-11-02)

Status: **host-only evidence.** No physical PW3 was booted, no framebuffer was
written and no device partition or boot path was changed.

## Scope

- `gw-shell` lock card, clock/date toggles, message slot and raw image apply.
- `gw-lockscreen.koplugin` sleep-screen export to raw 8 bpp.
- Config and image paths under `/mnt/us/gleworks/`.

## Results

| Check | Result |
| --- | --- |
| `make check CC=cc` | **CHECK PASSED** |
| UI geometry | **2868 checks, 0 failed** |
| Lock text checks | clock/date visible; message hidden by default |
| Lua syntax (`qemu-arm` + KOReader `luajit`) | **OK** |
| Device write | **none** |

The Lua check only compiles the plugin; it does not exercise KOReader's
Blitbuffer export path.

## Not proven

- The exported raw image matches the KOReader sleep screen on hardware.
- Image scale, rotation and letterboxing on the PW3 panel.
- Lock latency with a 1.5 MB image read from FAT.
- Message wrapping and clock/date toggles on the device.
- Fallback when the raw file is removed while locked.

## Reproduction

```sh
nix-shell tools/build/nix-shell.nix --run 'make check CC=cc'
```

See [`../reference/lock-screen.md`](../reference/lock-screen.md) and
[`../how-to/customize-lock-screen.md`](../how-to/customize-lock-screen.md).
