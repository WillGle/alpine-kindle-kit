---
status: historical
date: 2026-10-30
superseded_by: ../../status/CURRENT.md
---

# Repository layout implementation plan

> Execute directly with superpowers:executing-plans. User approved the proposed layout and instructed immediate execution. No new maintained tests, no device writes, no usable Git metadata.

**Goal:** Separate Alpine/runtime source, reader source, host tools, historical code and local working data.

**Architecture:** `os/alpine`, `os/devices/pw3`, `apps/koreader`, `tools/{build,install,maintenance,package,lib}`, `docs`, `archive`, ignored `work` and `dist`. Host tools resolve source from their own location and local output from `--work-dir`; default base build excludes KOReader, explicit reader variant retains paired identity/artifact gates.

**Spec:** Approved proposal in the conversation, 2026-10-30.

## Constraints and review focus

- Preserve all existing data, backups, release archive bytes and historical record contents; verify rename identity and important artifact hashes.
- No compatibility symlinks or duplicate maintained source trees; archive old experiments instead of treating them as active code.
- No host scripts/build outputs inside OS or app source; no reader files in base image.
- Workspace can live outside repo and commands can run from any current directory.
- Do not bypass PW3 identity, manifest, guardian or recovery gates when changing paths.
- Current docs follow new paths; historical records retain original contents, with a migration map for lookup.
- Public source package allowlist excludes all work/private/archive data and generated tokens/images.

## Task 1 — inventory and moves

- [x] Record move map, file metadata and important artifact hashes; save original maintained source in private migration backup.
- [x] Move active runtime/host sources, vendor downloads/sources/toolchains, private evidence, old app layer and experimental code/data to their approved destinations.
- [x] Verify every original regular file/link is present at its mapped path with identical inode/size/mtime before editing maintained source.

## Task 2 — boundaries and tools

- [x] Add Alpine/reader source manifests and exact PW3 profile; centralize repo/work path resolution in tools/lib.
- [x] Update all active CLI imports, resources and output defaults; keep migration/qualification behavior unchanged.
- [x] Make base Alpine the default build; add explicit KOReader variant with app data separate from OS. Preserve image size and paired guardian protocol.
- [x] Package new source layout with explicit allowlist, keep historical source/image packages unchanged.

## Task 3 — docs and validation

- [x] Update current guides, community instructions, WORKSPACE and acceptance; retain historical documents and append BACKLOG.
- [x] Run syntax/help/link/public-manifest/private-data checks and disposable behavior/variant probes, including out-of-repo cwd and external work-dir.
- [x] Build base and reader candidates with explicit prepared dependencies from extracted source; check filesystem/base excludes reader, compare paired source/artifact metadata.
- [x] Obtain final independent code review, fix material findings, record exact evidence and hardware limits.

## Execution decisions

- Work in place, with private original-source backup and rename inventory, because Git metadata is unusable. No repository initialization.
- Prepared legacy kernel build output may embed old paths; preserve the historical original and adapt a separate work/build copy for validation.
- Keep all notes in this plan/BACKLOG and machine-readable evidence; no new maintained test suite.

Validation: both extracted-source variants built successfully; 35 behavior checks, 27 syntax checks, source/payload hash matching, root-owned critical inodes, base excludes reader/glibc, current Markdown links resolve. See `docs/evidence/repository-layout-checks.json`.
