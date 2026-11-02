---
status: historical
date: 2026-10-30
superseded_by: ../../status/CURRENT.md
---

# PW3 community installer implementation plan

> **For agentic workers:** Use superpowers:executing-plans, directly in this session. User authorized completion; no new maintained tests. No usable Git metadata: preserve changed inputs in /tmp instead of worktree/commits.

**Goal:** Deliver an offline-buildable, guarded Linux-host PW3 installer and a private-data-free source kit, with hardware acceptance reported separately.

**Architecture:** Reuse the stock-kernel bootstrap, dispatcher, guardian and reader. Add a Python coordinator with per-device private session state and fresh checks at each mutation; do not add cloud, GUI or multi-model architecture.

**Tech stack:** Python standard library, POSIX shell, SSH, ARM compiler, e2fsprogs, UDisks, existing kernel build.

**Spec:** ../specs/2026-10-30-pw3-community-installer-design.md (accepted by user instruction to complete PW3 first).

## Global constraints

- PW3 Muscat/Wario, firmware 5.16.2.1.1, kernel 3.0.35-lab126 only; root required.
- Preserve bootloader/kernel, independent USB recovery, historical releases and private data.
- No new maintained tests; disposable behavioral probes on host, syntax and existing package checks.
- No public upload; no automatic device mutation in this development session.
- First-install acceptance on stock PW3 cannot be inferred from existing native device.

## Review focus

- Wrong model/firmware/session or changed device must fail before writing.
- Interrupted or repeated steps must not overwrite backup, existing stages or boot requests.
- Recovery evidence must bind session/device/build; a copied old proof cannot promote.
- Source package must exclude keys, tokens, captures and device-specific generated config.
- Clean extracted build must not need author home/Nix paths or author binaries.

## Task 1: Reusable build inputs and fresh private candidates

- [x] Add a candidate builder with explicit rootfs, modules, reader, p1 backup, toolchain/kernel build inputs and new output directory.
- [x] Generate paired identity only inside output workspace, compile paired components and build clean reader root with captured modules and compatibility libraries.
- [x] Remove fixed launcher dependency by rendering return config from explicitly supplied launcher hash; expose portable build environment paths without changing defaults for existing local work.
- [x] Verify candidate source/build paths, ELF/ARM checks, filesystem and manifest; reject malformed/missing inputs. Record limitations if dependencies unavailable.

## Task 2: First-install coordinator

- [x] Add `pw3-install.py` with explicit per-device session and pinned SSH, strict stock identity/layout preflight and host backup/capture.
- [x] Add staged install, stock return, readonly native trial, deliberate invalid-image independent rescue/repair qualification and promotion; fresh evidence and artifact binding required throughout.
- [x] Keep CLI steps separate for reboots/recovery; save operation intent before mutation and refuse blind retry after interruption. Never auto-delete user documents or erase Amazon state.
- [x] Verify rejection paths and happy-path state transitions with disposable fakes; never execute these probes against hardware.

## Task 3: Public source kit and contributor documentation

- [x] Add English installation/contribution documentation under ours/docs and a minimal explicit source packager, excluding machine captures, generated headers/configs, historical probes and private material.
- [x] Include source license/provenance status; avoid redistributing captured proprietary components or unreviewed reader images.
- [x] Verify manifest/extraction/CLI from an empty directory and inspect public payload for private values and host paths.

## Task 4: Review and handover

- [x] Run syntax, disposable behavior/build checks and package verification; obtain fresh final code review and fix material findings.
- [x] Update CURRENT, ACCEPTANCE and append BACKLOG with source/host/device/release boundaries and exact remaining hardware acceptance.
- [x] Do not publish, certify another PW3 or describe first-install hardware acceptance as passed without fresh evidence.

## Execution record

User authorized completion in-session. No usable Git metadata; existing inputs retained and comparison copies in /tmp/pw3-installer-before. No new maintained tests. Disposable probes: 35 rejection checks, 2 bound-rescue checks and simulated flow with 16 shell scripts. Private candidate built successfully from freshly extracted source using explicit prepared dependencies. One independent review finding fixed: final rescue now keeps the authenticated console and checks rescue session before advancing. Hardware first-install, full toolchain clean-room recreation and physical reading remain pending; no device writes or publication performed.
