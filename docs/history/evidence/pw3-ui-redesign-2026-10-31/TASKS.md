---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Session task inventory

Scope: PW3 shell UI software tasks for this session

| ID | Status | Summary |
|---|---|---|
| `ui-redesign` | **done** | Reading-first Home, Settings, Calendar, To-dos, Network implemented |
| `shared-style-constants` | **done** | Shared layout/style constants and theme tokens |
| `vietnamese-fonts` | **done** | Vietnamese + LVGL symbol glyph fonts generated and checked in |
| `host-lvgl-simulator` | **done** | Host LVGL preview renders 1072x1448 without /dev/fb0 |
| `acceptance-screenshots` | **done** | Home, Settings, Calendar, To-dos, To-dos history, Network, empty and popup renders produced |
| `geometry-navigation-tests` | **done** | 633 geometry/navigation checks, 0 failures |
| `keyboard-latency` | **done** | A2 key feedback, frame splitting, pause-based text refresh |
| `keyboard-flicker` | **done** | Separate key regions; no whole-keyboard or per-key full-screen promotion |
| `input-ghosting` | **done** | Localized REAGL text cleanup after typing pause; no full-screen dismiss flash |
| `network-popup` | **done** | Bounded styled popup with DU partial region |
| `todos-keyboard-layout` | **done** | To-dos input/list layout separated from keyboard |
| `keyboard-symbols` | **done** | Backspace/enter/arrows/check/keyboard glyphs present |
| `todos-feature` | **done** | Checkbox TODOs, Active/History views, soft delete/restore, atomic TSV storage |
| `visibility-contrast` | **done** | Darker grayscale tokens, bold section labels, 2 px borders, no layout change |
| `shared-ui-kit` | **done** | Shell UI kit created; pages migrated pixel-identically |
| `theme-consistency` | **done** | C and Lua theme adapters up to date |
| `arm-musl-build` | **done** | Static ARM/musl artifact built and deployed |
| `wifi-recovery` | **done** | Connected to SSID Will at 192.168.1.200 |
| `tailscale-recovery` | **done** | alpine-reader peer online |
| `temporary-artifact-cleanup` | **done** | Diagnostic binaries/logs/socket removed from device |
| `closure-report` | **done** | Closure report and task inventory recorded |
| `physical-visual-acceptance` | **out-of-scope** | Requires human visual inspection in room and low light; not a software automatable task |
| `suspend-wake-hardware-test` | **out-of-scope** | Requires controlled physical wake; outside this software task scope |
| `long-session-battery-measurement` | **out-of-scope** | Requires a long physical battery run; outside this software task scope |

## Open tasks

```text
none
```

Total: 24 closed/out-of-scope, 0 open.
