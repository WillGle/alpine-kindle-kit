# GleWorks Kindle — Alpine native

Trạng thái hiện hành và điểm bắt đầu session: [CURRENT.vi.md](../guides/CURRENT.vi.md).
Máy đã chuyển sang Alpine native + KOReader, với lớp OS riêng (`gw-system`,
`gw-shell`, Home/Settings/Applications/App updates) và signed app manager. Nội
dung appliance bên dưới là lịch sử hệ Amazon/JB; không dùng hướng dẫn cài bên
dưới trên máy native.

## Repository hardening (2026-11-01)

Kế hoạch xử lý các phát hiện từ đợt review toàn repo: trạng thái git, bảo mật
installer/signature, quyền của `gw-system`, lỗi UI, an toàn tooling, build tái
lập và CI:

- [2026-11-01-repository-hardening.md](superpowers/plans/2026-11-01-repository-hardening.md)
  — **approved**, chưa triển khai phase nào. Có bảng truy vết finding → phase.

## PW3 UI refresh (2026-11-01)

Kế hoạch và bằng chứng cho đợt sửa flash/scroll + UI remediation:

- [PW3-UI-REFRESH-UX-PLAN-2026-11-01.md](PW3-UI-REFRESH-UX-PLAN-2026-11-01.md)
  — **implemented + device-verified**, không còn checkbox mở.
- [PW3-DISPLAY-PROGRAMMING-RULES.md](PW3-DISPLAY-PROGRAMMING-RULES.md)
  — **bộ quy tắc bắt buộc** cho lập trình hiển thị trên PW3.
- [evidence/pw3-ui-refresh-2026-11-01](../evidence/pw3-ui-refresh-2026-11-01/README.md)
  — log 9 hành động, 20 tap, lock/unlock và ảnh framebuffer (build `149ae024…`).
- [evidence/pw3-wifi-scroll-fix-2026-11-01](../evidence/pw3-wifi-scroll-fix-2026-11-01.md)
  — Wi-Fi list hex/dBm, scroll union + idle GC16, hash build cuối.

Kết quả trên PW3 (bản cuối `df2c9bd7…`, 2026-11-01): 20 tap = **0 GC16 / 0 full
update**; scroll = **6 preview/160 ms** khi kéo, sau đó một full GC16 khi dừng
cuộn 1.2 giây (throttle 3 giây) để dọn ghost; lock = một GC16 full duy nhất
(điểm dọn ghost U3). Home đầu tiên sau boot cũng full GC16 một lần để xoá bóng
ảnh boot. Wi-Fi Scan dùng `gw-system` `aeedb481…` (đủ signal/secure, tên + dBm
đúng). Các mốc cũ: `149ae024…` (UI refresh), rollback `6020c6fe…`.

## Historical Amazon/JB appliance

Controlled app layer for the inspected Paperwhite 3 (7th generation), ARMv7,
firmware 5.16.2.1.1, Linux 3.0.35. Retains Amazon's kernel, hardware drivers,
power management, KMC, and jailbreak.

**Reader, Terminal and Calendar opening have user acceptance.**
The current app-lifecycle repair is locally verified and awaits a user-run USB
update. VPN connectivity, real transfers and physical Home/redraw behavior
remain pending. See [APP-AUDIT.md](APP-AUDIT.md) for findings and probe limits.

Notifications now use a fixed framed GleWorks INFO/ERROR popup. Managed failures
are journaled in `gleworks-logs/errors.log` on USB, with bounded rotation;
**GleWorks System Inspection** automatically includes recent safe events.
See [NOTIFICATIONS.md](NOTIFICATIONS.md) for layout, collection and verification.

## Install this integrated revision on the existing appliance

Close KOReader, stop Transfer/VPN, connect USB, then run in your normal host
terminal (this agent session cannot write to the mounted Kindle):

```sh
python3 /home/will/dev/kindle/ours/scripts/update-appliance.py
```

This installs all controlled scripts, launchers and the GleWorks KOReader plugin
in one pass, preserving verified upstream binaries, personal settings and data.
It recognizes the already installed reader/VPN patch, backs up changed files,
rejects unrecorded edits and reads every changed file back. Safely eject USB.
Do not rerun the original migration/install scriptlets over the appliance.

