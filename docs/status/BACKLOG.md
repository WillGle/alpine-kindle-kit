---
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Backlog — Kindle Alpine

Open and tracked work only. Imported historical records and the old anchor index
live in [`../history/backlog-history.md`](../history/backlog-history.md), which
is append-only. Current facts are generated in
[`CURRENT.md`](CURRENT.md); do not infer status from an old checkbox.

| ID | Status | Work |
| --- | --- | --- |
| B01 | Not started | New release with full refresh and locale cleanup; verify package/image before deployment |
| B02 | Awaiting verification | Multi-page reading after the refresh policy change; check ghosting after scrolling and idle cleanup |
| B03 | Partial | Native Wi-Fi and auto-reconnect achieved; Tailscale/SSH after authentication remains open |
| B04 | Not implemented | Authenticated web upload of documents over VPN |
| B05 | Partial | Mem suspend, RTC wake and the physical button path achieved; power measurement remains open |
| B06 | Not verified | Amazon fallback boot after cleanup; do not boot it only to close backlog |
| B07 | Partial | App runtime: package/installer/UI achieved; generic launch, UID/limits/fd handoff, crash/OOM, data image and `AS-*` checklist remain open. Contract: [`../reference/app-session-contract.md`](../reference/app-session-contract.md); history: [`../history/plans/2026-10-31-app-runtime.md`](../history/plans/2026-10-31-app-runtime.md) |
| B08 | Partial | App manager and signed USB installer/update UI achieved; OS A/B, online catalog and per-app UID remain open |
| B09 | Deployed, device acceptance pending | UI polish Phase 0–7 host passed; ARM shell deployed; touch/ghosting and paint-quality regression on PW3 remain |
| B10 | Implemented and device-verified, partly superseded | Tap/scroll feedback merged with the 2026-11-01 UI review; scroll policy later refined by B11. Evidence: [`../history/evidence/pw3-ui-refresh-2026-11-01/`](../history/evidence/pw3-ui-refresh-2026-11-01/) |
| B11 | Complete and deployed | Storage optimization, Wi-Fi list fix and scroll ghost cleanup (2026-11-01). Evidence: [`../evidence/PW3-STORAGE-OPTIMIZATION-2026-11-01.md`](../evidence/PW3-STORAGE-OPTIMIZATION-2026-11-01.md), [`../evidence/pw3-wifi-scroll-fix-2026-11-01.md`](../evidence/pw3-wifi-scroll-fix-2026-11-01.md) |
| B12 | Complete and deployed | Repository hardening after the 2026-11-01 review; plan in [`../history/plans/2026-11-01-repository-hardening.md`](../history/plans/2026-11-01-repository-hardening.md) |
| B13 | Fixed and deployed | Brightness key full-screen flash; scroll cleanup now requires a real drag |
| B14 | Implemented and deployed | Shared paint engine and per-screen profile; evidence: [`../evidence/PW3-PAINT-PERF-2026-11-01.md`](../evidence/PW3-PAINT-PERF-2026-11-01.md) |
| B15 | Deployed and accepted | Static text visibility tuning; periodic full refresh every 15 page snaps |
| B16 | Deployed, speed perception pending | REAGL text page snap experiment |
| B17 | Tried and reverted | DU4 lost stroke quality on text; reverted to GC16 |
| B18 | Deployed and accepted | Two-band text page repaint |
| B19 | Deployed and accepted | Three-band text page repaint |
| B20 | Deployed and accepted | Four-band text page repaint |
| B21 | Deployed, perception pending | Reader open/close latency reduction; audit: [`../evidence/latency-audit-2026-11-01.md`](../evidence/latency-audit-2026-11-01.md) |
| B22 | Deployed, reconfirmation pending | Reader startup plugin trim |
| B23 | Code A–E complete; new UI device acceptance pending | Code-review remediation; plan: [`../history/plans/2026-11-01-code-review-remediation.md`](../history/plans/2026-11-01-code-review-remediation.md); device evidence: [`../evidence/remediation-device-2026-11-01.md`](../evidence/remediation-device-2026-11-01.md) |
| B24 | Complete (2026-11-02) | UI/UX hierarchy and shared component pass; plan: [`../history/plans/2026-11-02-ui-ux-hierarchy.md`](../history/plans/2026-11-02-ui-ux-hierarchy.md); evidence: [`../evidence/pw3-ui-hierarchy-2026-11-02/README.md`](../evidence/pw3-ui-hierarchy-2026-11-02/README.md) |
| B25 | Complete (2026-11-02) | Diátaxis documentation migration: status source, front matter, evidence index, history split and docs CI. Plan: [`../history/plans/2026-11-02-diataxis-docs-migration.md`](../history/plans/2026-11-02-diataxis-docs-migration.md) |
| B26 | P0.5 next; device acceptance pending | Thin-client hub: `gw-agent` on the PW3 and `kindle-hub` on a Proxmox LXC for server-side web rendering, mirror, document conversion and OTA. Roadmap: [`../status/ROADMAP.md`](../status/ROADMAP.md); contract: [`../reference/kindle-hub-protocol.md`](../reference/kindle-hub-protocol.md); how-to: [`../how-to/kindle-hub.md`](../how-to/kindle-hub.md); host evidence: [`../evidence/kindle-hub-host-2026-11-02.md`](../evidence/kindle-hub-host-2026-11-02.md) |
| B27 | Waiting on server | Bring up the `kindle-hub` LXC: Debian 13, Tailscale, FastAPI, Playwright, `/healthz`, test frame and event echo. Required before P0.5. Checklist: [`../status/ROADMAP.md`](../status/ROADMAP.md) |
| B28 | Deployed trial shell; plugin menu acceptance pending | Custom lock screen via KOReader sleep-screen export. Contract: [`../reference/lock-screen.md`](../reference/lock-screen.md); how-to: [`../how-to/customize-lock-screen.md`](../how-to/customize-lock-screen.md); host evidence: [`../evidence/lock-screen-host-2026-11-02.md`](../evidence/lock-screen-host-2026-11-02.md); device evidence: [`../evidence/lock-screen-device-2026-11-02.md`](../evidence/lock-screen-device-2026-11-02.md) |

This list tracks work; it does not authorize a boot change or expand a task.
