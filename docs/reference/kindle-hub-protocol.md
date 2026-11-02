---
diataxis: reference
status: draft
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Kindle hub protocol (`kindle-hub` + `gw-agent`)

Status: **DRAFT, agreed 2026-11-02; host implementation in place, P0.5 next.**
The wire contract below is implemented by `os/system/gw-agent.c` and the
`gw-shell` Hub screen, with host tests in `tools/tests/test_hub_agent.py`.
No physical PW3 frame, touch, Tailscale login or pairing flow has been accepted
yet.

Agreed direction: the hub hosts HTTP; the Kindle is a client; Tailscale runs in
userspace only while the Hub screen is open; pairing uses a one-time PIN.

It does not change the current OS, app package format or boot path. The
existing signed `.gwapp` installer and `gw-shell` remain the only code that
may touch the panel until the gates below pass on the physical PW3.

Related: [system IPC](ipc.md), [app session contract](app-session-contract.md),
[display programming rules](display-programming-rules.md),
[device profile](device-profile-pw3.md).

## 1. Topology and trust boundary

```text
 PC / phone ──HTTPS──> kindle-hub (LXC, x86_64)
                          │ Tailscale tailnet (hub always on)
 Kindle PW3 ──────────────┘
   gw-agent  (starts tailscaled on demand)
   gw-shell  (LVGL + EPDC, sole owner of /dev/fb0)
```

- The Kindle is a **thin client**. It never runs a browser engine, a document
  converter or a TLS server.
- The hub is the only HTTP server. TLS is terminated there.
- The device **initiates** every connection; it opens no inbound port.
- Tailscale runs in userspace only while the Hub screen is open. Exiting the
  screen stops `tailscaled`; no background mirror, alerts or OTA remain.
- The hub is untrusted for display safety: every frame and event is validated
  by `gw-agent` before it reaches `gw-shell`.

### Transport modes

| Mode | Path | Use |
| --- | --- | --- |
| `tailscale` (default) | Agent starts `tailscaled --tun=userspace-networking --socks5-server=127.0.0.1:1055`; HTTP goes through SOCKS5 to the hub tailnet address | Normal use |
| `lan` (fallback) | Direct HTTP to the hub LAN address with a bearer token | First bring-up and debugging |
| `reverse-ssh` (fallback) | Kindle opens SSH to a relay; the relay forwards to the hub | If Tailscale fails the P0.5 budget |

The PW3 kernel has `CONFIG_TUN` and IPv6 disabled, so Tailscale must use
userspace networking. `lan` is plaintext; the UI must show that warning. No mode
opens an inbound port on the Kindle.

### Device configuration

`gw-agent` reads `/etc/gleworks/hub.conf` (regular file, mode `0600`), with
`/mnt/us/gleworks/hub.conf` as a fallback. A symlink is rejected.

```text
transport=tailscale
host=100.x.y.z
port=8080
socks5=127.0.0.1:1055
device_id=pw3
poll_ms=2000
token=<filled by pairing>
```

`host` is the hub's tailnet address in `tailscale` mode and its LAN address in
`lan` mode. `poll_ms` is bounded to `250..600000`. The token is never written
to logs.

## 2. Hub endpoints (agent → hub)

The agent is an HTTP client. It does not listen on a TCP port. All bodies are
bounded; the limits below are hard failures, not truncation.

### `GET /v1/frame`

Returns the next frame as `image/x-gw-raw` (8 bpp, row-major, pitch `w`).
PNG is a hub-side convenience for mirror uploads; the v1 device does not carry
a PNG decoder. A future version may add a PNG body only if a bounded decoder is
accepted on the device.

Request headers:

| Header | Meaning |
| --- | --- |
| `Authorization: Bearer <token>` | Required in every mode; Tailscale adds encryption, not authorisation |
| `X-GW-Seq` | Last sequence the device has applied; `0` on first poll |

Response headers:

| Header | Meaning |
| --- | --- |
| `X-GW-Seq` | Frame sequence, strictly increasing |
| `X-GW-Mode` | `full` in v1; `partial` is reserved |
| `X-GW-Rect` | Reserved for `partial`; omitted for `full` |
| `X-GW-Refresh` | Advisory waveform hint; v1 uses the Hub paint profile |
| `X-GW-Expires` | Seconds the frame stays valid; `0` means until replaced |
| `X-GW-Id` | Opaque content id for cache and deduplication |

