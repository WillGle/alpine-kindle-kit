---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Kindle workspace — session entry

Read [current native Alpine handover](../status/CURRENT.md) first,
then the linked operations/evidence for the requested task. Do not infer the
current device state from old appliance plans or historical entries in BACKLOG.md.

- Preserve the installed bootloader/kernel and independent USB recovery.
- Ordinary USB only; no UART, opening the device or guessed block-device writes.
- Keep research notes, logs and backups on this host, not the Kindle.
- Keep edits minimal and unrelated state intact. No new maintained tests unless asked.
- Historical captures and release archives are evidence; do not delete or rewrite
  them as part of documentation cleanup.
- The migration began without usable Git metadata and used a private preservation inventory. Git metadata became available during the final audit; no commits or rollback were performed.
- Before device writes, verify fresh identity/state. Only one USB console or
  maintenance operation at a time; never export FAT while Alpine uses its root image.
- Update CURRENT.vi.md and ACCEPTANCE.json when runtime/source/release diverge;
  append history to docs/BACKLOG.md. Do not silently edit old releases.

- All project documentation lives under docs. Use only docs/status/BACKLOG.md
  for history/backlog; do not recreate Markdown copies or redirect files in source directories.

- Maintained source is in os/, apps/ and tools/. Local/generated/private data belongs in ignored work/; new release output belongs in dist/.
- Read [STRUCTURE.md](repository-layout.md) for source/work boundaries and [layout-moves.json](../evidence/layout-moves.json) for historical paths.
- Base Alpine is the default build; KOReader is an explicit reader variant. Use current tools/, not archived experiments.
- Use --work-dir for output defaults; never put captures, images, keys or build products in maintained source.
