---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: user
---

# GleWorks Kindle documentation

GleWorks OS is a minimal native Linux system for the Kindle Paperwhite 3: Alpine
as the base, the installed kernel and bootloader retained, and an LVGL/EPDC
interface drawn directly to the framebuffer. KOReader is an application, not the
OS shell. The supported profile is PW3 `G090KB`, firmware `5.16.2.1.1`, kernel
`3.0.35-lab126`.

Start with the category that matches your task:

- **Current truth** — [`status/CURRENT.md`](status/CURRENT.md) (generated from
  [`status/STATUS.json`](status/STATUS.json)), [`status/BACKLOG.md`](status/BACKLOG.md),
  [`status/ROADMAP.md`](status/ROADMAP.md), and
  [`status/EVIDENCE-INDEX.md`](status/EVIDENCE-INDEX.md).
- **Tutorials** — guided first success:
  [`tutorials/01-build-private-candidate.md`](tutorials/01-build-private-candidate.md),
  [`tutorials/02-first-install-community.md`](tutorials/02-first-install-community.md),
  [`tutorials/03-first-boot.md`](tutorials/03-first-boot.md).
- **How-to** — task instructions:
  [`build-and-install.md`](how-to/build-and-install.md),
  [`build-private-candidate.md`](how-to/build-private-candidate.md),
  [`build-arm-binaries.md`](how-to/build-arm-binaries.md),
  [`kindle-hub.md`](how-to/kindle-hub.md),
  [`customize-lock-screen.md`](how-to/customize-lock-screen.md),
  [`install-update.md`](how-to/install-update.md),
  [`operate-reader.md`](how-to/operate-reader.md),
  [`recover-usb.md`](how-to/recover-usb.md),
  [`qemu-gate.md`](how-to/qemu-gate.md),
  [`capture-evidence.md`](how-to/capture-evidence.md),
  [`package-source.md`](how-to/package-source.md).
- **Reference** — contracts and layouts:
  [`repository-layout.md`](reference/repository-layout.md),
  [`workspace.md`](reference/workspace.md),
  [`device-profile-pw3.md`](reference/device-profile-pw3.md),
  [`ipc.md`](reference/ipc.md),
  [`app-package.md`](reference/app-package.md),
  [`app-session-contract.md`](reference/app-session-contract.md),
  [`kindle-hub-protocol.md`](reference/kindle-hub-protocol.md),
  [`lock-screen.md`](reference/lock-screen.md),
  [`cli.md`](reference/cli.md),
  [`paint-profile-engine.md`](reference/paint-profile-engine.md),
  [`display-programming-rules.md`](reference/display-programming-rules.md),
  [`contributing.md`](reference/contributing.md),
  [`licenses.md`](reference/licenses.md).
- **Explanation** — architecture and theory:
  [`architecture.md`](explanation/architecture.md),
  [`boot-and-recovery.md`](explanation/boot-and-recovery.md),
  [`eink-display.md`](explanation/eink-display.md),
  [`prior-art.md`](explanation/prior-art.md),
  [`decisions/`](explanation/decisions/README.md).
- **Evidence** — [`evidence/README.md`](evidence/README.md) and the generated
  [`status/EVIDENCE-INDEX.md`](status/EVIDENCE-INDEX.md).
- **History** — completed plans, superseded docs and old evidence live under
  [`history/`](history/README.md). They are preserved but are not current
  instructions.

## Documentation contract

Maintained docs use the Diátaxis categories plus `status/`, `evidence/` and
`history/`. Every current document declares front matter with `status`,
`updated`, `verified_build`, `supersedes` and `audience`; category docs also
declare `diataxis`. Historical documents declare `status: historical` or
`superseded`, a date, and a forward pointer.

Run the docs checker before committing:

```sh
python3 tools/docs/check.py
python3 tools/docs/check.py --write   # regenerate CURRENT.md and EVIDENCE-INDEX.md
```

The checker is part of `make check` and the docs CI job.
