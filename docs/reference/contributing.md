---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Contributing

Start with the PW3 source kit and [status](../status/STATUS.json). Keep changes small,
preserve provenance/comments and historical evidence, and distinguish source
checks, emulator behavior, real-device results and user-visible acceptance.

Current admission profile: `pw3-0kb-5.16.2.1.1` only. Other PW3 model codes,
firmware or partition layouts are unsupported until separately qualified.
Do not add an override flag to bypass identity/layout/recovery checks.

For another profile/model, submit:

- Exact public model code, firmware and kernel versions, architecture, RAM,
  partition layout and authenticated root/boot prerequisites. Redact serials.
- Source/license provenance for code, modules, firmware and build inputs.
  Do not redistribute proprietary captures without verified permission.
- A reproducible build and a recovery path independent of the candidate root.
- Host backups made before writes; clean default boot, native PID1/root and
  fresh USB challenge evidence; deliberate failure/repair and a complete
  native → rescue → native roundtrip on that model.
- Physical reader, touch and page-refresh checks; power/suspend results only
  when actually measured. Note every untested part explicitly.

A different Android reader needs its own boot/driver/recovery port. An ESP32
reader needs firmware appropriate to its microcontroller; it cannot be added
as an Alpine profile. No generic multi-device plugin framework is required.

Before sending a contribution, run the existing syntax/help/package checks
and appropriate disposable behavioral checks. Do not introduce maintained
tests or unrelated refactors without agreement. Never attach private session
directories, keys, tokens, backups, books or full device identifiers.

If reporting an interrupted install, provide command name, phase, error,
public profile and redacted diagnostics. Keep the original private evidence
locally. Do not force the next state or repeatedly reboot to make a gate pass.
