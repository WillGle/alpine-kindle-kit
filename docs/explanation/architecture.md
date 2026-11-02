---
diataxis: explanation
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Architecture

GleWorks OS keeps the installed PW3 kernel and bootloader and replaces the
Amazon userspace with a small Alpine system. There is no X11, Wayland or
compositor: the UI renders through LVGL into the EPDC framebuffer.

## Runtime layers

```text
stock bootloader + kernel
        |
        +-- static bootstrap / PID1  (os/devices/pw3/bootstrap)
        |       selects Alpine, stock return or USB rescue
        |
        +-- Alpine root (p4 image)
                |
                +-- gw-system (supervisor, power, Wi-Fi, apps, IPC)
                |       /run/gw-system.cmd, /run/gw-system.evt
                |
                +-- gw-shell (LVGL UI, EPDC policy, screens)
                |       Home, Network, App manager, Settings, To-dos,
                |       Calendar, Kindle Hub, Lock
                |
                +-- gw-agent (thin hub client)
                |       /run/gw-hub.sock, raw frame cache, touch/key
                |
                +-- KOReader (managed foreground application)
```

## Components

| Component | Responsibility |
| --- | --- |
| `os/devices/pw3/bootstrap` | Early mount, boot markers, stock return, USB rescue |
| `os/devices/pw3/guardian` | Reset deadline and rollback to stock |
| `os/system/gw-system` | Single hardware owner: power, Wi-Fi, app install/launch, command socket |
| `os/system/gwctl`, `gw-appctl` | Fixed-command clients |
| `os/system/gw-agent` | Thin hub client: HTTP frames, raw frame cache, touch/key forwarding |
| `os/ui/gw-shell` | LVGL screens, theme, input and paint profiles |
| `os/ui/lv_port_epdc.*` | The only code that submits `MXCFB_SEND_UPDATE` |
| `apps/koreader` | Reader layer and Return-to-Home integration |

## Contracts

- [`ipc.md`](../reference/ipc.md) — command and event socket, peer credentials,
  command policy.
- [`app-package.md`](../reference/app-package.md) — signed `.gwapp` format and
  installer behaviour.
- [`app-session-contract.md`](../reference/app-session-contract.md) — draft
  foreground app session contract and remaining gaps.
- [`display-programming-rules.md`](../reference/display-programming-rules.md) —
  mandatory waveform and paint rules.
- [`kindle-hub-protocol.md`](../reference/kindle-hub-protocol.md) — thin-client
  frame, touch, status and shell-link contract.
- [`../reference/repository-layout.md`](../reference/repository-layout.md) —
  source and work-directory boundaries.

## Current limits

The signed app manager and built-in screens are implemented. Generic launch of
apps outside the reader, per-app UIDs and limits, fd-based display/input handoff,
authenticated web upload, OS A/B update and an online catalog remain pending.
The Kindle hub agent and Hub screen are implemented on the host; physical PW3
frame/touch acceptance is still pending. See
[`../status/CURRENT.md`](../status/CURRENT.md) and
[`../status/BACKLOG.md`](../status/BACKLOG.md).
