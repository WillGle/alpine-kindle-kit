# Reader application package contract

Status: **v1 implemented and verified on PW3 on 2026-10-31** for signed
per-user bundles (`.gwapp` + `.gwapp.sig`, RSA/SHA-256), installation,
upgrade with old-version retention, removal with data retention and the
Applications/App updates screens. See
`docs/evidence/pw3-app-installer-2026-10-31.md`. Strong sandboxing, distinct
app UIDs at runtime, catalog/online updates and `.apk` signature envelopes
remain future work; the text below records the wider contract and is not a
claim that every item is implemented.

## Implemented v1 format

```text
<name>.gwapp       gzip tar: manifest + payload/...
<name>.gwapp.sig   raw RSA-2048 PKCS#1 v1.5 signature over the bundle (SHA-256)
trust root         /etc/gleworks/app-keys/release.pem
USB drop           /mnt/us/gleworks/apps/
tools              gw-appctl (verify/install/remove/list/scan),
                   tools/build/gwapp.py (deterministic build + detached sign)
```

Installation verifies the signature before extraction, validates every tar
member name, rejects links/special files, parses and bounds the manifest,
checks `arch`/`profile`/`min_os`, verifies the entrypoint, extracts into a
staging directory on the app filesystem, then moves the `active` symlink. The
service exposes `APP COUNT`, `APP INFO <i>`, `APP SCAN`, `APP USB <i>`,
`APP INSTALL USB` / `APP UPDATE USB` and `APP REMOVE <id>` to the supervised
shell only.

Runtime/session contract (draft):
[APP-SESSION](../../docs/technical/superpowers/specs/2026-10-31-app-session-contract.md);
this file defines package contents only.

An application release consists of one Alpine-compatible `.apk`, a manifest
inside its signed package contents, and signed release metadata containing the
exact package SHA-256. The ASCII `manifest` at the package root has one
`key=value` pair per line, no whitespace around keys or values, and
comma-separated lists without spaces:

```text
id=koreader
version=2026.07.1
entrypoint=bin/koreader
formats=epub,pdf
permissions=display,touch
arch=armv7
profile=pw3-0kb-5.16.2.1.1
min_os=0.1
```

`id` is a lowercase ASCII identifier with digits, `-` and `.`; it is not a
path. `version` is an opaque, bounded release identifier; ordering and
downgrade policy must be defined by signed release metadata rather than string
comparison. `entrypoint` is a relative path within the extracted payload,
with no empty, `.` or `..` components. The installer chooses the UID and data
path; the manifest cannot choose a privileged UID or an arbitrary data path.
Unknown fields, permissions, architectures or profiles are refused.
The exact field limits are in `gw-app-manifest.h` and must be enforced before
using the values. Format and permission lists must contain unique known tokens.
All eight example keys are required; `formats` and `permissions` may have an
empty value. An optional `description` may contain printable ASCII. `min_os`
is a numeric version of up to three components, each at most 65535. The parser
rejects unknown and duplicate keys and must be checked before its result is
used.

Trust roots are configured locally and are not tied to a vendor server. Verify
the release metadata signature, package digest and `.apk` signature against
configured trusted keys before extraction. Release metadata must bind the app
ID, version, profile, architecture and digest. An unsigned digest on its own
is not authentication. The precise signature envelope and key rotation policy
remain to be specified and tested with the chosen Alpine package tools.

Extract only into a newly created, private version directory on the same
filesystem as the active link. Reject absolute or escaping paths, duplicate
entries, device nodes, FIFOs, set-ID bits and links that could escape the
version root; never execute package maintainer scripts. Resolve and validate
the entrypoint inside that root, check its type and executable permission, and
verify available space before staging and before activation. Refuse a package
whose safe extraction cannot be established with the selected tools.

The active version is selected only after verification and extraction complete;
failure leaves the previous version selected. This is a desired ordering, not
a claim of power-loss atomicity on every filesystem. Retain user data in a
separate app data directory across update and uninstall. Data deletion is a
separate explicitly confirmed operation. Confirm the eventual activation and
recovery protocol on the actual storage filesystem before describing it as
power-loss safe.

Applications are trusted software with a distinct UID and a read-only payload,
not a strong sandbox. The OS assigns display and touch only to the foreground
app and routes light, power and network requests through the service. Kernel
3.0 DAC enforcement, foreground transfer, crash recovery and read-only payloads
must be tested on PW3 before installation is exposed. Do not claim namespace
or seccomp isolation.
