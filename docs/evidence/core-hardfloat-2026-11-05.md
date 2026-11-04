---
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Core hard-float correction

The former core build mixed soft-float compiler/startup objects with Alpine's
hard-float dynamic libc. A disposable ARM QEMU user probe reproduced an actual
failure: `strtod("1.25")` returned `0.0`, exit 19. ELF flags alone were not the
only problem. Rebuilding musl startup objects alone was insufficient because
the old GCC runtime was also soft-float; no mismatch suppression was used.

The core-only builder now uses the Bootlin 2026.08-1 ARMv7 musl toolchain
(GCC 15.3.0, musl 1.2.6), pinned by URL/SHA256 in
`tools/build/core-toolchain.json`. The downloaded archive matched the upstream
checksum. Flags use ARMv7-A, VFPv3-D16, hard-float; the bootstrap/kernel
toolchain is unchanged. Both dynamic and static float probes return 1.25.

New artifacts are retained at `work/build/alpine-core-hardfloat-20261105/`;
previous artifacts/evidence remain intact. The exact hash, payload and VM
measurements are in [results](core-hardfloat-2026-11-05.json).
Full-system ARM QEMU passed 41 lifecycle/package/UI/float checks and one
separate recovery-console check. The image checksum remained unchanged.
This supersedes the old core build for future host trials, not its historical
measurements. The image is still VM-only, not a PW3 installation candidate.

Private raw logs and reproduction sources are under
`work/private/core-abi-20261105/`. Existing source syntax/lint checks were run
after edits. Fresh full host/package checks could not be rerun in the current
sandbox because Nix daemon access is denied; earlier host checks do not
certify this toolchain change.

## Community application research

Miniserve 0.35.0 (MIT) is the selected file-drop candidate for evaluation:
upstream provides an ARMv7 musl binary, HTTP authentication, upload, no-symlink
mode and duplicate-file rejection. Its upstream checksum matched
`0be26e48d8d7b4f2a9019b30c9eae1a1054c482989ec41024025bd9f54f9c8ff`.
The ARM binary runs `--version` in QEMU user mode. Community evidence includes
the existing Homebrew formula and upstream's Arch/Scoop packaging instructions,
not just repository stars. Sources:

- https://github.com/svenstaro/miniserve/tree/v0.35.0
- https://github.com/Homebrew/homebrew-core/blob/master/Formula/m/miniserve.rb
- https://github.com/9001/copyparty (MIT alternative; requires Python runtime)

The HTTP probe is prepared privately, but socket creation is denied in the
current sandbox. No upload, authentication, resource-limit, full-disk, session
teardown, real tailnet forwarding or PW3 acceptance is claimed. In particular
an upload-size limit still needs qualification; upstream's browser concurrency
setting is not a server resource limit. No candidate is installed or signed
as an accepted app.

For mirror, the established Kindle-specific candidate is
https://github.com/hwhw/kindlevncviewer (GPL-2.0, observed commit
`5849208c2406c5e439666a9010f5b71973de9408`). Its README links the existing
MobileRead discussion and uses LibVNCClient. Last upstream push observed was
2022; current PW3/EPDC/app-handoff compatibility remains unverified, so it is
not selected as a release dependency. LibVNCServer 0.9.15 is another upstream
reference, not a completed mirror app. Existing hub plans remain unchanged.
