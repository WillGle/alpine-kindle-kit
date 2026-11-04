---
diataxis: how-to
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Integrate a community app with an agent

Use this workflow to port and package established community apps for the
Alpine ARMv7 core. Select apps with demonstrated real-world usefulness and
broad community use, such as KOReader. Agent-written code supplies the build,
launcher and necessary integration; do not recreate an existing app from scratch.
The [compatibility contract](../reference/app-compatibility.md) defines the
limits. It does not make arbitrary desktop Linux binaries PW3-compatible.

## Select upstream before coding

Record the upstream project URL, pinned release/commit, license, evidence of
real community adoption and the need it solves. Explain why the existing app
fits the device and identify the minimum porting changes. Do not use star
counts alone as proof or call an unresearched candidate community-proven.
This selection review precedes the technical gate; the checker cannot certify
an app's popularity or usefulness. The minimal example below is a packaging
fixture, not a proposed product app.

## Give the agent this contract

- Keep app code, libraries and resources inside its versioned `.gwapp`.
- Use `$GW_APP_DATA` for writable state; never write boot files or core libraries.
- Build for ARMv7-A without NEON; match the target libc and float calling ABI.
  Static binaries are also eligible. Desktop X11/Wayland apps need a separately
  qualified display/runtime adaptation; do not add a desktop stack to the core.
- Prefer native compiled code or the existing `/bin/sh`. If another runtime is
  required, package it explicitly and check its dependency closure. Shared
  optional runtimes are not implemented by this gate.
- Exit cleanly on SIGTERM; stay in the foreground for supervision. Declare all
  external commands, including commands invoked by shell scripts. Avoid shell
  lookups of undeclared tools. Private libraries must match the launcher's
  explicit `LD_LIBRARY_PATH`.
- Do not add install hooks, execute unreviewed input or silently download
  dependencies. Ask for network/display/touch permissions only when needed.
- Include the declaration below. Run the static gate, then the ARM QEMU
  lifecycle checks, before requesting any physical test.

## Minimal source tree

```text
my-app/
  manifest
  compatibility.json
  payload/
    run
```

`manifest` (existing device format):

```ini
id=my-app
version=1.0
entrypoint=payload/run
formats=
permissions=
arch=armv7
profile=pw3-0kb-5.16.2.1.1
min_os=0.1
```

`compatibility.json` (all six fields required; no unknown/duplicate fields):

```json
{
  "schema": 1,
  "kind": "cli",
  "network": "none",
  "library_dirs": [],
  "commands": ["/bin/sh"],
  "kernel_features": []
}
```

`payload/run`, executable mode `0755`:

```sh
#!/bin/sh
set -eu
: "${GW_APP_DATA:?app data directory required}"
printf 'Hello\n' > "$GW_APP_DATA/hello.txt"
```

Kinds are `cli`, `foreground`, `service`. `service` currently blocks signing:
concurrent background lifecycle is unqualified. Network is `none`, `client`
or `server`; the latter two require `network` in manifest permissions.
`library_dirs` are relative package directories, such as `payload/lib`.
`commands` are absolute guest executable paths, not host commands. Each list
allows up to 32 unique nonempty strings of at most 255 characters. Nonempty
`kernel_features` block signing until a target capability profile can verify
these requirements; do not omit a real requirement to get a pass.

This first gate rejects embedded ELF RPATH/RUNPATH and `/usr/bin/env` shebangs
with explicit adaptation messages. Use a reviewed launcher with explicit
interpreter/library paths. It inspects every packaged ELF and shebang, including
private libraries, without calling `ldd` or executing the package.

## Check, package and qualify

Use an extracted target rootfs, preserving guest symlinks. For host experiments
this can be the `root` directory produced by the
[core image procedure](alpine-core-qemu.md). A VM target does not substitute for
a restored PW3 target.

```sh
nix-shell tools/build/nix-shell.nix
python3 tools/build/gwapp.py check \
  --source work/my-app --rootfs work/target-root --os-version 0.1 \
  > work/my-app-static.json
python3 tools/build/gwapp.py build \
  --source work/my-app --rootfs work/target-root --os-version 0.1 \
  --out work/my-app.gwapp --key work/private/app-keys/release.key
```

The checker exits `0` only for `static-passed`; it exits `1` and emits JSON
errors with stable `code`, `path` and `detail` fields when blocked. Fix the
reported incompatibility and rerun. Build repeats the check before signing
whenever `compatibility.json` is present; omitting `--rootfs` then fails.
Legacy bundles without the declaration remain supported by the old builder.
The device does not enforce this JSON declaration; signed package validation
and manifest compatibility remain the device's enforcement boundary.

Record the gate JSON (source/target dependency SHA256s), exact root image,
package hash, toolchain and runtime logs together. Source and rootfs must be
stable private inputs during checking/building; hashes are evidence, not an
adversarial concurrent-edit guarantee. Keep the private signing key private;
the committed fixture key is only for disposable VM probes.

A static pass always reports QEMU and hardware verification as false. Next:

1. Boot the target in ARM QEMU. Install/run/stop/remove the app, exercise the
   real feature rather than just `--version`, then crash/update/manual rollback.
2. Record peak RAM, CPU, process/fd cleanup, preserved data and Home recovery.
3. After UART restoration, verify CPU/kernel compatibility, display/input,
   power/suspend and app handoff on PW3. Follow the
   [safe device procedure](safe-reinstall.md); never deploy the VM root.

The existing QEMU core harness proves platform fixtures; it does not
implicitly qualify an arbitrary new app or set the checker's verification
flags. Runtime symbol versions, language imports, dynamically loaded plugins
and actual CPU instructions remain runtime/build-review checks.

## Planned mirror and file drop apps

Research established community implementations for both features before
adding product code. Select and pin a suitable upstream component, then add
only the required port, launcher or OS adapter. No mirror/file-drop upstream
has been selected or qualified by this document. Existing hub code is a
retained prototype, not a reason to build another app from scratch.

Mirror is already B26 / [hub roadmap P3](../status/ROADMAP.md): PC → hub →
Kindle first, with e-ink refresh policy. Its client needs `foreground`,
`network: client` and display/input qualification. It must release display
ownership and stop session processes when closed. Kindle → PC is later work.

B04 now includes an optional **Kindle-hosted HTTP file drop**, enabled only
for an explicit session, over Wi-Fi directly or through Tailscale. This is
separate from the hub client's existing outbound-only protocol. Its contract
is `foreground`, `network: server`, permission `network`, with a launcher that
keeps the service supervised until the session ends. Do not disguise an
always-on daemon as a foreground app.

Before acceptance, verify authenticated uploads, bounded request/file sizes,
path traversal/symlink rejection, temporary-file cleanup, atomic publication,
full-disk behaviour, network loss, stop/exit and suspend. Books go to the
OS-approved document storage; never write directly to an exported FAT volume
or let the sender select arbitrary filesystem paths. File drop does not
install apps or execute uploaded files.

PW3 lacks TUN in the retained kernel. Userspace Tailscale does not create a
normal tailnet interface for binding an HTTP server, and an outbound SOCKS5
proxy does not provide inbound file drop. Qualify a supported inbound TCP
forward/Serve path from the tailnet into a loopback HTTP listener. Include the
required client/control tools; the previously exported `tailscaled` binary
alone does not prove this path. Test a real upload from a second tailnet node,
reject unauthorised requests and confirm no listener/session process remains
on exit. VPN transport does not replace HTTP application authentication.

Neither mirror nor file drop is implemented or hardware-qualified by adding
this authoring gate. Their checks remain separately tracked in B26 and B04.
