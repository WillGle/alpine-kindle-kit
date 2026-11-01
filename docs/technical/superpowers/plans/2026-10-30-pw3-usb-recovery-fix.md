# PW3 USB and recovery repair plan

User explicitly requests comprehensive fixes after detailed RCA; proceed with existing C/Python/shell architecture. No new maintained test suite. Preserve all inherited changes and paired device identity; no unrelated cleanup or publication.

- [x] Repair bounded console upload, verify with disposable oversized-payload and real-device checks.
- [x] Build a minimal PW3 USB gadget module change in a separate vendor-source staging directory: keep UDC awake on physical disconnect, rather than entering the demonstrated non-waking low-power path. Validate ABI/build; trial root-only module with original retained and independent rescue intact. Reject if hardware hotplug fails.
- [x] Verify repeated real reconnect with unchanged boot ID; clean reboot/rescue; synchronize builder and public-source allowlist for reproducibility.
- [x] Assess autonomous crash recovery and visible rescue state without bypassing pending, paired identity, unmount or reset-deadline gates. Implement only a concrete bounded safe path; record any remaining qualification limits.
- [x] Record RCA/evidence, update current acceptance and backlog. Do not claim first-install or power-loss recovery from host checks.

Alternatives rejected: force-detect alone (hardware failed), clock wake plus force-detect (hardware clock on but no enumeration), automatic reboot after unplug (interrupts reading, masks driver fault). Holding the existing controller awake trades power for reliable physical administration; power impact remains an explicit limitation until measured.
