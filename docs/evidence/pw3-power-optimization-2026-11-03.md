---
status: current
updated: 2026-11-03
verified_build: 6db3bec21a4ace7fc4f816691eeeddfacfa3d5e81b9f7827ef82134065c73c72
supersedes: none
audience: maintainer
---

# Power optimisation: 5 s retry and on-demand Tailscale (2026-11-03)

Status: **deployed; two device checks pending.** The suspend retry window is
shortened, Tailscale no longer starts at boot, and the Hub screen starts/stops
it. The Hub UI path and the 5-second retry still need a physical interaction
test.

## Changes

| Change | Detail |
| --- | --- |
| Suspend retry | `GW_SUSPEND_RETRY_MS` 30 s → **5 s** |
| Tailscale boot | autostart removed from `gleworks-early.sh` |
| Control | `/usr/sbin/gw-tailscale on\|off\|status` |
| Hub trigger | `gw_hub_set_view()` runs `gw-tailscale on/off` with fixed path/args |
| SOCKS5 | `--socks5-server=127.0.0.1:1055` |

## Deployment

| Item | Value |
| --- | --- |
| Boot ID | `7a351ab3-2717-44a2-8787-eec34c60f3c2` |
| `gw-system` | `6db3bec21a4ace7fc4f816691eeeddfacfa3d5e81b9f7827ef82134065c73c72` |
| `gw-shell` | `dc80c765b6fdee1b69f7005723214595dddba6a0b85be55d97eaea8511dc3ecf` |
| `gw-tailscale` | `56b1d024558dfd5c944896de2c733600f08c9eb2e75f96a6e702ffe433379714` |
| `gleworks-early.sh` | `c36cf2c7b696f29455d77ae270f2ba7acd1e80523b0adb5fd6b091ddd7c06d7a` |
| Backups | `gw-shell.before-hubtrigger-20261103`, `gw-system.before-opt-20261103`, `gleworks-early.sh.before-tailscale-20261103` |
| Host gate | `make check CC=cc` **PASS** |
| ARM build | **PASS** |

## Verified

- After boot, `pidof tailscaled` is empty.
- `gw-tailscale status` is empty.
- `gw-tailscale on` starts `tailscaled` (PID observed) with loopback SOCKS5.
- `gw-tailscale off` stops it; `status` is empty again.
- Lock/unlock still works on the new shell.
- Previous build measured 1%/h idle on battery.

## Pending

- Open **Settings → Kindle Hub**, confirm `gw-tailscale status` gains a PID,
  then exit and confirm it is empty.
- Lock, unplug for 2 minutes, replug; confirm the log contains
  `suspend scheduled in 5000 ms` and a `slept=` value.
- If the device still wakes every few minutes, identify the wake source.

## Rollback

Restore the three backups above. Tailscale can also be started manually with
`gw-tailscale on` if the Hub trigger regresses.
