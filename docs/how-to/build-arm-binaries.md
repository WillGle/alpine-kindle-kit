---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Build ARM binaries

Build `gw-system`, `gwctl`, `gw-appctl`, `gw-agent` and `gw-shell` for the PW3
from the repository root. The script is the supported entry point; it validates
the ARM toolchain and keeps host and ARM outputs separate.

## Procedure

```sh
# From a clean host environment, with no host CC override.
env -u CC -u STRIP -u CFLAGS \
  bash tools/build/build-arm-binaries.sh
```

The script writes ARM binaries under the build directory configured by the
script. Confirm the output hashes before deploying:

```sh
sha256sum <build-dir>/gw-system <build-dir>/gwctl \
  <build-dir>/gw-appctl <build-dir>/gw-agent <build-dir>/gw-shell
```

When building a full OS trial image, pass the same agent explicitly:

```sh
python3 tools/build/pw3-os-trial.py ... \
  --service <build-dir>/gw-system --shell <build-dir>/gw-shell \
  --agent <build-dir>/gw-agent --client <build-dir>/gwctl \
  --appctl <build-dir>/gw-appctl
```

## Rules

- Run the host and QEMU gates first:
  [`qemu-gate.md`](qemu-gate.md).
- Use `GW_ENABLE_HW_TRIAL=1` only for an explicitly labelled trial build. The
  default build is the accepted policy.
- Never replace a single paired bootstrap/guardian component on a running
  device. Deploy components that were built together.
- Record the resulting hashes in [`../status/STATUS.json`](../status/STATUS.json)
  when a build becomes the current deployed build.
