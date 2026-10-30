#!/usr/bin/env python3
"""Check generated inputs and package a load-only readiness snapshot."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess

base = Path(__file__).resolve().parent
linux = base.parent
llvm = "/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/"
config = (base / "build/.config").read_text()
for option in ["MMC", "MTD", "WATCHDOG", "NET", "ARM_ATAG_DTB_COMPAT"]:
    assert "# CONFIG_" + option + " is not set" in config
for option in ["USB_G_SERIAL", "BLK_DEV_INITRD", "DEVTMPFS", "BINFMT_ELF", "CMDLINE_FORCE"]:
    assert "CONFIG_" + option + "=y" in config
archive = (base / "build/usr/initramfs_data.cpio").read_bytes()
at = 0
files = {}
while archive[at:at+6] == b"070701":
    fields = [int(archive[at+6+i*8:at+14+i*8], 16) for i in range(13)]
    size, namesize = fields[6], fields[11]
    name = archive[at+110:at+110+namesize-1].decode()
    start = (at + 110 + namesize + 3) & ~3
    files[name] = (fields, archive[start:start+size])
    at = (start + size + 3) & ~3
    if name == "TRAILER!!!":
        break
assert files["init"][1] == (base / "init").read_bytes()
assert files["init"][0][1] == 0o100755
assert files["init"][0][2:4] == [0, 0]
assert files["dev/console"][0][9:11] == [5, 1]
assert all(name in files for name in ["dev", "proc", "sys"])
for name in ["init", "gleworks-watchdog-read", "build/vmlinux", "build/arch/arm/boot/compressed/vmlinux"]:
    data = (base / name).read_bytes()
    assert data[:6] == b"\x7fELF\x01\x01" and struct.unpack_from("<H", data, 18)[0] == 40
    off = struct.unpack_from("<I", data, 28)[0]
    stride, count = struct.unpack_from("<HH", data, 42)
    assert all(struct.unpack_from("<I", data, off+i*stride)[0] not in [2, 3] for i in range(count))
    if name in ["init", "gleworks-watchdog-read"]:
        assert not subprocess.check_output([llvm + "llvm-nm", "-u", str(base / name)], text=True).strip()
kernel = (base / "build/arch/arm/boot/zImage").read_bytes()
dtb = (base / "imx6sl-pw3-probe.dtb").read_bytes()
magic, start, end = struct.unpack_from("<III", kernel, 36)
assert magic == 0x016f2818 and end-start == len(kernel)
assert struct.unpack_from(">II", dtb) == (0xd00dfeed, len(dtb))
image = kernel + dtb
assert 1048576 < len(image) < 2097152
(base / "native-zImage-dtb.bin").write_bytes(image)
artifacts = {
    "gleworks_kexec_load.ko": (linux / "kexec-load-probe/gleworks_kexec_load.ko").read_bytes(),
    "gleworks-load-helper": (linux / "kexec-load-probe/gleworks-load-helper").read_bytes(),
    "gleworks-native-image.bin": image,
    "gleworks-watchdog-read": (base / "gleworks-watchdog-read").read_bytes(),
}
hashes = {name: hashlib.sha256(data).hexdigest() for name, data in artifacts.items()}
prior = json.loads((linux / "kexec-load-probe/host-review.json").read_text())
for name in ["gleworks_kexec_load.ko", "gleworks-load-helper"]:
    assert hashes[name] == prior["artifacts_sha256"][name]
script = (base / "readiness-trial.in.sh").read_text()
for token, name in [("MODULE", "gleworks_kexec_load.ko"), ("HELPER", "gleworks-load-helper"),
                    ("IMAGE", "gleworks-native-image.bin"), ("READER", "gleworks-watchdog-read")]:
    script = script.replace("@" + token + "_HASH@", hashes[name])
assert "@" not in script
artifacts["gleworks-boot-readiness.sh"] = script.encode()
bundle = base / "readiness-bundle"
bundle.mkdir(exist_ok=True)
for name, data in artifacts.items():
    (bundle / name).write_bytes(data)
(bundle / "gleworks-native-readiness.sha256").write_text("".join(
    hashlib.sha256(data).hexdigest() + "  " + name + "\n" for name, data in artifacts.items()))
checksums = {}
for size in [4096, 65536, 262144, 1048576, len(image)]:
    padded = image[:size] + b"\0" * ((-size) % 4096)
    checksum = 2166136261
    for byte in padded:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    checksums[str(size)] = f"{checksum:08x}"
result = {"kernel_version": "6.18", "kernel_bytes": len(kernel), "dtb_bytes": len(dtb),
          "combined_bytes": len(image), "combined_sha256": hashes["gleworks-native-image.bin"],
          "init_bytes": len(files["init"][1]), "init_in_generated_archive": True,
          "no_interpreter_or_dynamic_segment": True, "storage_drivers_disabled": True,
          "watchdog_driver_disabled": True, "expected_padded_fnv1a": checksums,
          "boot_attempted": False, "boot_confirmed": False,
          "watchdog_recovery_proven": False, "hardware_readback_pending": True}
(base / "host-review.json").write_text(json.dumps(result, indent=2) + "\n")
(base / "readiness-artifacts.json").write_text(json.dumps({name: {
    "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    for name, data in artifacts.items()}, indent=2) + "\n")
print("Embedded PID1, static ARM inputs and load-only readiness bundle checked/packaged.")
