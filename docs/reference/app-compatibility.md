---
diataxis: reference
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# App compatibility gate

Selection clarified by the user: only integrate established community apps
with evidence of broad real use and practical usefulness, such as KOReader.
Agents primarily port, build, package and integrate these apps for ARMv7;
they do not create replacement product apps from scratch. Keep Alpine small;
dependencies belong to the app
unless a shared runtime has separately been qualified. Extend the existing
signed `.gwapp` flow rather than introduce another package manager.

Before technical qualification, record upstream source, pinned version or
commit, license, evidence of real community use and the user need served.
Popularity alone is not technical compatibility. Record agent changes as
minimal build patches, launchers or integration adapters. This selection
review is documented by the agent; the static checker does not measure
community adoption or usefulness.

The host gate checks the source against an explicit extracted target rootfs
before signing. A signed `compatibility.json` describes the app kind, private
library search directories, external commands and required kernel features.
It also declares network use as none, client or server. A server declaration
adds explicit listener/auth/upload/stop and optional VPN-inbound qualification
requirements; it does not silently enable a listener.
Inspect binaries without executing them: ARM ELF32 little-endian EABI, loader,
float ABI, ELF dependencies and script interpreters. Reject missing or
unsupported requirements with machine-readable reasons. Record hashes of
inspected source files and target dependencies for reproducibility.

Static acceptance does not prove symbol-version compatibility, dynamically
loaded plugins, language imports, CPU instruction compatibility, kernel calls,
resource isolation or working hardware. QEMU lifecycle/resource checks and PW3
display/input/power checks remain separate mandatory qualification stages.
Do not describe a static pass as full compatibility or security isolation.

CLI and foreground packages are eligible for static checking. Background
service packages are blocked until their concurrent lifecycle is supported.
Kernel-feature requests remain blocked without a qualified target capability
profile. No automatic installation of dependencies, arbitrary install hooks,
boot changes or on-device code execution occur in this host gate.

Legacy packages remain buildable through the existing path. New packages
containing `compatibility.json` must pass the gate during `gwapp.py build`.
This is an authoring gate, not a new device-side enforcement boundary: the
existing installer still enforces signature, manifest and extraction rules.

Approaches considered: put all runtimes into the OS (increases every install),
accept arbitrary Linux binaries (cannot satisfy PW3 architecture/kernel/UI
constraints), or retain a small core with checked app-local dependencies.
The selected approach is the third. Shared optional runtimes are future work,
not installed or claimed by this change.

Authoring instructions and the exact contract are in the
[agent app guide](../how-to/agent-apps.md).
