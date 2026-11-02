---
diataxis: tutorial
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Tutorial 1 — build a private PW3 candidate

This tutorial takes a prepared Linux host to a private, source-built Alpine
candidate. It does not install anything on a device. Read
[`../reference/device-profile-pw3.md`](../reference/device-profile-pw3.md) and
[`../how-to/build-private-candidate.md`](../how-to/build-private-candidate.md)
before starting.

## Before you begin

- A rooted/jailbroken PW3 matching `pw3-0kb-5.16.2.1.1`, with a private backup
  of p1 and the stock modules.
- A Linux x86-64 host with Python 3.10+, `make`, a C compiler, `e2fsprogs`,
  `qemu-arm`, and the private ARM toolchain described in the build guide.
- At least 5 GiB free for inputs, builds and images.
- A separate work directory. Never build inside a mounted device root image.

## Steps

1. Enter a clean host environment:

   ```sh
   nix-shell tools/build/nix-shell.nix
   ```

2. Run the host gate before building:

   ```sh
   make check
   ```

3. Assemble the base candidate. Replace every `<...>` placeholder with a path
   on your host:

   ```sh
   python3 tools/build/pw3-build.py \
     --rootfs <alpine-minirootfs.tar.gz> \
     --modules <stock-modules.tar> \
     --p1-backup <p1.ext3> \
     --return-launcher <gleworks-alpine-session.sh> \
     --cross-prefix <arm-toolchain-prefix> \
     --kernel-source <vendor-linux> \
     --kernel-build <kernel-build-dir> \
     --musl-prefix <musl-prefix> \
     --linux-headers <linux-uapi-include> \
     --output <new-private-candidate>
   ```

4. Inspect `candidate.json` in the output. Confirm the profile, image size,
   payload hashes and variant are what you intended.

5. Build the optional reader layer by adding
   `--variant reader --reader <clean-koreader>`. The base candidate alone does
   not include `gw-system`, `gw-shell`, `gwctl`, `gw-appctl`, OpenRC, Wi-Fi or
   Tailscale.

## What success looks like

The candidate directory contains a filesystem image and a manifest whose hashes
verify. Host checks and QEMU execution are not device acceptance; continue with
[`03-first-boot.md`](03-first-boot.md) only through the supervised install flow.
