---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Package the public source

The public package is an allowlisted source archive, never a copy of the working
directory or a binary release.

## Build and verify

```sh
python3 tools/package/public-source.py <new-source.tar.gz>
python3 tools/package/verify-package.py <new-source.tar.gz> \
  --extract-to <new-empty-check-directory>
```

The package includes maintained `os/`, `apps/`, `tools/` and documentation. It
excludes `work/`, `archive/`, images, keys, tokens, captures, book libraries and
release artifacts. Read [`../reference/licenses.md`](../reference/licenses.md)
before distributing any binary or vendor component.

## Checks

```sh
python3 tools/package/public-source.py --check
make check
```

The deterministic roundtrip check is part of `make check`. A successful package
does not grant redistribution rights for captured vendor libraries or firmware.