`204 No Content` means no new frame. The device keeps the current frame.

### `POST /v1/events`

JSON body, at most 4 KiB:

```json
{"seq": 12, "type": "touch", "action": "down", "x": 32768, "y": 40000}
{"seq": 13, "type": "touch", "action": "move", "x": 33000, "y": 40500}
{"seq": 14, "type": "touch", "action": "up",   "x": 33000, "y": 40500}
{"seq": 15, "type": "key",   "key": "back"}
```

- `x` and `y` are normalized to `0..65535` over the panel, so the hub can map
  them to any viewport without knowing the device geometry.
- `key` is one of `back`, `home`, `page_next`, `page_prev`, `menu`.
- The hub replies `{"ok": true, "ack": 15}`. Unknown fields are rejected.
- Events are best-effort. The hub must tolerate duplicates and gaps.

### `POST /v1/status`

JSON body, at most 2 KiB:

```json
{
  "device_id": "pw3",
  "uptime_s": 3600,
  "agent": "0.1.0",
  "screen": {"w": 1072, "h": 1448, "bpp": 8}
}
```

v1 keeps the device report minimal. Battery, Wi-Fi and storage are already
available to the hub through the shell/service status path and are not
duplicated here; a later revision may add them as optional fields.

Reply may include `{"poll_s": 30}` to set the next poll interval. The hub must
not command arbitrary shell execution through this endpoint.

### `POST /v1/pair`

One-time pairing. Body at most 512 bytes:

```json
{"pin": "123456", "device_id": "pw3", "agent": "0.1.0"}
```

The hub validates a six-digit, single-use PIN with a five-minute expiry and
rate-limited attempts, then returns:

```json
{"token": "<48 hex characters>", "hub_id": "kindle-hub"}
```

The device stores the token `0600`. A wrong, expired or reused PIN is rejected.
Pairing runs over Tailscale or a trusted LAN, never over plaintext public
Internet.

### `POST /v1/mirror`

Accepts `image/png` or `image/jpeg`, at most 8 MiB, and renders it through the
same frame path. Used by the PC mirror script. The hub, not the device, does
resize, grayscale conversion and dithering.

### `GET /v1/screenshot`

Returns the current panel content as `image/png`. Authenticated in every mode;
screen content is private.

## 3. Display rules

- Panel: `1072 × 1448`, 8 bpp Y8, pitch `1088` bytes.
- The hub sends full frames at panel resolution in v1. It may letterbox, but
  must not assume a different geometry. `partial` and dirty rectangles are
  reserved until the device-side apply path is accepted.
- v1 applies the Hub paint profile (`hub-full`, no scroll preview) and does not
  yet consume `X-GW-Refresh`. The hint is reserved for a later revision.
- `gw-agent` may downgrade a request but must never upgrade `full` to a
  partial waveform.
- Frames are applied through `gw-shell`. The agent does not write `/dev/fb0`
  directly.
- Mirror frame rate is capped at one frame per 2 seconds. There is no video
  streaming.

### Shell link

The agent and `gw-shell` use `/run/gw-hub.sock` (mode 0600, one client,
newline-terminated messages bounded by `GW_HUB_LINE_MAX`):

- agent → shell: `FRAME <seq> <w> <h> <mode> <rect> <path>`, `CLEAR`,
  `STATUS <text>`, `LOGIN <url>`
- shell → agent: `TOUCH <down|move|up> <x> <y>`, `KEY <name>`,
  `VIEW <active|idle>`

The shell validates the frame path prefix and exact size before reading it.
`VIEW idle` stops frame fetches so an unattended device can sleep; `VIEW active`
requests a frame immediately.

### Tailscale lifecycle

On `VIEW active`, the agent starts:

```text
tailscaled --tun=userspace-networking --socks5-server=127.0.0.1:1055
```

It waits for the SOCKS5 port and logs in if needed. `tailscale up` prints a
login URL; the agent sends `LOGIN <url>` to the shell, which shows the URL and a
QR code. On `VIEW idle`, the agent stops `tailscaled` and closes the tunnel. No
background process or listener remains.

## 4. Authentication and limits

