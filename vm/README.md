# Local QEMU gate — x86-64 Linux

This directory is an independent, disposable workspace for running the target
tests before touching a PW3. It is not part of the device image and its
artifacts are ignored.

## Supported host

- Linux, **x86-64 / AMD64 / 64-bit only**.
- `qemu-arm` (QEMU user-mode) for target binaries.
- Host build tools: `python3`, `make`, `cc`, and the private ARM toolchain used
  by `tools/build/build-arm-binaries.sh`.
- The ARM prepared root from the private build tree, or set `PW3_ROOT`.

The scripts fail closed on any other platform. They never open `/dev/fb0`,
`/dev/input`, a USB console, or a network socket to a device.

## Standard gate (run before every device test/deploy)

```sh
# 1. Host suite: Python, shell, system/installer/UI/tooling tests.
./vm/check-host.sh

# 2. QEMU target gate: ARM policy tests + ARM signature/fingerprint test
#    + ARM CLI smoke, executed with qemu-arm on this x86-64 host.
./vm/qemu-arm/run.sh

# 3. Rebuild the ARM device binaries and stage them.
env -u CC -u STRIP -u CFLAGS \
  bash tools/build/build-arm-binaries.sh

# 4. Only after 1–3 pass: upload/deploy with the USB console procedure.
```

`vm/qemu-arm/run.sh` defaults `PW3_ROOT` to
`work/build/pw3-reader-storage-20261101/root` and builds into a temporary
directory under `/tmp`. Override `PW3_ROOT`/`OUT` to use another prepared root.

## Why user-mode QEMU, not a bootable Kindle kernel

The repository does not contain a QEMU-bootable PW3 kernel. QEMU user-mode
executes the exact ARM/musl binaries and test programs that run on the device,
on an x86-64 host, with the same libc ABI. It covers policy, installer
signature/fingerprint and CLI startup without pretending to validate the
device's EPDC, touch controller, power button or USB gadget.

For a full-system VM, supply your own ARM kernel and rootfs image under
`vm/images/` (ignored); do not add private images to git. The mandatory gate
above remains the minimum.

## Results

Temporary outputs are in `/tmp/pw3-qemu-arm.*`. Set `OUT=vm/results/<name>` if
you want to retain logs; `vm/results/` is ignored.
