# Repository hardening plan — 2026-11-01

- Status: **implemented** (host-verified; no device write). See §7 execution
  record for commands and the remaining non-device limitations.
- Trigger: full-repository review on 2026-11-01. Findings are static-review
  results; none is a new device acceptance claim.
- Scope: repository state, app installer/signature, `gw-system` privilege and
  build config, `gw-shell` UI correctness, host tooling safety, build
  reproducibility, public source package, tests/CI and docs.
- Out of scope: new features, app runtime (tracked in
  [2026-10-31-app-runtime.md](2026-10-31-app-runtime.md)), paint-policy changes
  (tracked in [2026-10-31-eink-paint-quality.md](2026-10-31-eink-paint-quality.md)),
  other PW3 models or firmware profiles.
- Rule: phases are ordered. Do not start a later phase while an earlier exit gate
  is unmet; record the reason instead. Every security fix ships with a host test
  that fails before the fix.

## 1. Goals

1. The maintained source is actually in git, and the deployed build is
   reproducible from a tagged commit.
2. Installing an app cannot bypass signature verification, install setuid files,
   or exhaust the root filesystem.
3. Only privileged clients can trigger privileged `gw-system` actions.
4. Destructive host tools cannot fail open because Python ran with `-O`.
5. Known UI correctness bugs (wrong row selected, stale Home, stuck busy state)
   are fixed and covered by host tests.
6. The public source package contains everything the builder consumes.
7. A root-level check runner plus CI makes regressions visible before a device
   write.

## 2. Decisions

| ID | Decision | Reason |
| --- | --- | --- |
| H1 | Repository capture comes before any code change | Untracked core source is the only irreversible risk |
| H2 | Security fixes are host-tested with fixtures, not only code review | Current installer/signature paths have no tests |
| H3 | Installer verification and extraction use one private staged copy | Closes the path-based TOCTOU instead of narrowing it |
| H4 | `assert` is never used for a safety gate; use explicit `require()`/exceptions | `python -O` removes assertions |
| H5 | Destructive tools require an inventory plus explicit `--yes` (and offer `--dry-run`) | Removes fail-open deletion |
| H6 | The public source allowlist is generated from a dependency manifest and verified by rebuilding | Current hand list already omits required files |
| H7 | Builds become reproducible with `SOURCE_DATE_EPOCH`, fixed UUID/seed and sorted metadata | Current output changes per run |
| H8 | UI cleanup is split no earlier than needed; behaviour fixes land first | Avoid a large refactor hiding regressions |

## 3. Phases

### Phase 0 — Repository capture and hygiene

**Goal:** nothing is lost, and the current deployed build has a discoverable
source commit.

- [x] Commit all untracked source, in reviewable groups:
      `os/system/*`, `os/system/app-keys/`, `os/devices/pw3/overlay/etc/conf.d/`,
      `os/devices/pw3/patches/`, `apps/koreader/overlay/etc/gleworks-managed-reader.sh`,
      `apps/koreader/overlay/opt/koreader/plugins/gw-home.koplugin/`,
      `apps/koreader/overlay/opt/koreader/plugins/gw-theme.koplugin/`,
      `tools/build/{gwapp.py,pw3-data-image.py,pw3-os-trial.py}`,
      `tools/maintenance/*` and `theme/*` additions.
- [x] Commit the 34 modified tracked files as separate logical commits
      (UI, docs, tools, theme); do not mix generated evidence with behaviour.
- [x] Extend `.gitignore` for `os/system/gw-system`, `os/system/gwctl`,
      `os/system/gw-appctl`, `os/system/gw-data-mount`, `os/system/test-*`,
      and any other build output. Confirm `git status --ignored` is clean.
- [x] Push `main`; create a tag for the deployed source (record the deployed
      binary hashes `df2c9bd7…`, `aeedb481…` in the tag annotation).
- [x] Keep one current release under `dist/`; move older tarballs to an ignored
      historical location or delete after confirming the source is in git.
      Every retained release gets a `.sha256`.
