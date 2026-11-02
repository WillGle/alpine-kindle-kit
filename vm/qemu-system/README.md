# Optional full-system ARM VM

The mandatory gate is `../qemu-arm/run.sh` (QEMU user-mode, exact ARM/musl
binaries on Linux x86-64). A full-system VM needs a QEMU-bootable ARM kernel
and rootfs that the repository does not ship.

Put them in `vm/images/` (ignored) and run:

```sh
PW3_QEMU_KERNEL=vm/images/zImage \
PW3_QEMU_ROOTFS=vm/images/rootfs.ext3 \
./vm/qemu-system/run.sh
```

The script only provides a conservative `virt` machine + virtio rootfs boot.
Adjust `PW3_QEMU_APPEND` or wrap `QEMU_SYSTEM_ARM` for a different machine or
console. Do not commit images, keys or device captures.

## Device-prepare boot test

`device-prep-test.sh` boots a minimal initramfs with the same ARM busybox and
`os/system/gleworks-device-prepare.sh` functions used on the device. It
simulates one touch module as already loaded and asserts that a second pass
does not insmod it again.

```sh
curl -sS -o /tmp/pw3-vmlinuz \
  http://ftp.debian.org/debian/dists/stable/main/installer-armhf/current/images/netboot/vmlinuz
PW3_QEMU_KERNEL=/tmp/pw3-vmlinuz \
  ./vm/qemu-system/device-prep-test.sh
```

`bootstrap-failure-test.sh` boots `bootstrap.c` as PID1 without `/proc`,
forcing the early-mount halt path. It passes when the log shows
`GW_BOOTSTRAP_HALT` followed by `reboot: Restarting system` and QEMU exits
rather than timing out.

The kernel used for the recorded run had SHA256
`b508abca3127cb0d9e1b3aba37f400cbd7dac170040641756807e0e496039de8` (Debian
armmp 6.12). Any 32-bit ARM kernel that boots QEMU `virt` works.