Check **Reader**, **Calendar and Todo** (create/edit/complete, exit and reopen),
**Terminal** (`pwd`, `uname -a`, then `exit`), **Update Apps** (reports no staged
package), **VPN/Login**, and **Transfer**. Open **Status** last. Stop services,
exit KOReader and reconnect; `documents/GleWorks-status.txt` is the shared report.
After this repair, standalone feedback returns to Home. Transfer Key stays
visible for 90 seconds; Status/errors for 30 seconds; enrollment links for up to
three minutes. These timers are intentional and do not finish immediately when
you authorize login on another device. Status includes safe categories for the
last VPN attempt; raw runtime logs remain private on the Kindle.

The terminal is a local root shell: commands can modify the Kindle; no remote
shell listener is installed.

Calendar and tasks are offline, with no accounts, sync, wake alarms or polling.
Data lives in `gleworks/data/planner.sqlite3`; back it up while KOReader is
closed. Each table supports 2,000 records and displays at most 500 in a list.
The native GleWorks menu also provides these tools while using KOReader.

## Complete Update Apps bundles

No update server exists on `gleworks.io.vn` yet. **Update Apps currently applies
a complete package copied over USB**, updating KOReader, Tailscale and every
managed GleWorks app together. It does not fetch new versions over Wi-Fi.
After installing the integrated revision above, prepare the next official
release into a **fresh** path and stage it:

```sh
python3 /home/will/dev/kindle/ours/scripts/prepare.py --output /home/will/dev/kindle/ours/bundle-next && \
python3 /home/will/dev/kindle/ours/scripts/stage.py --update --bundle /home/will/dev/kindle/ours/bundle-next
```

Use a new output path for each release. Eject USB, then open **GleWorks Update
Apps** once, or select Update Apps in the reader's GleWorks menu and confirm
normal reader exit. The installed validator checks the complete tree and hashes
before staged code executes. USB bundles are trusted local input: checksums
catch corruption, not a malicious person replacing both files and manifest.

Updates stop network services and retain previous applications, preferences,
planner data and uploaded files. **GleWorks Restore Apps** returns to the retained
version, including when activation was interrupted. Handled activation failures
attempt immediate recovery; original app folders and a last-update record remain
available. VFAT/power loss is not a transactional guarantee: if recovery reports
an error, keep all retained directories and inspect the USB log before retrying.
Exit the reader before update/restore. Keep USB disconnected and the device
powered during activation. Future online updates need a separately deployed,
authenticated release channel; no DNS or hosting changes were made here.

## Apps

| App | Behavior |
| --- | --- |
| GleWorks Reader | Official KOReader, normal upstream launcher, configurable in its menus |
| GleWorks Transfer | Toggle our authenticated, loopback-only HTTP file server |
| GleWorks Transfer Key | Deliberately display the private access token on the device |
| GleWorks VPN | Toggle official Tailscale in userspace mode |
| GleWorks VPN Login | Fresh interactive enrollment; show login link or connected IP |
| GleWorks Status | Service state, tailnet IPv4 if connected, and free storage |
| GleWorks Calendar and Todo | Native offline month view, events, dated tasks and completion |
| GleWorks Terminal | Bundled KOReader PTY terminal and virtual keyboard |
| GleWorks Update Apps | Apply one complete verified USB-staged app bundle |
| GleWorks Restore Apps | Restore the previous complete app revision, retaining personal data |
| GleWorks Rollback | Restore quarantined apps and their package records; leave services stopped |

The balanced reading profile uses a full refresh every four pages after this update, animations
off, and double tap disabled. Unrelated data preferences are retained. Original
settings are backed up; unreviewed plugins and executable default overrides are
not imported. See [READER-PERFORMANCE.md](READER-PERFORMANCE.md).

The server uses KOReader's LuaJIT, LuaSocket and LuaFileSystem. It blocks while
idle and processes one request at a time, using 32 KiB streaming buffers.
There is no dashboard polling, thumbnail generation, database, shell endpoint,
app store, telemetry, or automatic download in our service. The Go source/build
is a historical host prototype, excluded from every device bundle: its runtime
does not support the Kindle's kernel.

## Prepare and install