- [x] Split `docs/BACKLOG.md`: keep the open-items table and a short index in
      place; move the 3,000+ lines of imported history to `docs/history/` with
      anchors preserved.
- [x] Add this plan to `docs/technical/README.md` so it is discoverable.

**Exit gate:** `git status` shows no untracked maintained source; a fresh clone
contains the full OS source; the deployed commit is tagged and pushed.

### Phase 1 — App installer and signature hardening (security)

**Goal:** a malicious or corrupted `.gwapp` cannot install code that the
signature check did not cover.

- [x] Stage the package into a root-owned `mkdtemp` directory with
      `O_CREAT|O_EXCL|O_NOFOLLOW`, copy it, `fsync` it, and verify **that copy**
      (`os/system/gw-apps.c:416-448`).
- [x] Run `tar -tzf`, `tar -tvzf` and `tar -xzf` only on the verified staged
      copy (or `/proc/self/fd/N`); remove the original path from the flow.
- [x] Reject setuid/setgid (`st_mode & (S_ISUID|S_ISGID)`), `st_nlink != 1`,
      and duplicate members (`os/system/gw-apps.c:224-263`).
- [x] Extract with `--no-same-owner --no-same-permissions`; clear set-ID bits
      after extraction as defence in depth.
- [x] Enforce decompressed size and member count **during** extraction; put a
      timeout on `tar` (`gw-apps.c:280-289,444-450`).
- [x] Verify the signature before `gw_apps_package_info()` metadata is used for
      update selection (`gw-system.c:1294-1315`, `gw-apps.c:313-331`).
- [x] Pin the trust key: owner/mode check, `O_NOFOLLOW`, key fingerprint in the
      image, absolute `libcrypto` path (`gw-signature.c:39-45,109-136`).
- [x] Make activation recoverable: rollback or resume on symlink/rename failure,
      `fsync` version directory and parent before switching `active`, and move
      appdata creation before activation (`gw-apps.c:489-518`).
- [x] Fix adjacent parsing defects: version arithmetic cap
      (`gw-apps.c:333-352`), uninitialised `waitpid` status (`:170-174`), NUL
      truncation before manifest validation (`:300-310`), `mkdtemp` EEXIST
      verification (`:406`), checked `dup2` (`:131-142`).
- [x] Add fixture tests: valid package, swapped-after-verify package, setuid
      member, hardlink, symlink, duplicate member, gzip bomb, oversized member,
      wrong key, wrong profile, version overflow.

**Exit gate:** every fixture test fails on the pre-fix code and passes after;
`make test` covers signature and extraction paths; no install writes outside the
verified staging tree.

### Phase 2 — Service and device security

**Goal:** privileged actions require privilege; boot and config state cannot be
corrupted by unprivileged or interrupted operations.

- [x] Move `TIME SYNC` behind `is_privileged()` and add a bounded ntpd timeout
      (`os/system/gw-system.c:1020-1038`).
- [x] Fix the default build: move `g_evt_clients`/`g_evt_clients_ready` outside
      `#if GW_LIFECYCLE_SPIKE` or default `GW_LIFECYCLE_SPIKE=1`; make CI compile
      both `0` and `1` (`gw-system.c:97-457,709-729`, `os/system/Makefile:26`).
- [x] Fix event broadcast: drop the client on partial write or persistent
      `EAGAIN`, or queue lines (`gw-system.c:720-731`).
- [x] Create device nodes with explicit owner/mode; stop making
      `/dev/fb0`, `/dev/input/event*`, `/dev/rtc0` world-readable
      (`gleworks-hardware-prepare.sh:12-24`, `gleworks-early.sh:24,39,68-70`).
- [x] Fix `return-control`: copy the launcher to root-owned tmpfs, verify the
      copy, exec the copy (`os/devices/pw3/bootstrap/return-control.in.conf:14-16`).
- [x] Config persistence: unique temp + cleanup, check `strtol` end pointer,
      propagate save errors (`gw-stage3.c:82-90,160-164` and callers).
