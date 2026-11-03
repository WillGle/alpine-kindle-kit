---
diataxis: how-to
status: draft
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Connect a PW3 to a Kindle hub

Status: **agreed design; P0.5 and P1 are not implemented yet, and P0.5 is
blocked on the hub LXC.** Do not deploy a new device image until the server
checklist below passes. This page is not proof that Tailscale, pairing, the Hub
screen or the agent has run on hardware.

Contract: [`../reference/kindle-hub-protocol.md`](../reference/kindle-hub-protocol.md).
Roadmap: [`../status/ROADMAP.md`](../status/ROADMAP.md).
Host tests: `tools/tests/test_hub_agent.py`.

## 0. Server readiness checklist

The server is the current blocker. P0.5 starts only when:

- the LXC is Debian 13, unprivileged, with Tailscale joined and its tailnet
  address recorded;
- `GET /healthz` succeeds from another tailnet node;
- `GET /v1/frame` returns exactly `1072 × 1448` raw bytes with `X-GW-Seq` and
  `X-GW-Mode: full`;
- `POST /v1/events` records a touch JSON body;
- `POST /v1/status` accepts the minimal device report;
- one test token or one-time PIN is available for the first device;
- server logs are reachable for the first connection.

`/v1/pair` is required for P1, not for the P0.5 frame spike. Do not build the
device image before the frame and event checks pass.

## 1. Prepare the hub

The hub is not part of this repository. Run it on the LXC with:

- `kindle-hub` (FastAPI) for `/v1/frame`, `/v1/events`, `/v1/status`,
  `/v1/pair`, `/v1/mirror` and `/v1/screenshot`;
- Playwright Chromium for reader mode and remote pages;
- Caddy for TLS;
- Tailscale on the hub, always connected to your tailnet.

The Kindle never listens on a TCP port. It connects out to the hub.

## 2. Tailscale on the Kindle

The PW3 kernel has no TUN device, so Tailscale must use userspace networking:

```text
tailscaled --tun=userspace-networking --socks5-server=127.0.0.1:1055
```

The Hub screen starts it via `gw-tailscale on` when opened and stops it on
exit. Manual control is still available:

```sh
gw-tailscale on       # start tailscaled + loopback SOCKS5
gw-tailscale off      # stop it
gw-tailscale status   # print the PID
gw-tailscale exit-node on|off|status   # advertise as a tailnet exit node
```

On-demand is deliberate: an exit node is only reachable while the Hub screen
is open. `gw-tailscale exit-node on` writes
`/etc/gleworks/tailscale-exit-node`, restarts a running daemon, and the next
start passes `--advertise-exit-node` (plus IPv4/IPv6 forwarding sysctls).
Approve the node once in the Tailscale admin console.

`tailscaled` is **not** started at boot. The first start needs an interactive
login:

1. `tailscale up` prints a login URL.
2. The agent sends the URL to the shell.
3. The Hub screen shows the URL and a QR code.
4. Open it on a phone or PC and approve the device.
5. The login state is kept for later starts.

Do not run `tailscaled` as an always-on boot service for this design.

## 3. Pair once with a PIN

1. In the hub UI, add a device. The hub shows a six-digit PIN valid for five
   minutes and usable once.
2. On the Kindle open **Settings → Kindle Hub → Pair**.
3. Enter the PIN on the numeric keypad.
4. The agent calls `POST /v1/pair` and stores the returned token `0600`.
5. The hub lists the device; the PIN is now invalid.

A wrong, expired or reused PIN must be rejected. Pairing runs over Tailscale or
a trusted LAN, never over plaintext public Internet.

## 4. Device configuration

`/etc/gleworks/hub.conf` (regular file, mode `0600`):

```text
transport=tailscale
host=100.x.y.z
port=8080
socks5=127.0.0.1:1055
device_id=pw3
poll_ms=2000
```

`host` is the hub's tailnet address. The token is added by pairing, not typed by
hand. `/mnt/us/gleworks/hub.conf` is a fallback; a symlink is rejected.

## 5. Build the ARM binaries

```sh
env -u CC -u STRIP -u CFLAGS \
  bash tools/build/build-arm-binaries.sh
```

The output contains `gw-system`, `gwctl`, `gw-appctl`, `gw-agent` and
`gw-shell`. Record all five hashes.

## 6. Build the OS image

Pass the ARM-built agent explicitly:

```sh
python3 tools/build/pw3-os-trial.py ... \
  --service <build-dir>/gw-system \
  --shell   <build-dir>/gw-shell \
  --agent   <build-dir>/gw-agent \
  --client  <build-dir>/gwctl \
  --appctl  <build-dir>/gw-appctl
```

The image includes the agent and its OpenRC unit. The agent waits for a config
instead of restarting when the device is not paired.

## 7. Deploy with a rollback image

Follow [`install-update.md`](install-update.md). Keep the previous image:

```sh
python3 tools/maintenance/kindle-linux.py deploy-root \
  <clean-system.ext3> --sha256 <manifest-hash> \
  --backup-name system-before-hub.ext3 \
  --evidence /tmp/pw3-hub-deployment.json
```

No bootloader, kernel or partition change. Books and documents stay on
`/mnt/us`.

## 8. Verify on the device

```sh
rc-service gw-agent status
ps | grep '[g]w-agent'
ls -l /run/gw-hub.sock
```

Open **Settings → Kindle Hub**. Expected states:

1. `Starting Tailscale`;
2. `Login required` (first time only, URL + QR);
3. `Pair` (first time only);
4. `Waiting for hub`;
5. `Connected`;
6. `Offline` when the hub is unreachable.

After leaving the screen:

```sh
ps | grep '[t]ailscaled'     # must be empty
ss -ltn                      # no 0.0.0.0 listener from the agent
```

Record the boot ID, binary hashes, screenshots and timings. Do not close
`HUB-02` through `HUB-13` from host output alone.

## 9. Roll back

Deploy the preserved `system-before-hub.ext3` with a new backup name, or restore
the previous root through the USB rescue path in
[`recover-usb.md`](recover-usb.md).

## 10. Security boundaries

- The Kindle opens no inbound port.
- Tailscale is on demand; the hub is always on the tailnet.
- Pairing uses a single-use PIN; tokens are revocable from the hub.
- `lan` mode is plaintext and only for bring-up.
- The agent has no shell execution, no arbitrary network path and bounded
  bodies.
- Screenshots and mirrored content are private.

## 11. Not implemented yet

- Tailscale on-demand supervisor, SOCKS5 client and login URL/QR.
- `/v1/pair` and the PIN keypad.
- Hub service, Playwright renderer and mirror.
- Physical PW3 acceptance.