Keep the existing verified USB backup. It is not a full system recovery image.
Connect USB, close KOReader and the package manager, then run in your **normal
host terminal**, where the Kindle mount is writable:

```sh
python3 /home/will/dev/kindle/ours/scripts/prepare.py && \
python3 /home/will/dev/kindle/ours/scripts/stage.py
```

The second command runs only if preparation succeeds. The downloader uses the
system CA bundle when standalone Python has no default CA file, while honoring
explicit certificate settings and retaining HTTPS certificate/hostname checks.

Preparation selects current official stable releases once, records their exact
versions, URLs and SHA256 in `bundle/gleworks/UPSTREAM.json`, and verifies
Tailscale's published checksum plus GitHub's KOReader release-asset digest.
It selects only the **kindlepw2** reader build for this PW3, prefers the official
ZIP package (also supports tar.gz), and checks the ARM
ELF headers. To reproduce a chosen release, use `--tailscale-version`,
`--koreader-tag` and `--koreader-asset`; see `prepare.py --help`. If GitHub has no
published digest, preparation stops; supply `--koreader-sha256` only after
checking an official published checksum independently. It never substitutes a
self-generated checksum as upstream verification. A failed preparation removes
its work directory and never produces an installable bundle.

Official ZIPs include an `extensions/` directory for KUAL integration. We leave
that directory out because this appliance supplies its own library launchers.

Staging verifies hashes, copies to `gleworks-stage`, reads back every file, and
copies the install/rollback entrypoints into the library. If interrupted, rerun
only `stage.py`: it resumes known partial files and skips files whose checksums
already match. It refuses unrelated files, symlinks, conflicting entrypoints,
or a completed stage that differs from the bundle. Safely eject USB using the
desktop, then open **GleWorks Install** from the Kindle library. Do not reconnect
USB during installation or while reader/network services use user storage.

The installer verifies the bundle again, checks device/runtime/package ownership,
keeps the complete previous reader, and preflights the new tools before retiring
the old layer. If anything fails, read the generated `gleworks-install-*.log`
and use rollback before retrying. Do not remove an interrupted migration lock
without reviewing what completed; a reboot clears locks but does not undo file
changes.

## First use and acceptance

Reader was reported working with good responsiveness. For the requested small
reduction in ghosting and to repair transient VPN/Status feedback, run this once
from the writable host terminal with USB connected and KOReader closed:

```sh
python3 /home/will/dev/kindle/ours/scripts/update-reader-vpn.py
```

This verifies existing appliance files against the original bundle, backs up
changed files, sets day/night full refresh to four pages, and reads back changes.
It preserves other preferences and saves the original settings as
`settings.reader.lua.gleworks-before-refresh4`. The patch can be rerun safely;
unexpected user edits stop it for review. Do not rerun the original installer.
Safely eject, then open Status. Feedback remains visible for ten seconds;
Status also saves `documents/GleWorks-status.txt` without enrollment URLs,
tokens, accounts or peer information. VPN status distinguishes a running daemon
from an authenticated connection, including `NeedsLogin`.

1. Open **GleWorks Reader**. Check page turns, menu response, text quality,
   normal exit to Amazon's library, sleep and wake. Books remain in their
   existing locations; new transfers appear in `documents/GleWorks`.
2. Open **GleWorks VPN**, then **GleWorks VPN Login**. Follow the login URL on
   your own phone/computer. VPN Login keeps the link visible for three minutes;
   other messages retain the shorter display time. Enrollment creates new private state; no old key or
   Tailscale state is imported. Reopen Status after authorization to get the IP.
3. Restrict the Kindle's tailnet policy to your own client devices and TCP8080.
   Existing broad allow rules can override a restrictive addition: inspect the
   effective policy and verify a disallowed client cannot connect. Do not enable
   Funnel, Tailscale SSH, public ingress, or subnet/exit-node routes.
4. Open **GleWorks Transfer**, then **GleWorks Transfer Key**. On an allowed
   tailnet client open `http://TAILSCALE-IP:8080`, enter the displayed token, and
   upload/download a small book. Compare size and SHA256 with the original.
   Also check wrong-token rejection and an existing-name upload refusal.
5. Stop Transfer and VPN with their toggles. Check sleep/wake again. Stop both
   and exit KOReader **before reconnecting USB**. Verify books and configuration
   persist across a normal reboot; services deliberately do not auto-start.