- [x] Suspend policy: either honour `charging` or remove it from the documented
      contract (`gw-power-policy.c:18-29`, `gw-power-policy.h:49-53`).
- [x] Bootstrap: replace `access()` gates with `open(O_NOFOLLOW)` and `fstat`;
      add a guardian make rule for the generated `reset-token.h`
      (`bootstrap.c:22,262,288`).
- [x] Make the USB console banner honest: remove the "native root" challenge
      claim or make it an HMAC over a per-build secret
      (`gleworks-console.sh:27-31`).
- [x] Remove the vacuous `|| 1` assertion and add tests for authorisation,
      event broadcast, config round-trip and suspend policy
      (`test-stage3.c:71`).

**Exit gate:** unprivileged IPC cannot trigger ntpd; `GW_LIFECYCLE_SPIKE=0` and
`=1` both build; device nodes have intended modes; a root shell cannot be
obtained through the USB launcher TOCTOU.

### Phase 3 — UI correctness (small, high-value fixes)

**Goal:** no wrong-item action, no stale Home, no permanently busy control.

- [x] Initialise `response[0]='\0'` in every failure path (or guarantee
      `ipc_cmd` always NUL-terminates) — `gw-shell.c:3596-3608,3627-3638,3738-3758`.
- [x] Fix Wi-Fi row identity: store an owned SSID or ID, not a pointer into a
      shifting array; update it after delete/reorder —
      `gw-shell.c:2921-2926,2931-2952,2983-3021`.
- [x] Fix app row identity: re-index surviving `user_data` after
      `apps_delete_slot`, or store an app ID —
      `gw-shell.c:3496,3507-3525,3613-3624`.
- [x] Refresh Home after todo changes: call `apply_model()` from
      `todo_commit()` or when showing Home — `gw-shell.c:988-992,4193-4261`.
- [x] Add hard timeouts to connect/scan that clear pending state, re-enable
      buttons and show failure — `gw-shell.c:1309-1336`.
- [x] Clamp `APPS %d` to `GW_APP_MAX` before the IPC loop —
      `gw-shell.c:3439-3445`.
- [x] Keep dirty rectangles until EPDC submit succeeds; log/retry failure —
      `lv_port_epdc.h:753-754,764-812`.
- [x] Reset keyboard state in one `keyboard_dismiss()` used by every path —
      `gw-shell.c:2717-2730,4211-4215`.
- [x] Scale touch axes from `EVIOCGABS` min/max before passing to LVGL —
      `lv_port_input.h:106-163`.
- [x] Report todo save failures instead of claiming success —
      `gw-shell.c:818-834,988`.
- [x] Delete dead state and vacuous tests: `g_screen_full_paints`,
      `g_paint_policy`, `note_preview`, `gw_rgb332_to_gray`,
      `host_sim.c:985-990`.
- [x] Add host tests for each fixed bug (failure-path IPC, delete/reorder,
      Home-after-todo, busy timeout).

**Exit gate:** each new regression test fails before and passes after; manual
host-sim walkthrough of Home, Network, Apps and To-dos shows no stale or
wrong-target action.

### Phase 4 — Host tooling safety

**Goal:** no destructive operation can silently skip its checks.

- [x] Replace all 67 `assert` safety gates with explicit `require()` or raised
      exceptions, starting with `kindle-linux.py`, `clean-legacy-storage.py`,
      `verify-package.py`, `trim-reader-locales.py`, `stock-capture.py`.
- [x] Add `--dry-run` and an inventory plus `--yes` confirmation to
      `clean-legacy-storage.py` and `clean-amazon-state.sh`.
- [x] Reject absolute and `..` symlink targets during extraction/copy, and
      verify every write stays under the build tree
      (`pw3-build.py:43-50,150-155,183`, `pw3-os-trial.py:85,147-151`).
- [x] Make `Rescue` a context manager that unmounts on every failure path
      (`kindle-linux.py:86`).
- [x] Replace the fixed `/tmp` lock with a session-scoped `O_NOFOLLOW` lock
      (`pw3-install.py:516`).