| Item | Rule |
| --- | --- |
| Token | 24 random bytes, hex; issued by `/v1/pair`; stored `0600`; constant-time compare |
| Pairing | six-digit PIN, single use, five-minute expiry, rate-limited |
| Tailscale | userspace networking; hub always on the tailnet; device on demand only |
| Body limit | 4 KiB events/status, 8 MiB mirror, 512 B pair |
| Frame limit | exactly `w*h` raw bytes; v1 has no PNG decoder |
| Paths | no filesystem paths in v1 |
| Replay | `seq` monotonic per device; duplicates acknowledged, not re-applied |
| Logging | never log tokens, PINs, keys or screen content |

The hub keeps one browser context per device, a memory cap per context, and a
hard timeout for page loads. A crashed renderer must not take down the hub.

## 5. Offline behaviour

- If the hub is unreachable, the agent keeps the last frame and shows a small
  status strip (`hub unreachable`) after a timeout.
- The last frame is cached on the device across agent restarts, but not across
  a reboot unless the cache write is proven safe.
- The agent retries with bounded exponential backoff and jitter while the Hub
  screen is open.
- When the Hub screen is closed, the agent stops `tailscaled` and stops
  retrying. There is no background queue, mirror or alert.
- The hub records `last_seen` and battery; it must not queue unbounded frames
  for an offline device.

## 6. Hub services (LXC)

Recommended baseline: unprivileged Debian 13 LXC on Proxmox, 4 GiB RAM,
2 vCPU, 16 GiB disk.

| Unit | Role |
| --- | --- |
| `caddy` | TLS, reverse proxy, per-device auth at the edge |
| `kindle-hub` | FastAPI service: device registry, frame/event/status, mirror, screenshot |
| `kindle-render` | Playwright Chromium worker, one context per device, grayscale + dither + diff |
| `calibre` | EPUB/PDF conversion and OPDS catalog |
| `freshrss` / `wallabag` | Feed and read-later sources for the daily digest |
| `ntfy` | Alerts to the panel |
| `tailscale` | Hub always on the tailnet; the device starts its userspace client on demand |

The device never talks to Calibre, RSS or Home Assistant directly. The hub
composes one frame per screen.

## 7. Implementation status

| Part | State |
| --- | --- |
| `gw-agent` HTTP client, config, frame write, status, touch/key forwarding | implemented; direct HTTP only |
| Tailscale userspace start/stop, SOCKS5, login URL/QR | **not implemented (P0.5/P1)** |
| One-time PIN pairing and token storage | **not implemented (P1)** |
| `gw-hub.sock` line protocol | implemented in `os/ui/gw_hub.c` |
| Hub screen and Settings entry | implemented; host screenshot and geometry checks pass |
| Build/image integration (`gw-agent`, OpenRC unit, runlevel) | implemented in the build scripts; no device deployment yet |
| Physical PW3 frame/touch acceptance | **pending** |

## 8. Acceptance gates

- **HUB-01** host tests: frame header parse, raw frame bounds, event JSON
  validation, token compare, body limits, sequence handling.
- **HUB-02** physical PW3 shows a hub-rendered page and reports `ack`.
- **HUB-03** a touch scroll changes the page and produces one partial update.
- **HUB-04** hub down: cached frame remains, status strip appears, no crash.
- **HUB-05** no inbound listener on `wlan0`; userspace SOCKS5 binds loopback only.
- **HUB-06** PC mirror through the hub reaches the panel with correct geometry.
- **HUB-07** a text-only change does not trigger a full refresh.
- **HUB-08** one hour of use shows bounded memory and no fd leak.
- **HUB-09** `make check` passes and docs remain consistent.
- **HUB-10** leaving the Hub screen stops `tailscaled`; no process or listener
  remains.
- **HUB-11** a wrong, expired or reused PIN is rejected; the token is `0600`.
- **HUB-12** Hub tap to frame is within the agreed budget (target: ≤ 15 s first
  login, ≤ 12 s subsequent starts).
- **HUB-13** one hour with the Hub closed is at baseline battery and CPU.

No gate may be closed from host output alone; HUB-02 through HUB-13 require the
physical PW3 and a recorded boot ID.

## 9. Non-goals

- On-device browser engine, JavaScript or WebKit.
- Always-on Tailscale, background mirror, background alerts or background OTA.
- Kindle-hosted HTTP server.
- Video or continuous mirroring.
- Input injection into arbitrary apps.
- Multi-user accounts on the device.
- Replacing the signed `.gwapp` installer or the current boot path.