Tailscale's current official runtime may exceed upstream support for this old
kernel. Version/startup checks are necessary but do not establish stable VPN
operation; actual enrollment, connectivity and sleep/wake remain required. A
failure aborts migration; do not bypass checks or flash a kernel to force it.

## Network and storage boundaries

The service binds `127.0.0.1:8080`. Tailscale userspace forwarding provides private
inbound access on the same port; verify that behavior on this device. Loopback
binding does not hide other localhost services from tailnet peers, hence the
port-limited tailnet policy above. No Wi-Fi firewall opening is added.

The public landing page contains no credentials. All file operations require a
random 256-bit bearer token. The UI keeps it only in memory, never URLs, browser
persistent storage, or ordinary logs. Cross-origin requests are rejected. Use
HTTP only over the encrypted tailnet, or an optional private HTTPS proxy you
control. Root compromise can expose all credentials; this service cannot make a
compromised firmware trustworthy.

| API | Directory on the Kindle |
| --- | --- |
| `/api/books` | `/mnt/us/documents/GleWorks` |
| `/api/files` | `/mnt/us/gleworks/files` |

GET on a collection lists files. GET on `/api/books/NAME` or `/api/files/NAME`
downloads; PUT uploads one raw body with Content-Length. Names are a single
UTF-8 component. Hidden/control/FAT-invalid/reserved names and symlink access
are refused. Linux `openat` uses target-specific no-follow flags. Listings stop
at 2,000 entries. Requests/headers have size/time bounds. Uploads are limited to
128 MiB with a 64 MiB storage reserve and never overwrite an existing path.

Transfers are serial: another request waits while a transfer runs. A single
large download/upload can occupy the service for up to five minutes; this is a
small personal transfer tool, not a multi-client server. Interrupted requests
remove partial uploads when the process can handle the error. **Power loss,
forced termination, or stopping Transfer during an upload can leave a partial
file.** Compare it and remove it manually via USB before retrying. Concurrent
writers can still fill storage. Uploaded content is not scanned for malware.

Token and VPN state live under root-private `/var/local/gleworks`, outside USB.
VPN binaries live on USB because `/var/local` has little free space. There are
no boot hooks or automatic keep-awake changes; wake the device for transfers.
Long transfers may be interrupted by normal sleep. Tailscale diagnostic upload
is disabled using its `TS_NO_LOGS_NO_SUPPORT` environment switch; its official
control service still handles enrollment/connectivity.

## Ownership, quarantine and rollback

Old `ku-http`, `ku-tailscale`, their launchers/state and the `unlocked` repository
are retired without running their uninstall hooks. KOReader is detached from
KPM's installed records because we now manage its official release; KMC/KPM and
the KindleModding repository stay intact. Do not reinstall KOReader through KPM
while this appliance owns `/mnt/us/koreader`.

The full originals are retained in `gleworks-quarantine-TIMESTAMP-PID` on USB.
Its path is recorded in `gleworks-last-migration.txt`; its SQLite snapshot is
consistent, and rollback selectively restores affected records, preserving
unrelated database changes. Conflicts stop automatic restoration for review.
Rollback does not restart old network services or reopen the old Wi-Fi port.
It retains new private credentials for deliberate cleanup.

**Quarantine contains old VPN credentials and is readable over USB.** Protect
the device/backup, revoke the old Tailscale node after successful acceptance,
and remove the quarantine only after checking reader, VPN, transfers, sleep,
and reboot behavior. Keep the verified off-device backup until recovery is
settled. Removing the app layer is not proof of a clean firmware baseline.

## Domain

`kindle.gleworks.io.vn` is the intended optional name. After fresh enrollment,
map it to the new verified tailnet IP in DNS you control. It remains reachable
only from the tailnet. IP access works first; no DNS, certificates, account
policy, or hosting were changed by this implementation. A private HTTPS proxy
and DNS-validated certificate can be added separately. Self-hosting Tailscale's
control plane is outside this app setup.

See [APPLIANCE-PLAN.md](../BACKLOG.md#history-002), [MIGRATION.md](MIGRATION.md), and
[VERIFICATION.md](../BACKLOG.md#history-005) for progress, safety boundaries and evidence.
