---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Command-line interface

All maintained host tools print their authoritative options with `--help`.
This page is the map, not a replacement for that output.

## Host tools

| Command | Purpose |
| --- | --- |
| `python3 tools/build/pw3-build.py --help` | Build an Alpine base or reader candidate |
| `python3 tools/build/pw3-os-trial.py --help` | Build a full OS trial image from a prepared root and binaries (requires `--agent`) |
| `python3 tools/install/pw3-control.py --help` | Create a private, host-bound control session |
| `python3 tools/install/pw3-install.py --help` | Supervised first installation and recovery steps |
| `python3 tools/maintenance/kindle-linux.py --help` | Inspect, rescue, resume, deploy and copy documents |
| `python3 tools/maintenance/capture-screen.py` | Capture the current framebuffer |
| `python3 tools/package/public-source.py` | Create the allowlisted public source archive |
| `python3 tools/package/verify-package.py` | Verify an extracted public source archive |
| `python3 tools/build/gwapp.py build` | Build a signed `.gwapp` bundle |
| `python3 tools/docs/check.py` | Check and regenerate documentation status |

## Device tools

| Command | Purpose |
| --- | --- |
| `gwctl status` | Service, display, lock and USB state |
| `gwctl suspend-test` | Root-only suspend/resume test (wakes after 10 seconds) |
| `gwctl lock` / `gwctl unlock` | Drive the lock screen |
| `gwctl ui-nav` / `gwctl ui-tap` | Navigate and tap the UI for diagnostics |
| `gw-appctl list` | List installed applications |
| `gw-appctl verify FILE` | Verify a signed `.gwapp` bundle |
| `gw-appctl install FILE` | Install or update a signed bundle |
| `gw-appctl remove ID` | Remove an application while retaining its data |
| `gw-appctl scan` | Scan USB for bundles |
| `gw-agent` | Thin hub client; reads `/etc/gleworks/hub.conf` and writes `/run/gw-hub/frame.raw` |
| `gw-tailscale` | On-demand Tailscale: `on`, `off`, `status`; not started at boot |

`gwctl` and `gw-appctl` require root on the device and talk to the fixed
command socket documented in [`ipc.md`](ipc.md). There is no arbitrary command
execution endpoint.
