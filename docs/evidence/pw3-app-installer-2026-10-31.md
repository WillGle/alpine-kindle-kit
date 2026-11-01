# PW3 — signed app installer, manager and update UI, 2026-10-31

Status: end-to-end verified on the reference PW3. A signed bundle was
installed, upgraded while retaining the old version, removed with app data
retained, and rejected after tampering or when unsigned. The Applications and
App updates screens drove the same operations through the real UI.

## Host tests

```text
$ make -C os/system CC=gcc test
Stage 3 tests: PASS
Power policy tests: PASS
Power I/O tests: PASS
App manager tests: PASS
```

`test-apps.c` covers dotted-version ordering (`1.10.0` > `1.9.0`, `1.0` ==
`1.0.0`, prerelease ordering), manifest compatibility and rejection of missing
keys, duplicates, absolute entrypoints and `..` traversal.

## Trust root

| Item | Value |
| --- | --- |
| Public key | `/etc/gleworks/app-keys/release.pem` |
| Algorithm | RSA-2048, PKCS#1 v1.5, SHA-256 (OpenSSL libcrypto 3 on device) |
| Public key file SHA-256 | `d92c1bfbf3f052f8fbcc418180ef0ec03cf3781fe0f6a2269505cfa83da89bef` |
| SPKI DER SHA-256 | `0fac2d52121e81694191663ac8db89e4973dacc69ac63ecfe137c6cbf1a92c98` |
| Private key | host-only `work/private/app-keys/release.key`, mode 0600, never in the image |

The device has libcrypto but no OpenSSL headers or CLI, so
`os/system/gw-signature.c` declares the small EVP surface it needs and loads
`libcrypto.so.3` at runtime. A missing library returns `libcrypto-unavailable`
instead of crashing or skipping verification.

## Package format

`.gwapp` is a deterministic gzip tar containing `manifest` and the payload
tree. `<name>.gwapp.sig` is a detached raw RSA/SHA-256 signature over the exact
bundle bytes.

Manifest fields follow `os/system/gw-app-manifest.h`:

```text
id=gwtest
version=1.0.0
entrypoint=payload/hello.sh
formats=
permissions=
arch=armv7
profile=pw3-0kb-5.16.2.1.1
min_os=0.1
description=Installer self-test
```

Build/sign on the host:

```sh
python3 tools/build/gwapp.py build --source /tmp/gwapp-src --out /tmp/gwtest.gwapp
```

## Verification and tamper rejection

```text
$ gw-appctl verify /mnt/us/gleworks/apps/gwtest.gwapp
OK

$ cp gwtest.gwapp /tmp/tampered.gwapp; echo x >> /tmp/tampered.gwapp
$ gw-appctl verify /tmp/tampered.gwapp
ERR signature-invalid

$ cp gwtest.gwapp /tmp/unsigned.gwapp
$ gw-appctl install /tmp/unsigned.gwapp
ERR signature-invalid
```

A correctly signed but malicious archive is still refused before extraction:

```text
$ gw-appctl install /tmp/evil-link.gwapp        # payload/link -> /etc
ERR link-or-special-file-rejected

$ gw-appctl install /tmp/evil-hard.gwapp        # payload/../../evil
ERR unsafe-or-incomplete-package
```

The second case is detected because both tar listings are captured with
stderr; BusyBox's `removing leading 'payload/../../'` warning becomes a
malformed listing line instead of being silently normalised.

## Install, upgrade and rollback retention

```text
$ gw-appctl install /mnt/us/gleworks/apps/gwtest.gwapp
OK
$ gw-appctl list
APP gwtest 1.0.0 Installer self-test
APP END
$ /var/lib/gleworks/apps/gwtest/active/payload/hello.sh
hello from gwtest 1.0.0
```

Upgrading from the **App updates** screen (UI taps through `/run/gw-shell.diag`):

```text
$ gwctl ui-nav UPDATE
$ gwctl ui-tap 280 520      # Check USB
$ gwctl ui-tap 760 520      # Install update
$ gw-appctl list
APP gwtest 1.1.0 Installer self-test
APP END

$ ls -l /var/lib/gleworks/apps/gwtest
drwx------ 1.0.0
drwx------ 1.1.0
lrwxrwxrwx active -> 1.1.0
$ /var/lib/gleworks/apps/gwtest/active/payload/hello.sh
hello from gwtest 1.1.0

GW_SYSTEM: USB app install: 1/1 (none)
GW_SYSTEM: USB app install: 1/2 (version-already-installed)
```

The second line is the upgrade scan seeing the already-installed 1.0.0 bundle
next to the new 1.1.0 bundle; the active link moved only after the new bundle
was fully verified, extracted and checked.

Directory order can never downgrade an app. With both 1.0.0 and 2.0.0 bundles on
USB, the service reads the manifests, keeps only the newest per app ID, and
installs that one:

```text
$ gw-appctl list
APP gwtest 2.0.0 Installer self-test
$ ls /var/lib/gleworks/apps/gwtest
2.0.0  active
GW_SYSTEM: USB app install: 1 installed, 1 skipped, 0 failed (none)
```

`gw_apps_version_compare` treats missing trailing components as zero
(`1.0` equals `1.0.0`) and a `-prerelease` suffix as older than the release.

## Remove keeps data

From the **Applications** screen:

```text
$ gwctl ui-nav APPS
$ gwctl ui-tap 930 285      # Remove first row
$ gw-appctl list
APP END
$ ls -ld /var/lib/gleworks/appdata/gwtest
drwx------ ... /var/lib/gleworks/appdata/gwtest
```

`gw-appctl remove koreader` returns `ERR builtin-app`: the built-in reader
cannot be uninstalled by the app manager.

## UI screens

Host renderer (`os/ui`, real widget tree, in-memory framebuffer):

```text
$ ./host-sim /tmp/gw-ui-preview
geometry checks: 760 total, 0 failed
screenshots: .../{home,settings,calendar,todos,network,apps,update}.ppm
```

Screens added:

- **Settings → Software → Applications** opens the installed-app list, with
  Install from USB, Refresh, built-in Reader launch, and per-app Remove.
- **Applications → App updates** opens the update screen with Check USB,
  Install update, the bundle list and the signature policy note.
- Both screens are reachable from Settings and the diagnostic `NAV APPS` /
  `NAV UPDATE` commands used for the acceptance taps.
- Host evidence: `docs/evidence/pw3-app-ui-2026-10-31/` (`settings.png`,
  `apps.png`, `update.png`, `geometry.json`).

## Safety boundaries

- Signature is checked before any extraction.
- Every tar member name is validated (relative, no `..`, bounded characters,
  bounded entry count/size); extracted links and special files are rejected.
- The installer refuses incompatible `arch`/`profile`/`min_os`, an invalid
  entrypoint, a missing/oversized manifest and an already-installed version.
- Staging is created under the app root on the same filesystem; activation is a
  single symlink rename after the version directory is complete.
- Old versions and app data are retained on upgrade; uninstall removes payload
  only. The service requires the supervised shell PID and root for install,
  update and remove commands.
