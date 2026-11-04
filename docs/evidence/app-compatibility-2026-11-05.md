---
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Agent app compatibility gate — host evidence

Scope: agent-authored/rebuilt ARMv7 app source, checked against an explicitly
selected extracted Alpine userspace before `.gwapp` signing. No device access,
OS image rebuild, dependency download or new maintained unit tests.
[Contract](../reference/app-compatibility.md) and
[authoring workflow](../how-to/agent-apps.md).

## Verification

Disposable inputs and raw reports are retained privately under
`work/private/app-compat-20261105/`:

- 20 host probes: valid ARM package, wrong architecture, missing library,
  float-ABI mismatch, missing command/interpreter, env shebang, source symlink,
  unknown/duplicate declaration, wrong target/version, unsupported service,
  unverified TUN requirement, missing network permission, session server,
  absolute guest symlink, escaping/looping guest symlinks and blocked signing
  without an explicit rootfs.
- Independent review reproduced two invalid-runtime-format false passes and
  one CRLF-manifest false pass before correction; all three block afterward.
  Required loaders/libraries must be ELF; script interpreters must be ELF,
  and executable commands require ELF or an explicit supported shebang.
- Two additional probes: wrong-architecture and missing-library builds fail
  before creating a bundle or signature.
- Valid declared package builds and its RSA/SHA-256 signature verifies with
  the committed fixture public key. This is disposable VM trust, not a release.
- Existing `make check` passed in the Nix build shell (including system/tooling,
  source-package roundtrip, docs and 1841 UI geometry checks). After review
  corrections, rerun the affected disposable gate probes, Python lint/compile,
  docs/path checks and source-package roundtrip.

The sample uses `/bin/sh`, a private data path and a real target ARM BusyBox
payload to exercise loader/dependency inspection. It is not mirror, file drop
or an accepted hardware app. No new app was installed on PW3 or claimed to
have passed the system-QEMU lifecycle gate in this change.

## Observed out-of-scope toolchain issue

Checking the retained core's `gw-system` as an app payload reports
`elf-float-mismatch`: its ELF flags say soft-float EABI, while the selected
Alpine musl loader says hard-float. The gate intentionally rejects that
combination. The previous baseline QEMU results only establish their exercised
paths; they do not settle general float ABI correctness. No core/toolchain
change was made here. New dynamic apps must use a matching target toolchain;
do not copy legacy compiler flags merely because an earlier executable ran.

## Boundaries and remaining work

A pass is **static only**. Reports keep QEMU/hardware flags false and identify
required lifecycle/resource, kernel/CPU, display/input and power checks.
Symbol versions, `dlopen`, language imports and arbitrary shell command
behaviour still require review and real execution. This authoring gate is not
a sandbox or a replacement for the existing signed device installer.

Mirror stays on the existing B26 hub roadmap. B04 now records a separately
switched-on Kindle HTTP file-drop session over Wi-Fi and Tailscale. The old
Hub no-inbound-server rule is scoped to that Hub client, not the separate
file-drop app. A userspace Tailscale inbound forwarding path, authenticated
real upload from another node, bounded storage and complete listener teardown
must be qualified before claiming VPN file drop works. Neither feature is
implemented by this gate.
