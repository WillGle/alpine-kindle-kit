---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Building the private PW3 candidate

The public archive is source only. Build inputs are not silently downloaded
or copied from the author's workspace. Build output is private to your device.

## Dependencies and inputs

On a Linux host install Python 3.10+, OpenSSH client (`ssh`, `ssh-keygen`),
e2fsprogs (`mkfs.ext3`, `debugfs`, `e2fsck`), QEMU user ARM (`qemu-arm`),
GNU make, `patch`, a native C compiler, Dropbear's host `dropbearkey` utility,
and UDisks2 (`udisksctl`, D-Bus service),
`findmnt` and `gdbus` (GLib). Your desktop/user needs permission to use the
Kindle's removable disk and `/dev/ttyACM*`. No `sudo` or new host daemon is
started by these tools. Use paths without whitespace for the legacy build.

Required inputs:

| Input | Provenance / expected value |
| --- | --- |
| Alpine minirootfs | `https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/armv7/alpine-minirootfs-3.24.3-armv7.tar.gz`; SHA256 `dc1ddfaf03ede4be79ad7475490f6b7859dbcbbbbb85150aa1ee013b4bc2f0c1` |
| KOReader | Clean `kindlepw2` release `v2026.07.1` from `https://github.com/koreader/koreader/releases/tag/v2026.07.1`; retain bundled licenses. Do not use a personal reader install with saved credentials/history. |
| p1 + modules | `backup/p1.ext3` and `backup/stock-modules.tar` from your private installer session |
| Return launcher | `gleworks-alpine-session.sh` from your own private control bundle |
| Kernel source | Amazon `Kindle_src_5.16.2.1.1_4097470002.tar.gz`, `https://kindledownloads.s3.amazonaws.com/Kindle_src_5.16.2.1.1_4097470002.tar.gz`; recorded SHA256 `af3355ecc855d4c91ab86b3442ec05209b3748e3bf594277cba3261f2e6bafa5` |
| ARM compiler | Soft-float `arm-linux-gnueabi-`; reference GCC 4.9.4 from `https://kernel.org/pub/tools/crosstool/files/bin/x86_64/4.9.4/x86_64-gcc-4.9.4-nolibc-arm-linux-gnueabi.tar.xz`, recorded SHA256 `df4bbafb808f6d55dbcb983904193a54a90825153a355e74717fff9a30161630` |
| ARM musl | Static musl 1.2.5 prefix with headers, crt objects and libc.a, built for ARMv7 soft-float with that cross compiler |
| Linux UAPI headers | Headers usable by musl for `linux/loop.h`; reference host used Linux headers 6.18.7. These are userspace build headers, not a replacement device kernel. |

Hashes above pin the previously inspected inputs, not a new signature
verification or proof that a remote URL is still available. Verify downloads
against their original upstream signatures/checksums as applicable. Captured
Amazon libraries/modules and individual hardware state are never downloaded
from another user's backup.

## Reproducible host environment (Nix)

`tools/build/nix-shell.nix` pins the host tools and exports the variables that
`tools/build/toolchain.sh` validates. It does not download the private ARM
toolchain or LVGL source; fetch those as described below.

```sh
nix-shell tools/build/nix-shell.nix
```

The shell sets `LINUX_HEADERS`, `GMP_LIB`, `MPC_LIB`, `OPENSSL`, `BUSYBOX`,
`QEMU_ARM`, `QEMU_SYSTEM_ARM`, `FONT_REG`, `FONT_BOLD` and `FONT_SYMBOLS`.
After placing the private toolchain at the default `work/toolchains/...`
locations (or exporting `TOOLCHAIN`/`CC`), the following reproduce the checked-in
host artifacts:

```sh
make check
npx --yes lv_font_conv@1.5.3 --version
os/ui/fonts/generate-fonts.sh
git diff --exit-code os/ui/fonts
```

`generate-fonts.sh` rewrites the generator comment to stable placeholders
(`<dejavu>`, `<lvgl>`) so regenerated files stay byte-identical regardless of
where the fonts were installed.

## Prepare the legacy build environment

Extract the vendor source and locate its Linux kernel directory. Use the
included `os/devices/pw3/stock-kernel.config` as the reference `.config` in a **new,
private kernel output directory**. Build the configured kernel/modules there
with the exact target config until generated headers and `Module.symvers`
are present. Kernel release must be exactly `3.0.35-lab126`; the helper module
also checks the known module struct ABI. Do not install or flash this kernel.

