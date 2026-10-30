# Exact stock kernel plus Alpine early userspace — host hypothesis

`build-host.py` creates a private host-only legacy MULTI uImage containing the
captured stock kernel unchanged and a separate uncompressed Alpine3.24.3 CPIO.
The source U-Boot supports kernel-first/initramfs-second components and passes
ATAG_INITRD2. Stock kernel supports external raw CPIO and executes `/init`
before mounting its configured root. If this works on hardware, Alpine
BusyBox becomes PID1 without starting Amazon Upstart or mounting Amazon rootfs.

This route keeps installed U-Boot and exact stock drivers. It still uses an
Amazon-derived Linux3.0.35 kernel, with its limitations: current Go/Tailscale
requires a newer kernel. It does not yet satisfy all device requirements.

Stock DEVTMPFS is disabled. The candidate creates console/null nodes, mounts
proc/sysfs, loads captured OTG/UDC/serial modules and derives ttyGS0's dynamic
major from `/proc/devices`. USB responds to a fresh challenge only when release,
kernel and `/proc/1/exe` match. It has no shell, network setup, storage mount,
filesystem write, reboot or watchdog-open command. Built-in stock kernel
drivers remain active; storage is not disabled as in the modern diagnostic.

This is a format/boot-source hypothesis, **not a qualified install package**.
Production bootloader MULTI handling, cold decompressor/reserved memory, early
USB ordering and Alpine ABI still require validation. No ordinary-USB rescue
has been demonstrated. No flash command is provided; do not write the candidate
to either kernel region or change a boot selector. Private output contains
captured device kernel/modules and stays on the host.

The current reversible alternative is to investigate PID1 replacement in RAM
after obtaining authenticated control. Vendor Upstart0.6.6 lacks modern reexec;
`telinit u` is not a usable takeover mechanism. Any later replacement needs its
own bounded failure/reset and userspace/process/mount review. A normal chroot
is only a compatibility check, not independent boot or permission to erase OS.