- [x] Validate and quote `return_launcher_sha256` (`pw3-install.py:260`).
- [x] Use atomic writes for archives/screens/command files; add `--` before
      paths in `pw3-ext3-harden-copy.sh:23-28`.
- [x] Harden `gwapp.py`: `umask(0o077)`, refuse overwriting the public key,
      reject symlinks in the bundle, validate `manifest` against payload
      (`gwapp.py:48-57,84-87,116-118`).
- [x] Make `pw3_common.run()` include command/stdout/stderr in raised errors
      (`pw3_common.py:47-48`).

**Exit gate:** `python3 -O` changes no safety outcome; a malicious archive or
symlinked rootfs fixture is rejected; `Rescue` always unmounts.

### Phase 5 — Build reproducibility and public source

**Goal:** a published source kit rebuilds the same inventory and, where
possible, the same bytes.

- [x] Generate the public-source allowlist from a dependency manifest; add the
      missing `conf.d/gw-system`, `gleworks-managed-reader.sh`, both KOReader
      plugins, `os/system`, `os/ui`, `theme`, and the build tools
      (`tools/package/public-source.py:14-34`).
- [x] Add a roundtrip check: package → extract → build base and reader variants →
      compare file inventory and binary hashes.
- [x] Pin KOReader with a SHA-256; record toolchain, kernel config and tool
      versions in `candidate.json` (`apps/koreader/source.json`,
      `pw3-build.py:57-65`).
- [x] Set `SOURCE_DATE_EPOCH`, fixed file mtimes, fixed `mkfs.ext3 -U`/hash
      seed, sorted tar metadata and `gzip mtime=0`
      (`pw3-build.py:55,83-84,224-225`, `public-source.py:53-63`).
- [x] Resolve the 15% vs 25% image headroom mismatch and the stale 256 MiB
      references (`pw3_common.py:80-86`, `profile.json:10`,
      `docs/community/README.md:89`).
- [x] Document the intentional generated-source step (`theme/generate.py`) or
      write its output under `work/`; update `docs/guides/STRUCTURE.md:39-42`.
- [x] Add `-D_FORTIFY_SOURCE=2`, PIE/RELRO/NOW, and `-ldl` where `dlopen` is
      used (`os/system/Makefile:30-37`).

**Exit gate:** a fresh package at a fixed commit produces identical archive
hashes across two runs; extraction reproduces the tested file inventory.

### Phase 6 — Tests, CI and maintainability

**Goal:** regressions are caught before a device write, and the largest files
become reviewable.

- [x] Add a root `Makefile`/`nox` `check` target: `python -m compileall`,
      `sh -n`, `theme/generate.py --check`, public-source roundtrip,
      `os/system make check` and `make test`, `os/ui make host-sim` + tests,
      plus an ARM build when the toolchain is present.
- [x] Add CI (or a pre-push hook when CI is unavailable) running that target.
- [x] Add a root `LICENSE`/`COPYING` and SPDX headers to tools missing them
      (`verify-package.py`, `kindle-linux.py`, `gwapp.py`, `usb_console.py`,
      `theme/generate.py`, most maintenance scripts).
- [x] Split `gw-shell.c` (4,692 lines) into `gw_todo_store`, `gw_ipc_client`,
      `gw_format`, `gw_app_model`, `gw_net_model`, and per-screen modules;
      replace globals with one context object and remove the
      `#include "gw-shell.c"` test seam (`host_sim.c:13`).
- [x] Move `static` style/string tables out of headers into `.c` files with
      `extern` declarations (`gw_ui_style.h:19-35,261-263`,
      `gw_ui_strings.h:176-190`).
- [x] Extract EPDC dirty/waveform policy into a pure `gw_epdc_policy.c` with a
      fake ioctl and host tests (`lv_port_epdc.h:379-828`).
- [x] Centralise remaining strings, geometry tokens and hardcoded paths;
      fix the Sunday-first vs Monday-first weekday table
      (`gw_ui_strings.h:176-182`).
- [x] Remove duplicated safety helpers (`digest`, `syncdir`, token, image
      sizing) into `tools/lib` and use them everywhere.