The 3.0 kernel needs a compatible host compiler/toolchain environment. If
your old ARM compiler needs host GMP/MPFR/MPC libraries, provide their paths
in `LD_LIBRARY_PATH` explicitly. The installer does not discover private
Nix store paths or acquire an unverified toolchain automatically.

To build musl from the official 1.2.5 source in a fresh build directory:

```sh
CC=/path/to/arm-linux-gnueabi-gcc \
AR=/path/to/arm-linux-gnueabi-ar \
RANLIB=/path/to/arm-linux-gnueabi-ranlib \
CFLAGS='-Os -marm -march=armv7-a -mfloat-abi=soft' \
  /path/to/musl-1.2.5/configure --target=arm-linux-musleabi \
  --prefix=/path/to/private-musl-prefix --disable-shared
make -j4 install
```

Use a separately prepared kernel build directory; the candidate builder
compiles an external module against it and retains the output. It does not
prepare a whole legacy kernel environment or certify arbitrary compilers.
The builder also rebuilds `arcotg_udc` with the PW3 USB hotplug patch against
the supplied kernel build. The vendor source hash and patch application are
checked strictly. Keeping the controller awake after unplug fixes the observed
reconnect failure; its battery cost has not been measured.
Missing build inputs fail before device installation. A clean-room recreation
of the entire toolchain remains separate from the extracted-source candidate
build against explicitly supplied, previously prepared dependencies.

## Build base Alpine

From the extracted `kindle-native` root:

```sh
python3 tools/build/pw3-build.py \
  --rootfs /path/to/alpine-minirootfs-3.24.3-armv7.tar.gz \
  --modules /path/to/private-session/backup/stock-modules.tar \
  --p1-backup /path/to/private-session/backup/p1.ext3 \
  --return-launcher /path/to/private-control/gleworks-alpine-session.sh \
  --cross-prefix /path/to/bin/arm-linux-gnueabi- \
  --kernel-source /path/to/vendor-linux \
  --kernel-build /path/to/private-kernel-build \
  --musl-prefix /path/to/private-musl-prefix \
  --linux-headers /path/to/linux-uapi-headers/include \
  --output /path/to/new-private-candidate
```

Optional `--make`, `--hostcc`, `--qemu-arm` accept explicit tools. The builder
creates a fresh identity, compiles bootstrap/shutdown/guardian/control,
checks ARM execution with QEMU, assembles a clean base Alpine root, checks its
filesystem and records every payload hash and the base variant in candidate.json.
The root image is sized from the assembled tree (25% plus 32 MiB headroom,
rounded to 8 MiB, minimum 128 MiB) instead of a fixed 256 MiB. For reproducible
candidate images set `SOURCE_DATE_EPOCH` and `GW_BUILD_SEED`, then run
`tools/build/roundtrip.py` with the same private inputs; it builds twice and
requires identical payload and image hashes. The chosen
size is recorded as `image_mib`. It never reuses an existing overlay or token
from the author. The default base
image contains no KOReader, reader service or reader compatibility libraries.

For the optional reader image, add `--variant reader --reader /path/to/clean-koreader`
to the same command. That app layer includes compatibility libraries captured
from your own p1, per-page refresh, Vietnamese/English translations and reader
autostart. Never mix images/components from different candidate builds.

**The candidate from `pw3-build.py` is not a complete replacement OS image.**
It contains Alpine plus the optional reader layer, but not `gw-system`,
`gw-shell`, `gwctl`, `gw-appctl`, OpenRC, Wi-Fi or tailscale. Deploying it by
itself with `deploy-root` boots without the GleWorks UI/service. Build a full
image with `tools/build/pw3-os-trial.py` from a prepared root and the compiled
service/shell/agent/client binaries, or shrink the running image as documented
in the maintenance guide.

Use `--work-dir /path/to/local-work` to select working storage outside the repo.
If `--output` is omitted, a new private candidate directory is created under
`WORK/build/pw3-VARIANT-ID`. See [layout](../reference/repository-layout.md).

QEMU execution and `e2fsck` establish host/artifact properties only. Different
build timestamps can change filesystem hashes; builds are not promised to be
bit-identical. Never replace only one paired guardian component on a running
device, and never use a running mounted root image as a clean build input.
