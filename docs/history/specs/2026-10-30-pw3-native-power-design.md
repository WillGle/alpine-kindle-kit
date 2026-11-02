---
status: superseded
date: 2026-10-30
superseded_by: ../../status/CURRENT.md
---

# PW3 native power-button sleep/wake

Status: superseded by the unified Alpine reader OS design; power adapter retained only as a private spike. Source/live restored to qualified candidate; physical button acceptance not completed.

User requests implementation after identifying unfinished power-button sleep/wake. Keep the existing Lua native adapter, kernel, bootstrap/guardian pairing and verified USB recovery. No new maintained tests.

Approach: use KOReader generic screensaver/power events with Linux KEY_POWER from event0, retaining touch event1. Save raw frontlight brightness, turn it off in screensaver, restore it and input on resume. Delay hardware suspend through the existing generic scheduler. While a cable is present (or cable status is unknown), retain screensaver with USB console available and retry later. Only unplugged sleep requests kernel mem, with wakeup_count handshake. Wake-key release closes the screensaver; unexpected/RTC wake retries sleep after30s. Keep physical long-hold reset behavior.

Alternatives: Amazon powerd/LIPC requires the retired OS services; a separate power daemon duplicates KOReader state; screen-only sleep cannot establish power saving. The chosen existing generic flow minimizes changes. USB-connected screen-only sleep is an implementation decision to preserve physical administration. No Wi-Fi, auto-poweroff, hibernation, or battery-life guarantee.

Evidence before implementation: PM devices dry-run returned0; actual mem suspend returned0 after a20s RTC alarm on unchanged boot; alarm cleared. Disposable Lua probe failed on canSuspend=false. Acceptance: mock checks for button mapping, events, light/input restore, cable/unknown-state gates and wakeup race; real short-press lock/unlock, unplugged sleep/button wake, same-boot USB reconnect and reader touch.