**Exit gate:** CI is green on a clean clone; test count and coverage of the
installer/UI failure paths increase; `gw-shell.c` is under ~1,500 lines with no
behaviour change beyond Phase 3.

## 4. Finding traceability

| Finding (from 2026-11-01 review) | Phase |
| --- | --- |
| Core source untracked / branch ahead / stale dist | 0 |
| Signature TOCTOU | 1 |
| setuid/hardlink/decompression | 1 |
| Unverified manifest for updates | 1 |
| Trust key / libcrypto loading | 1 |
| Activation not recoverable | 1 |
| TIME SYNC before privilege gate | 2 |
| `GW_LIFECYCLE_SPIKE=0` build broken | 2 |
| Event broadcast truncation/stuck slot | 2 |
| Device node modes | 2 |
| `return-control` exec TOCTOU | 2 |
| Config temp/parse/error handling | 2 |
| Suspend policy vs contract | 2 |
| Uninitialised UI response | 3 |
| Wi-Fi/app stale identity | 3 |
| Home not refreshed after todo | 3 |
| Connect/scan stuck busy | 3 |
| EPDC dirty set dropped on error | 3 |
| UI touch scaling | 3 |
| `assert` safety gates | 4 |
| Destructive cleanup without confirmation | 4 |
| Build-input symlink escape | 4 |
| Source allowlist incomplete | 5 |
| Unpinned non-Alpine inputs | 5 |
| Non-reproducible builds | 5 |
| No CI / root test runner | 6 |
| `gw-shell.c` monolith | 6 |

## 5. Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Committing everything at once hides the deployed source | Group commits by subsystem; tag the deployed binaries |
| Installer changes break signed-package compatibility | Keep the `.gwapp` format; add fixture packages for old and new keys |
| Extraction restrictions reject legitimate packages | Validate against both existing released packages and new fixtures |
| `GW_LIFECYCLE_SPIKE=1` default changes host test behaviour | Compile both variants in CI; keep tests explicit |
| UI identity fix changes host expectations | Add failing regression tests before the fix |
| Symlink hardening breaks upstream Alpine extraction | Allow safe relative links; reject only absolute/`..`/escaping targets, with a fixture |
| Reproducibility work changes image hashes | Treat new hashes as a new release; keep old tags |
| Refactor in Phase 6 hides behavior changes | No Phase 6 refactor until Phase 3 tests are green |

## 6. Definition of done

1. A fresh clone at the tagged commit builds the deployed OS variant.
2. A malicious `.gwapp` fixture cannot bypass signature, install set-ID files,
   or exhaust the root filesystem.
3. Unprivileged local clients cannot trigger privileged service actions.
4. Running any tool under `python3 -O` produces the same safety decisions.
5. Phase 3 regression tests cover every listed UI bug.
6. The public source package rebuilds the tested inventory.
7. CI (or the documented pre-push hook) is green.
8. Remaining limitations are written down; nothing is marked verified without
   evidence.

## 7. Execution record

- 2026-11-01: plan created from the full-repository review; approved, no phase
  started, no source/device/boot/release change made. Static review only; this
  environment has no `gcc`/`make`, so no build or host suite was executed.

