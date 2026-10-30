# Alpine native using the installed PW3 kernel

Accepted scope: user wants only Alpine native, retaining bootloader and exact
stock kernel. Alpine global PID1/root replaces Amazon userspace; no chroot-only
completion. Reader, Tailscale and Web UI are deferred. User previously opted out
of repeated questions/approvals; this records the accepted direction.

Approaches: keep stock kernel and replace p1 userspace (selected: working exact
drivers and demonstrated live AlpinePID1); new kernel+initramfs (deferred: DMA,
board support and recovery not qualified); live takeover (proven experiment,
not persistent delivery). A separate full Alpine ext3 root image is the next
host artifact; do not alter the known-good diagnostic/MULTI image or flash it.

Initial root image: pinned Alpine3.24.3 armv7 minirootfs, direct BusyBox init,
full minirootfs tools including apk, exact captured stock modules, readonly
root, RAM dev/proc/sys/run/tmp, USB nonce proof, qualified stock-kernel90s
recovery guardian. This is a short cold-start candidate, not a production OS.
It deliberately resets after90s and supplies no unauthenticated shell.

Install acceptance: direct cold startup to AlpinePID1 without old Amazon
processes; fresh proof through USB; independent USB restoration after failed
Alpine startup; installed bootloader/stored kernel unchanged. Only then remove
Amazon userspace. Host image validity and live takeover are not coldboot proof.

Current recovery blocker: p2 is actively mounted at /usr/share/nh and holds
Nh_* data, not a demonstrated diagnostics OS. Stock kernel has both
CONFIG_MXC_REBOOT_MFGMODE and CONFIG_MXC_REBOOT_ANDROID_CMD disabled. No enabled
nonpersistent reboot-to-fastboot route. A written bootmode would persist and
has no proven independent return path. No selector/partition/init mutation
until cable rescue is qualified; user agreement does not remove no-brick limit.
