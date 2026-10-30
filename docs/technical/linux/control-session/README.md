# Temporary Kindle control session

This is a transition tool, not an Alpine boot or an OS installer. It leaves
Upstart as PID1, the installed bootloader and both stored kernels untouched.
No rootfs remount, boot selector, partition write or permanent service is used.

`prepare.py HOST_IPV4` packages the already installed KOReader Dropbear/libz,
separate temporary client/server keys and the pinned Alpine3.24.3 BusyBox/musl.
The client private key remains in host `.private/`; it is never copied to Kindle.
Keep `bundle/` private: it contains the temporary SSH server's private key.

Copy the launcher to `/mnt/us/documents/gleworks-alpine-session.sh`, and its
nine inputs plus `SHA256SUMS` into `/mnt/us/documents/gleworks-alpine-session/`.
Copy the independently reviewed `../alpine-stage/bundle/` inputs to documents
as well. Existing proven native-stage files are preserved.

After safe USB eject, launch **GleWorks Alpine Session** once. The launcher
waits for the worker to close captures and sync before returning. It verifies the manifest and copies all control files to a fresh root-only tmpfs
directory before starting its worker. The worker:

1. Captures init/Upstart and selected stock loader/library evidence. No keys,
   Wi-Fi credentials or Amazon account data are requested.
2. Runs a minimal Alpine BusyBox/musl chroot **only as a stock-kernel ABI check**.
   This is not PID1 replacement or independent boot.
3. Runs the separate returning Alpine-image staging suite if installed; it
   has no execute command. A failure is logged without automatic retry.
4. Enables saved Wi-Fi and starts SSH bound to the Kindle's IPv4 address on
   port2222. The firewall permits only the configured host, and password login
   and forwarding are disabled. The listener expires after approximately one
   hour; cleanup stops its own listener and descendants using process identity
   and start-time checks. Each connection also has a one-hour duration limit.

The worker closes its evidence files before control handoff and does not reopen
`/mnt/us` afterward, so it does not require the USB filesystem to remain mounted
by Kindle. Evidence is `gleworks-alpine-session-*/` on user storage. The worker
and server logs remain in tmpfs until reboot. Normal cleanup stops this server and its descendants
and removes only its own firewall rules; reboot also clears these RAM changes.

The host observer does not scan the LAN. If stock curl supports Telnet, Kindle
connects to the host's existing SSH listener on2222 and reads its banner. This
does not authenticate to the host. `observe.py --seconds 600` checks only peers
seen connecting to that listener, pins the pre-generated Kindle server key,
then requires a fresh nonce, root UID, stock kernel and stock PID1. A successful
observation saves SSH arguments in host `.private/session-ready.json` for the
agent to continue. Unsupported curl, Wi-Fi isolation, changed host IP or a
different host SSH port can prevent discovery; the device `CONTROL.txt` retains
the endpoint for explicit connection. No new host firewall port is opened.

Host verification: shell/Python syntax and bundle digests; Dropbear host-key and
client-key interoperability with a temporary host-native server, including
missing-key rejection; SSH banner/`ss` parsing. These checks do not prove the
Kindle ARM binary, Wi-Fi, firewall, cleanup or Alpine diagnostic has run.


## First hardware result — 2026-10-30 01:05

The captured Alpine3.24.3 BusyBox/musl runs under stock Linux3.0.35. Upstart0.6.6
is still PID1. The prepare-only Alpine-image suite passes78 checks with normal
module cleanup. This is not native Alpine boot.

Dropbear starts and the worker reports192.168.1.200:2222. The host's connection
fails before authentication with `No route to host`; its neighbor entry is
FAILED. Association, current route and peer reachability were not captured, so
USB-mode Wi-Fi behavior, AP isolation and a stale interface address are possible
causes, not established diagnoses. Do not report SSH working from daemon startup.

`observe.py --address 192.168.1.200 --seconds 300` retries only the exact endpoint
from that verified capture with the same pinned key and nonce checks. This does
not scan other peers. If USB storage mode disconnects Wi-Fi, unplugging USB and
keeping Kindle awake/on Wi-Fi can allow the existing one-hour session to work.
No need to repeat the passed stage/ABI suite merely to reconnect SSH.


## Verified Wi-Fi control and RAM PID1 experiment — 2026-10-30

Unplugging USB restored pinned-key root SSH; relation to USB mode is observed,
not a proven root cause. A separate RAM-only experiment then replaced global
PID1 with Alpine3.24.3 BusyBox on stock3.0.35, retained old processes for control,
and reset after the90s guardian interval. Fresh authenticated control after
physical relaunch verifies changed bootID, restored Upstart and cleared RAM
trial. This launcher remains a temporary control tool; it does not itself boot
Alpine. See ../pid1-handoff/pid1-proof.json. SSH does not autostart after reboot.
