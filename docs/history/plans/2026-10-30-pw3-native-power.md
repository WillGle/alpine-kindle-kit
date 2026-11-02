---
status: historical
date: 2026-10-30
superseded_by: ../../status/CURRENT.md
---

# PW3 native power implementation plan

Status: superseded by the unified Alpine reader OS design; power adapter retained only as a private spike. Source/live restored to qualified candidate; physical button acceptance not completed.

Goal: short power press sleeps/wakes KOReader with real unplugged suspend, preserving USB administration and rescue.
Architecture: extend apps/koreader/overlay/opt/koreader/native.lua only; use existing KOReader generic PM flow and vendor sysfs. No new maintained test suite.

- [x] Verify baseline, PM devices dry-run and RTC-bounded real suspend; preserve logs.
- [x] Observe disposable Lua probe failing before implementation.
- [ ] Add event0/KEY_POWER, native PM handlers, frontlight restore and guarded mem suspend; run disposable probe and syntax check.
- [ ] Back up live native.lua, deploy with hash verification and restart reader only.
- [ ] Verify physical connected/unplugged button cycles and same-boot USB/read recovery.
- [ ] Synchronize evidence/current/community status; retain power measurement and first-install limits.
