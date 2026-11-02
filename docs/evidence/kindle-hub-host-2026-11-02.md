---
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Kindle hub — host checks (2026-11-02)

Status: **host-only evidence.** No physical PW3 was booted, no framebuffer was
written and no device partition or boot path was changed. This record closes
host gates only; `HUB-02` through `HUB-08` remain pending.

## Scope

- `gw-agent` HTTP client, configuration, raw-frame write and event forwarding.
- `gw-shell` Hub screen, `/run/gw-hub.sock` line handling and touch forwarding.
- Build/image integration for `gw-agent` and its OpenRC unit.

## Results

| Check | Result |
| --- | --- |
| `make check CC=cc` | **CHECK PASSED** |
| UI geometry | **2864 checks, 0 failed** |
| `gw-agent` host tests | **5/5 passed** |
| ARM build | **OK** |
| Device write | **none** |

Host tests cover:

- a full raw frame is written byte-for-byte;
- `204 No Content` is not an error and leaves no frame;
- an oversized `Content-Length` is rejected before writing;
- a symlinked config is rejected;
- a touch line reaches the hub as JSON.

## ARM hashes

```text
gw-agent   e48c7ad61f5ada4e9efeaec4ae2b062068c3d180dde8891adec2e7a9cc774191
gw-shell   0f55dee3842548ad64a1a42a000903d4c2534626d6bef17b704a7d8d16f5b3dc
gw-system  9afb2e553fbecb84839db7601e33618a884a565c3fade59bfaa4c917ea323445
```

These are build artifacts, not deployed binaries. The current deployed shell
remains `1804ea8f…` until the image is installed and accepted.

## Not proven

- A frame is visible on the PW3 panel.
- Touch scroll reaches the hub on hardware.
- Hub-down behaviour on the device.
- Tunnel mode has no inbound listener on `wlan0`.
- Mirror geometry and refresh policy on hardware.
- One-hour memory and fd stability.

## Reproduction

```sh
nix-shell tools/build/nix-shell.nix --run 'make check CC=cc'
nix-shell tools/build/nix-shell.nix --run \
  'env -u CC -u STRIP -u CFLAGS sh ./tools/build/build-arm-binaries.sh'
```

See [`../how-to/kindle-hub.md`](../how-to/kindle-hub.md) and
[`../reference/kindle-hub-protocol.md`](../reference/kindle-hub-protocol.md).
