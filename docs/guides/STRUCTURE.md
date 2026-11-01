# Repository and working-directory layout

The repository contains maintained source, documentation and historical project
source. Downloads, third-party checkouts, toolchains, builds, filesystem images,
private device data and generated release packages live outside maintained source.

```text
os/
  alpine/                 pinned Alpine input and minimal root overlay
  devices/pw3/            exact profile, bootstrap, guardian and PW3 overlay
apps/
  koreader/               pinned reader input and optional reader overlay
tools/
  build/                  offline candidate builder
  install/                private control setup and guarded first installation
  maintenance/            installed-system USB administration and captures
  package/                public source packaging and package verification
  lib/                    shared host-side Python helpers
docs/
  guides/                 current workspace, build and operation instructions
  community/              standalone English installer/contributor instructions
  technical/              technical analysis, specs and plans
  evidence/               acceptance summaries and redacted host evidence
  BACKLOG.md              single append-only history/backlog
archive/
  amazon-appliance/       previous Amazon/JB application layer source
  experiments/            historical probe/boot/build source, not the active path
work/                    local, ignored; may be placed outside the repository
  downloads/              original upstream archives and acquisition records
  sources/                third-party source trees and clean reader bundle
  toolchains/             ARM compiler, musl prefix and host libraries
  build/                  private build output and prepared dependency builds
  images/                 retained clean/reference image artifacts
  private/                keys, paired identities, device backups and captures
  archive/                historical runtime logs, emulator disks and old packages
dist/                    newly generated source packages and checksums; ignored
```

`os/` and `apps/` contain no host tooling or generated tokens. Runtime files
installed on the device are supplied by these overlays and device sources.
Host tools read source relative to their own location, never the current
working directory. One intentional exception writes tracked source:
`theme/generate.py` renders `theme/tokens.json` into `os/ui/gw_theme_tokens.h`
and the KOReader `gw_theme.lua` adapter. Run `theme/generate.py --check` in CI
before committing; do not edit those two generated files by hand.

The default build variant is **base**: native Alpine, PW3 runtime and physical
USB administration, without KOReader, reader service or glibc reader libraries.
`--variant reader --reader /path/to/clean-koreader` explicitly adds the optional
app layer. Each new candidate has its own paired bootstrap/guardian/control
identity and manifest; image and components must remain together.

## Work directory

Tools that choose local working-output defaults accept `--work-dir PATH`.
The default is `work/` next to `os/`, `apps/` and `tools/`. An absolute external
path can be used, for example `/data/kindle-work`, without moving source.
Explicit `--output`, `--session`, backup/evidence or archive paths override the
corresponding defaults. Package output defaults to repository `dist/` and
package verification only writes when given `--extract-to`.

Examples, from any current directory:

```sh
python3 /path/to/kindle/tools/build/pw3-build.py --help
python3 /path/to/kindle/tools/install/pw3-install.py \
  --work-dir /data/kindle-work status
python3 /path/to/kindle/tools/maintenance/kindle-linux.py \
  --work-dir /data/kindle-work --help
```

The default installer session is `WORK/private/sessions/pw3`; it must be new
for `init`. Use explicit `--session` for multiple devices or installation
attempts. The control bundle defaults to `WORK/private/control`; a build
candidate defaults to a new `WORK/build/pw3-VARIANT-ID` directory. Inspection,
deployment evidence and screenshots default beneath `WORK/private/`.
Private session directories remain mode 0700; never publish the work tree.

## Historical data and migration

All pre-reorganization files and links were retained. Historical scripts,
logs, captures and release archive contents remain snapshots: their internal
absolute paths may describe the old workspace and are not current commands.
The original prepared legacy kernel output is retained under
`work/archive/experiments/abi-probe/build`; use a separate adapted copy under
`work/build/` when preparing dependencies for a new build.

Current guides follow the new layout. Historical documents and BACKLOG retain
their original statements; use `docs/evidence/layout-moves.json` to locate old
paths. Entries in that map are longest-prefix moves; strip an obsolete initial
`ours/` when looking up older workspace paths. Private file inventories and
pre-edit source copies remain under `work/private/migrations/`.

Public source packaging uses an explicit allowlist. It includes maintained
OS/app/tool source, profiles and standalone community documentation, excluding
`work/`, `archive/`, existing release packages, generated keys/tokens, images
and captured vendor components. Moving source or rebuilding on the host does
not constitute new device, reader, recovery or first-install acceptance.