- 2026-11-01 (implementation): all Phase 0 capture/hygiene items committed and
  pushed (`main`, tag `pw3-deployed-20261101`); `.gitignore` covers every system
  and UI build output; `dist/` retains one release with a verified `.sha256`;
  `docs/BACKLOG.md` history moved to `docs/history/BACKLOG.md` with anchors.
  Phase 1 landed a private staged copy, fsync, verification of that copy,
  set/ID and duplicate rejection, `--no-same-owner/--no-same-permissions`,
  extraction timeouts and size gates, verified-manifest selection, key
  fingerprint pinning, recoverable activation, and signed fixtures. Phase 2
  gated `TIME SYNC`, built both lifecycle variants, fixed event writes, device
  modes, the return-control copy, config durability, charging policy,
  bootstrap `access()` gates and the console banner. Phase 3 fixed IPC
  response resets, SSID/app identity, Home refresh, scan/connect timeouts, app
  count clamping, EPDC dirty retention, keyboard dismissal, touch scaling and
  todo-save reporting with host regressions. Phase 4 replaced all 67
  `assert` gates (`python -O` suite passes), added inventories/`--yes`/
  `--dry-run`, symlink-target rejection, a `Rescue` context manager,
  session-scoped locking, atomic writes and bundle hardening. Phase 5 replaced
  the hand allowlist with `tools/package/source-manifest.json` (195 files,
  deterministic roundtrip), pinned the KOReader tree hash, set
  `SOURCE_DATE_EPOCH`/fixed UUIDs/mtimes, fixed the 25% headroom wording and
  added hardening flags. Phase 6 added `make check`, CI, a pre-push hook, root
  `LICENSE` and SPDX headers, moved header tables into `.c` owners, extracted
  pure EPDC policy tests, fixed the Sunday-first weekday table, split
  `gw-shell.c` to 1,026 lines across named screen/model fragments, and
  centralised `digest`/`syncdir`/atomic writes.

  Evidence: `make check CC=gcc` passed (`CHECK PASSED`) inside a nix-shell with
  gcc 15.3.0, busybox and OpenSSL; system tests
  (`test-stage3`, `test-power-policy`, `test-power-io`, `test-apps`,
  `test-apps-install`), tooling tests under `python3 -O`, EPDC policy tests,
  theme check, public-source roundtrip and the 2,636-check host-sim suite all
  passed. `git status` is clean apart from ignored build/work paths.

  Structural follow-up completed: all shell mutable state now lives in one
  `gw_shell_state_t g_shell` object declared by `gw_shell_state.h`; shared
  types live in `gw_shell_types.h`; EPDC panel state has one owner in
  `lv_port_epdc_state.c`; and `host_sim.c` no longer includes `gw-shell.c`.
  The suite links `gw-shell.c` as its own translation unit through
  `gw_shell_test.h`. `gw-shell.c` is 1,026 lines and the named `.inc` screen
  fragments are included in that one production translation unit.

  Additional evidence produced after the first pass:
  - `os/ui/tests/prefix_regression.py` rebuilds mutated copies that restore
    five pre-fix behaviours (uninitialised IPC response, missing Home refresh,
    non-reindexed app rows, missing scan timeout, position-based network
    selection). Each mutated build fails its named regression; the clean build
    passes. Output: `pre-fix regression evidence: PASS`.
  - `tools/build/roundtrip.py` rebuilt the real private base and reader
    candidates twice at the same output path using the captured Alpine,
    modules, p1 backup, launcher, kernel build and KOReader bundle. Every
    payload hash matched; resulting images were byte-identical:
    base `d976739d520b9f6bd9bec5137dc8b551304d5cdeddd8a9159f12adcf20616e34`,
    reader `ffa2b9d130c3d3de0fc6d848ca1497761e065f4407b68bd39e802f1a9350d3c5`
    (128 MiB, `SOURCE_DATE_EPOCH=0`, `GW_BUILD_SEED=roundtrip`).
  - Public-source roundtrip now covers 195 files and remains deterministic.
  - ARM device binaries built from the hardened source and staged under
    `work/build/arm-hardening-20261101/` (ignored): `gw-system`
    `b9d861f2…`, `gwctl` `e83ba414…`, `gw-appctl` `0938cada…`, `gw-shell`
    `a7682079…`. They are static ARM `EXEC` files (no dynamic loader on the
    deployed image); `gw-appctl` was smoke-executed under `qemu-arm` and
    printed its usage. No device write was performed.

  Limitations recorded honestly: no device write was performed; the ARM
  runtime was exercised only through the candidate builder/QEMU checks, not on
  physical PW3 hardware. The KOReader pin is a tree hash of the locally
  reviewed clean bundle. Absolute or `..` symlink targets are allowed only
  when they resolve inside the archive root (real Alpine rootfs links require
  this); escaping relative targets are still rejected.
