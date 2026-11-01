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
