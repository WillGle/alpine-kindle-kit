# schuhumi/alpine_kindle reference

User selected this project as a reference on 2026-10-30.

- Upstream: https://github.com/schuhumi/alpine_kindle
- Local checkout: `alpine-kindle-reference/`
- Reviewed commit: `dc87d5b63856596e440f89363e2dcd63eb9797b3`
- Commit date: 2019-09-07. Working tree was clean when reviewed.
- Scope: source inspection only; no upstream installer or device script run.

README.md:2 reports testing on Paperwhite 3. README.md:18 describes a chroot
using the existing Kindle Linux/X11 environment. This is relevant PW3
userspace experience, not evidence of independently booting a new kernel.

## What to reuse or investigate

| Reference | Useful information | Application to our independent OS |
| --- | --- | --- |
| `create_kindle_alpine_image.sh:103` | Bootstrap Alpine ARM userspace with apk and host QEMU | Compare package/dependency setup with our pinned Alpine ARMv7 build |
| `create_kindle_alpine_image.sh:28` | Desktop, keyboard and application package examples | Evaluate only packages needed for reading and a terminal after native boot |
| `create_kindle_alpine_image.sh:60` | Display geometry, Xephyr and touch/application integration | Reference UI behavior; replace the dependency on Amazon's X server/window manager |
| `alpine.sh:11` | Image mounting and resource bindings | Understand host dependencies; independent Alpine must own its device/service setup |
| `README.md:27` | Concurrent USB mass-storage and mounted-rootfs corruption risk | Never export a filesystem writable over USB while the running OS uses it |

## Dependencies that do not transfer directly

- `alpine.sh:22` enters `chroot /tmp/alpine /bin/sh`; the Amazon kernel
  and system remain running.
- `alpine.conf:24` stops `lab126_gui`, then line26 starts the chroot through
  Amazon Upstart. Stopping that GUI does not replace the OS or PID1.
- `alpine.sh:12-16` bind-mounts existing `/dev`, `/proc`, `/sys` and DBus.
  It does not provide new PW3 kernel drivers or independent power management.
- The nested desktop uses Amazon's existing X display `:0` and window naming.
  It is not a standalone e-ink display/touch driver implementation.
- The historical bootstrap uses HTTP, moving edge/latest-stable repositories
  and `--allow-untrusted` (`create_kindle_alpine_image.sh:17,108,123-127`).
  Keep our pinned release, verified HTTPS and signed package verification.
- The example `alpine/alpine` credentials are not target-system defaults.

No native kernel handoff, DMA shutdown or ordinary-USB boot recovery recipe
was found in this checkout. Keep this project as a userspace/UI reference.
The accepted goal remains: installed bootloader retained, Amazon OS replaced
by independently booted Alpine, with no jailbreak runtime dependency.
