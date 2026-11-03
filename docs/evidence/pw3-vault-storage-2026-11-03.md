---
status: current
updated: 2026-11-03
verified_build: none
supersedes: none
audience: maintainer
---

# Vault storage (2026-11-03)

Status: **storage directory, cable copy and HTTP pull are implemented.** The
HTTP pull was exercised on-device against a local HTTP server; the cable copy
uses the proven rescue path and still needs a physical run.

## Storage

```text
/mnt/us/gleworks/vault/
```

`/mnt/us` is the 3.0 GB FAT control partition; about 2.8 GB free. Files are
stored in plain form (the user chose private storage, not encryption).

## Cable

- `kindle-linux.py put-vault <files>` enters USB rescue, copies into
  `gleworks/vault`, verifies SHA-256, and resumes Alpine.
- Manual drag-and-drop: `kindle-linux.py rescue`, copy into `gleworks/vault`,
  eject, `kindle-linux.py resume`.

The runtime gadget stays `g_serial`; switching it to mass storage while Alpine
runs would drop the USB console and conflict with the console getty.

## HTTP pull

`gw-vault-pull` reads `/mnt/us/gleworks/vault.conf`:

```text
url=http://host/vault
token=optional
proxy=tailscale
```

The server publishes `index.txt` as `<sha256>  <name>` plus the files. The
puller skips files whose local hash matches, downloads new files to `/run`,
verifies hash and the 64 MiB limit, and renames them into the vault. A
five-minute loop starts at boot.

With `proxy=tailscale` the puller starts `gw-tailscale on` on demand and uses
the loopback HTTP proxy on `127.0.0.1:1056`; the daemon is stopped afterwards.

## Verification

Local HTTP pull on the device (BusyBox `nc` server on `127.0.0.1:8768`):

```text
index:  3225ec76…  secret.txt
result: pulled secret.txt, 23 bytes, content matches, hash verified
```

Tailscale proxy path: `gw-tailscale on` now listens on both
`127.0.0.1:1055` (SOCKS5) and `127.0.0.1:1056` (HTTP). A test against the
host's tailnet IP returned `502 Bad Gateway`; the host's firewall does not
expose the test port on `tailscale0`. That is a server-side firewall issue, not
a puller defect. The user's homelab must allow the port on its tailnet
interface.

## Hashes deployed

```text
gw-tailscale          b1d2ddb1bb6a5d619b91ddc8d09555e62f50beedaa563d2d2d6aaa88014228a4
gw-vault-pull         f5db7829bb977ff62d3ffe6a235fc5870edda197322e258129acb1e58ea72537
gleworks-early.sh     80f5d0029369c4c8ba14b3b90c56a2c0e55d578a3164407f43cc5387e659b71a
```

## Rollback

Remove `gw-vault-pull` and the vault loop block from `gleworks-early.sh`.
`put-vault` is a host-side command and can be left in place.
