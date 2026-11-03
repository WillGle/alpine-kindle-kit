---
diataxis: how-to
status: current
updated: 2026-11-03
verified_build: none
supersedes: none
audience: user
---

# Store files on the Kindle (vault)

Status: **storage directory, cable copy and homelab pull are implemented.**
Files are stored in plain form; this is private storage, not encryption.

```text
/mnt/us/gleworks/vault/
```

`/mnt/us` is the 3.0 GB FAT control partition; about 2.8 GB is free.

## Cable: command copy

From the computer, with the USB cable connected:

```sh
python3 tools/maintenance/kindle-linux.py put-vault secret.pdf notes.txt
```

The command enters the proven USB rescue mode, copies each file into
`gleworks/vault` on the FAT partition, verifies the SHA-256, and resumes
Alpine. It refuses a file larger than 64 MiB and never overwrites an existing
name.

## Cable: drag-and-drop

The runtime USB gadget is the serial console, not mass storage. To use the
Kindle as a USB drive, use the same proven rescue path:

```sh
python3 tools/maintenance/kindle-linux.py rescue
```

The control partition appears as a removable drive. Copy files into:

```text
gleworks/vault/
```

Eject the drive on the computer, then:

```sh
python3 tools/maintenance/kindle-linux.py resume
```

Do not switch the gadget to mass storage while Alpine is running: it would
drop the USB console and the getty holds the serial function.

## Network: pull from the homelab

Edit `/mnt/us/gleworks/vault.conf`:

```text
url=http://192.168.1.10:8080/vault
token=optional-bearer-token
# proxy=tailscale
```

The server must publish `index.txt` with one line per file:

```text
<sha256>  <filename>
```

and the files beside it. The Kindle:

1. downloads `index.txt`;
2. skips files whose local SHA-256 already matches;
3. downloads new files to `/run`, verifies SHA-256 and the 64 MiB limit;
4. renames them into `/mnt/us/gleworks/vault/`.

Only flat names using `A-Z a-z 0-9 . _ + -` are accepted. Nothing outside the
vault is written.

Run it now:

```sh
gw-vault-pull
```

or wait for the five-minute loop started at boot. The log is
`/run/gw-vault-pull.log`.

### Tailscale proxy

If the homelab is only reachable over the tailnet, or the LAN filters the
return path, set:

```text
proxy=tailscale
```

The puller starts `gw-tailscale on` on demand, uses the loopback HTTP proxy on
`127.0.0.1:1056`, and stops the daemon afterwards. The homelab URL should then
use its tailnet address (for example `http://100.119.82.77:8080/vault`).

## Limits and safety

- Maximum file size: 64 MiB.
- Names are flat; subdirectories and symlinks are refused.
- The FAT partition is readable over USB. Do not store real secrets unless you
  accept that exposure; encryption is a separate project.
- A failed download stays in `/run` and is removed; a failed cable copy is
  removed before resume.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `gw-vault-pull` does nothing | `url=` is empty in `vault.conf` |
| `index fetch failed` | URL reachable from the Kindle; try `proxy=tailscale` |
| `502 Bad Gateway` with proxy | homelab port not reachable on the tailnet (firewall) |
| `hash/size mismatch` | server `index.txt` does not match the served file |
| Cable copy says already exists | choose another name or remove the old file |
