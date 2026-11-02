---
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Kindle hub roadmap

Status: **agreed 2026-11-02; P0.5 is blocked on the hub LXC.** No further
device code changes until the server exposes the P0.5 endpoints. The device
foundation (`gw-agent`, Hub screen, socket contract) exists and passes host
checks, but no physical PW3 frame, touch, Tailscale login or pairing flow has
been accepted.

## Decisions

| Topic | Decision |
| --- | --- |
| Device role | Thin e-ink terminal; the LXC is the only HTTP server |
| Web view | Reader mode by default; remote Chromium on demand |
| Mirror | PC → Kindle first; Kindle → PC later |
| Transport | Tailscale userspace on the device, started only while the Hub screen is open |
| Tailscale auth | Interactive login URL, shown as URL + QR on the device |
| Pairing | One-time numeric PIN entered on the Kindle; token stored `0600` |
| Background | Nothing runs while the Hub screen is closed; no background mirror, alerts or OTA |
| Hub | Debian 13 LXC, FastAPI + Playwright, Caddy for TLS |
| Update | USB image deployment now; OTA after P5 |

## Principles

- The Kindle never opens an inbound port. It initiates every connection.
- The panel has one owner: `gw-shell` through LVGL/EPDC. The agent never writes
  `/dev/fb0`.
- On-demand means on-demand: when the Hub screen exits, `tailscaled` stops and
  the SOCKS5 listener disappears.
- Pairing and tokens are revocable from the hub.
- No feature is accepted from host output alone; HUB-02 through HUB-13 require
  the physical PW3 and a recorded boot ID.

## Phases

| Phase | Scope | Exit gate |
| --- | --- | --- |
| **P0** | Requirements, threat model, acceptance criteria | This document agreed |
| **P0.5** | Tailscale userspace spike: on-demand start/stop, SOCKS5, login URL/QR, CPU/battery/start-time measurement, reverse-SSH fallback comparison | Transport chosen from measurements |
| **P1** | PIN pairing, Tailscale lifecycle, Hub screen states, frame + touch, offline behaviour | HUB-02/03/10/11/12 |
| **P2** | Reader mode and remote Chromium, URL entry from the hub, one session per device | HUB-04/07 |
| **P3** | PC → Kindle mirror through the hub, refresh policy | HUB-06 |
| **P4** | Calibre/OPDS, RSS/Wallabag digest, KOReader sync | Real document round trip |
| **P5** | Signed `.gwapp` catalog, heartbeat, logs, remote screenshot | Update without USB |
| **P6** | Tunnel hardening, one-hour soak, rollback, evidence | HUB-08/09 |

## Server prerequisites

P0.5 cannot start until the LXC provides these. The server is the current
blocker.

| Item | Requirement |
| --- | --- |
| Host | Proxmox LXC, Debian 13, unprivileged, 4 GiB RAM, 2 vCPU, 16 GiB disk |
| Network | Hub joined to the tailnet; record its `100.x.y.z` address |
| Service | FastAPI app with `/healthz`, `/v1/frame`, `/v1/events`, `/v1/status`; `/v1/pair` for P1 |
| Test frame | Raw 8 bpp `1072 × 1448` bytes, `X-GW-Seq` and `X-GW-Mode: full` |
| Events | Echo/record `POST /v1/events` so touch can be verified |
| TLS | Caddy optional for PC/phone clients; the Kindle path can use HTTP inside Tailscale |
| Access | One test token or a one-time PIN; logs for the first connection |
| Proof | A `curl` from another tailnet node returns the test frame and status |

When the server is ready, provide the tailnet address, port, token/PIN and a
sample frame request. Do not deploy a new device image before that proof exists.

## P0.5 — transport spike

The PW3 kernel has `CONFIG_TUN` and IPv6 disabled. Tailscale must run with
`--tun=userspace-networking`; the agent reaches the hub through the userspace
SOCKS5 proxy.

Measure on the physical PW3:

- time from Hub tap to first frame, first login and subsequent starts;
- CPU while the Hub screen is open;
- battery over one hour with the Hub open and one hour with it closed;
- reconnect after suspend/resume and after Wi-Fi loss;
- whether `tailscaled` exits cleanly on Hub exit and leaves no listener;
- login URL capture and QR rendering.

Fallback if the measurements fail the battery or start-time budget: a reverse
SSH tunnel from the Kindle to a relay, with the same no-inbound-port property.

## P1 — pairing and end-to-end

1. Hub creates a six-digit, single-use PIN with a five-minute expiry and
   rate-limited attempts.
2. Kindle Hub screen offers **Pair**, a numeric keypad and a retry state.
3. `POST /v1/pair` returns a 24-byte token; the device stores it `0600`.
4. On **VIEW active**, the agent starts `tailscaled`, waits for SOCKS5, logs in
   if needed and fetches a frame.
5. On **VIEW idle**, the agent stops `tailscaled` and closes the tunnel.
6. Hub-down keeps the last frame and shows `Hub unreachable`.

Screen states: `Starting Tailscale`, `Login required` (URL + QR), `Pair`,
`Waiting for hub`, `Connected`, `Offline`.

## P2 — web view

- Reader mode: hub fetch, Readability extraction, image scaling, grayscale.
- Remote Chromium: Playwright at 1072 × 1448, dither, one context per device,
  memory cap and hard timeout.
- URL entry lives on the hub web UI; the Kindle only displays and sends touch.
- Text-only changes must not trigger a full refresh.

## P3 — mirror

- PC script captures its screen and posts to the hub, not the Kindle.
- Hub resizes, converts to 8 bpp, dithers and forwards.
- One frame per two seconds; a full refresh every fifteen frames.
- Kindle → PC screenshot and input injection are later work.

## P4–P6

- P4: Calibre conversion and OPDS, RSS/Wallabag digest, KOReader progress sync.
- P5: signed app catalog, heartbeat, logs, remote screenshot. Because Tailscale
  is on-demand, OTA checks happen when the Hub screen opens.
- P6: tunnel, auth, one-hour soak, rollback, evidence.

## Acceptance gates

- **HUB-01** host tests: frame bounds, event JSON, token compare, body limits,
  sequence handling.
- **HUB-02** physical PW3 shows a hub-rendered frame after login.
- **HUB-03** touch scroll changes the page and produces one partial update.
- **HUB-04** hub down: last frame remains, status strip appears, no crash.
- **HUB-05** no inbound listener on `wlan0`; userspace SOCKS5 binds loopback only.
- **HUB-06** PC mirror reaches the panel with correct geometry.
- **HUB-07** text-only change does not trigger a full refresh.
- **HUB-08** one hour of use shows bounded memory and no fd leak.
- **HUB-09** `make check` and the docs contract pass.
- **HUB-10** leaving the Hub screen stops `tailscaled`; no process or listener
  remains.
- **HUB-11** a wrong, expired or reused PIN is rejected; the token is `0600`.
- **HUB-12** Hub tap to frame is within the agreed budget (target: ≤ 15 s first
  login, ≤ 12 s subsequent starts).
- **HUB-13** one hour with the Hub closed is at baseline battery and CPU.

## Non-goals

- Always-on Tailscale, background mirror, background alerts or background OTA.
- Kindle-hosted HTTP server.
- Video or continuous mirroring.
- Input injection into arbitrary apps.
- On-device browser engine, WebKit or JavaScript.
- Replacing the signed `.gwapp` installer or the current boot path.
