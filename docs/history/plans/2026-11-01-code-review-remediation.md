---
status: historical
date: 2026-11-01
superseded_by: ../../status/CURRENT.md
---

# Code review remediation plan — 2026-11-01

- Status: **A–E code complete; new-UI device verification pending.** All
  implementation checkboxes are closed and the host gate passes. The original
  device evidence did not deploy the post-remediation UI binary, so Apps,
  Network, selected-Wi-Fi, lock/reader handoff, and EPDC/paint behavior still
  need a current `gw-shell` device run before claiming full device completion.
  Workstream F was moved to its own plan (B24). D3 per-app isolation is
  explicitly deferred to the app-runtime project.
- Trigger: codebase review on 2026-11-01. Findings are static-review results;
  none is a device acceptance claim.
- Scope: 19 correctness/safety findings plus an organization/performance
  workstream (E) covering shared atomic-file code, toolchain setup, device
  preparation, command dispatch, IPC batching, UI translation units, EPDC
  dirty-region handling and test architecture. Workstream F is appended for
  tracking a separate UI/UX hierarchy pass; it is not part of A–E acceptance.
- Out of scope: new features, app runtime implementation (see
  [2026-10-31-app-runtime.md](2026-10-31-app-runtime.md)), paint-policy
  redesign beyond the listed cleanup, other PW3 models or firmware profiles.
  Workstream F requires its own design sign-off and must not be mixed with
  A–E remediation commits.
- Rule: one finding/work item per branch and PR. Do not mix behavior changes
  with refactoring. Default builds remain hardware-stub; hardware-touching
  builds require an explicit trial/release flag and recorded evidence.
- Backlog entries: [../../../BACKLOG.md](../../../BACKLOG.md), item **B23**
  (A–E remediation) and item **B24** (Workstream F UI/UX hierarchy).

## 1. Traceability

| Finding | Task | Summary |
| --- | --- | --- |
| 1 | A1 | Three Python tools raise `NameError` (`F821`); add lint gate |
| 2 | A2 | Open Wi-Fi networks cannot connect |
| 3 | A3 | Network row user-data selects the wrong SSID after delete/reorder |
| 4 | A4 | “Install selected package” installs every USB package |
| 5 | A5 | uinput power tool writes a 24-byte event instead of 16 |
| 6 | B1 | UI implementation is one TU split mid-function across `.inc` files |
| 7 | B2 | Dead feature flags, unreachable paint paths, fake tests |
| 8 | B3 | `SPIKE` flags bypass the `ACCEPTED` hardware gate |
| 9 | B4 | Crash handler calls non-async-signal-safe functions |
| 10 | B5 | Package install lacks file-data fsync and has size/TOCTOU gaps |
| 11 | B6 | Public source leaks test private keys and omits build entry points |
| 12 | B7 | CI misses reader shell scripts, no Python lint, unpinned actions |
| 13 | C1 | Command socket is world-writable; PID auth is not a sandbox |
| 14 | C2 | Todo writer and root-run logger use unsafe/predictable temp files |
| 15 | C3 | Guardian module-ref leak, shutdown `ENOENT` hang, PID1 `halt()` spin |
| 16 | C4 | Hardcoded `/nix/store`/`/home/will`, unpinned `npx`, `.` in library path |
| 17 | D1 | Reader hardcodes `/dev/input/event1` |
| 18 | D2 | Reader forks `gwctl` every two seconds |
| 19 | D3 | `CAPS apps=1` but installed apps cannot launch |
| — | E1 | One shared atomic-file implementation |
| — | E2 | One toolchain/build environment |
| — | E3 | One device-preparation script |
| — | E4 | Command table with centralized authorization |
| — | E5 | Batch IPC; remove N+1 app/network calls |
| — | E6 | Persistent/multi-command IPC connection |
| — | E7 | Collapse paint/profile/policy stack; fix EPDC dirty batching |
| — | E8 | Organize tests around behavior, not implementation |
| — | F1–F10 | UI/UX hierarchy pass; separate product workstream, not started |

## 2. Workstream A — P0 correctness

### A1. Fix the three `F821` tools and add a lint gate

**Changes**
- [x] `tools/build/pw3-os-trial.py`: pass `epoch` into `build(args, epoch)`
      from `main()`.
- [x] `tools/maintenance/kindle-linux.py`: replace undefined `call(...)` with
      `pw3_common.run(...)` in `clean_image()` and `Rescue.eject()`.
- [x] `tools/maintenance/stock-capture.py`: import `write_atomic` from
      `pw3_common`.
- [x] Add pinned `tools/requirements-dev.txt` containing Ruff.
- [x] Extend `tools/check.py` to run `ruff check --select F821,E9` over tracked
      Python.
- [x] Add `--help` smoke tests for the three entry points.

**Tests / exit gate**
- [x] Ruff `F821` returns zero.
- [x] All three `--help` commands exit 0.
- [x] CI fails when an undefined name is reintroduced.

### A2. Support open Wi-Fi networks

**Changes**
- [x] `os/system/gw-network.c`: accept `password_len == 0` and write a
      `key_mgmt=NONE` network block without `psk`.
- [x] Keep 8–63 bytes for non-empty passwords; reject 1–15 hex characters.
- [x] Make `GW_WIFI_DIR` overridable for tests.
- [x] Keep `NET CONNECT <ssid_hex> [pass_hex]`; the UI already sends an empty
      password for open networks.
- [x] Add a host test for open vs WPA-PSK config content and mode 0600.

**Tests / exit gate**
- [x] Open AP connects and obtains DHCP: `GW-Open`, `key_mgmt=NONE`,
      lease `10.42.0.217/24`, `NET STATUS net=connected` (see
      `docs/evidence/remediation-device-2026-11-01.md`).
- [x] WPA2 AP still connects: `Will`, CCMP, DHCP 192.168.1.200.
- [x] Generated config has `key_mgmt=NONE` for open and `psk` for WPA.

### A3. Fix network-row identity after deletion/reorder

**Changes**
- [x] Add `char *g_net_row_ids[GW_NET_MAX]` to the shell state.
- [x] `network_make_row()`: own a `strdup()` SSID and point
      `lv_obj_set_user_data()` at it.
- [x] `network_delete_slot()`: free the slot ID, shift the pointer array, null
      the tail.
- [x] `network_build_rows()` reorder: move ID pointers alongside widget arrays.
- [x] Add a host-sim regression: remove the middle of three APs and verify row
      user data and `network_select_cb` choose the correct SSID.

**Exit gate**
- [x] Removing/reordering APs cannot retarget a row to another network.

### A4. Make “Install selected package” install exactly one package

**Changes**
- [x] Protocol: `APP INSTALL USB <index>`, `APP INSTALL USB ALL`,
      `APP UPDATE USB <index>`.
- [x] `gw-system.c`: parse index, re-scan, validate, verify/info/install only
      the selected package.
- [x] Keep “newest per app ID” logic only for `ALL`.
- [x] Update screen sends selected index; apps screen sends `ALL` and is
      labelled “Install all from USB”.
- [x] Extend the host IPC fake to record the exact command sent.
- [x] Device test with two signed packages: index 0 installed 1.0.0 only;
      `ALL` selected 1.1.0.

**Exit gate**
- [x] Selecting the older package installs only it; status names the same
      package.

### A5. Fix the uinput power-event struct

**Changes**
- [x] `tools/maintenance/pw3-uinput-power.py`: use `struct.Struct("IIHHi")`.
- [x] Add a size assertion (`== 16`) with a clear error.
- [x] Optionally extract shared `tools/lib/uinput.py` used by touch and power.
- [x] Add a host test for event size.

**Exit gate**
- [x] Device logs show 16-byte writes and injected power works
      (`event_size=16`, panel locked).

## 3. Workstream B — truth, gates, durability

### B1. Stop splitting one translation unit mid-function across `.inc` files

**Step A — boundary fix**
- [x] Merge current includes into one temporary implementation file.
- [x] Split at top-level function boundaries.
- [x] Rename files by content (`todo_store`, `status`, `shell_common`,
      `screen_home`, etc.).
- [x] Keep behavior identical.

**Step B — real translation units**
- [x] Add `os/ui/gw_shell_internal.h` with cross-file declarations.
- [x] Move `g_shell` definition to `gw_shell_state.c`.
- [x] Move each screen/group to its own `.c`.
- [x] Remove implementation `.inc` includes from `gw-shell.c`.
- [x] Update `gw_shell_test.h` to include the internal header.

**Exit gate**
- [x] No function starts in one file and ends in another.
- [x] Host-sim geometry output is unchanged.
- [x] `ctags`/`clang-format` can parse each file.
- [x] Each `.c` compiles separately (`make -C os/ui check` parses all UI TUs).

### B2. Remove or wire all dead features and fake tests

- [x] Four-band repaint: `gw_epdc_set_split_page()` has no caller. Delete the
      split path/setter unless a product requirement exists; if kept, call it
      from the page-snap event and test four actual submissions.
- [x] `GW_UI_EINK_FAST_MODE`: currently only makes a host test pass. Delete it,
      or implement DU4-during-input + GC16-after-settle and test the real intent.
- [x] `gw_epdc_should_periodic_clean()`: use it in production with the configured
      limit, or delete it and its test.
- [x] Paint rules: production always passes `scrolling=1, area_pct=0`. Pass real
      values, or simplify the rules to reachable cases and update tests.
- [x] `cleanup_wave`: use it in the cleanup path or delete the field.
- [x] Delete or implement `gw_eink_hold_partial()` and
      `gw_eink_periodic_clean_enabled()`.

**Exit gate**
- [x] No flag is referenced only by tests.
- [x] No tested policy function is bypassed by production.

### B3. Make build acceptance flags fail closed

- [x] Remove `|| GW_*_SPIKE` from hardware gates in `lv_port_epdc.h`,
      `lv_port_input.h`, `gw-power.h`.
- [x] Add one explicit trial flag, e.g. `GW_ENABLE_HW_TRIAL=1`.
- [x] `tools/build/build-arm-binaries.sh`: remove hardcoded
      `POWER_SUSPEND_ACCEPTED=1`/`LIGHT_ACCEPTED=1`; default 0.
- [x] `os/ui/build-arm.sh`: default EPDC/TOUCH accepted to 0; no `SPIKE=1` in
      the default build.
- [x] Add a CI grep check for unconditional `ACCEPTED=1` in build scripts.
- [x] Update comments so “accepted” and “trial” mean what they say.

**Exit gate**
- [x] Default `make` and default ARM build produce stub binaries.
- [x] Hardware writes require an explicit flag and evidence.
- [x] `CAPS` matches the binary capability.

### B4. Replace the unsafe crash handler

- [x] Replace `dprintf()`/`backtrace()` in `crash_handler()` with
      `write(STDERR_FILENO, ...)` and `_exit(128 + sig)`.
- [x] Optionally raise `RLIMIT_CORE` in `main()` and collect core dumps
      externally.
- [x] Add a `GW_UI_TEST_CRASH` hook and verify the process exits promptly.

**Reference:** `signal-safety(7)`.

### B5. Fix package-install durability and TOCTOU

- [x] Add `copy_into_staging(..., max_bytes, ...)`; re-`fstat` after open and
      enforce the limit.
- [x] After extraction, fsync every regular payload file (or `syncfs()`), then
      fsync the directory, then move `active`.
- [x] Add `gw_apps_package_info_verified(package, sig, info)` staging,
      verifying and reading metadata from the staged copy; use it in
      `gw-system.c`.
- [x] Add a test seam that grows the file between `lstat()` and `open()` and
      expects rejection.
- [x] Use `strace -f -e fsync` in a test or QEMU run to prove file data is
      synced before activation (`make check` asserts fsync precedes rename).

**Exit gate**
- [x] Payload data is durable before the active link moves.
- [x] Metadata cannot race the verified copy.

### B6. Fix the public-source package contents

- [x] Exclude `**/*.key` from `tools/package/source-manifest.json`; keep `.pem`
      public keys.
- [x] Add `Makefile`, `tools/check.py`, `tools/hooks/pre-push`,
      `.github/workflows/check.yml` and the dev requirements file to the
      manifest.
- [x] Extend `public-source.py --check` to extract into a temp dir and run
      `make test` (or at least `make -n` plus tests in CI).
- [x] Add an archive-content test: no `*.key`, required build files present.

**Exit gate**
- [x] Archive has no private key material.
- [x] A clean extraction can run the test suite.

### B7. Fix CI coverage and pinning

- [x] Discover all tracked `.sh` files, including
      `apps/koreader/overlay/etc/*.sh`.
- [x] Add Ruff `F821` to `tools/check.py` and CI.
- [x] Pin GitHub actions to full commit SHAs.
- [x] Remove the duplicate LVGL commit from the workflow; read
      `os/ui/source.json`.
- [x] Add a check for new hardcoded `/nix/store` or `/home/` paths.

**Exit gate**
- [x] Broken reader shell scripts and undefined names fail CI before merge.

## 4. Workstream C — security and durability hardening

### C1. Tighten IPC sockets and PID authorization

- [x] Change command/event socket modes from `0666` to `0600` (root-only) or
      `0660` with a dedicated group.
- [x] Keep `SO_PEERCRED`; do not treat PID equality as a sandbox.
- [x] If shell-only commands remain, add `/run/gw-shell.token` mode 0600 or a
      private per-shell socket.
- [x] Test non-root access is denied and root access works: socket mode 0600,
      root rc=0, `nobody` rc=1.

**Exit gate**
- [x] Only intended processes can command the service.

### C2. Use symlink-safe temporary files everywhere

- [x] Extract `gw_atomic_write()` from `gw-stage3.c` into
      `os/system/gw-file.c/h`.
- [x] Use it for `todo_store_save()` instead of `fopen(tmp, "w")`.
- [x] Change `pw3-battery-logger.sh` to a `mktemp -d` private directory.
- [x] Change `test-stage3.c` to `mkdtemp()` and fix the temp-name test
      (currently checks `path.tmp.0..7`, but names include PID/sequence).
- [x] Guard `tools/build/roundtrip.py` against deleting `/`, `$HOME`, and the
      repo root; require `--force`.

**Exit gate**
- [x] Symlink at the temp path is not followed.
- [x] `roundtrip.py --output /` refuses.
- [x] No fixed `/tmp` names remain in root-run scripts.

### C3. Fix guardian and shutdown wedge states

- [x] Remove `BUILD_BUG_ON(sizeof(struct module)!=292)`; rely on
      `vermagic`/`modinfo`.
- [x] On guardian test fire (`state=4`), release `pinned`/`module_put()` under
      `guard_lock`.
- [x] `shutdown.c`: treat `unlink(boot.pending)` `ENOENT` as success.
- [x] `bootstrap.c`: make `halt()` bounded — `sync()`, `reboot(RB_AUTOBOOT)`,
      then diagnostics if reboot fails. Do not leave early failures as
      indefinite `pause()`.

**Exit gate**
- [x] `TEST → wait → STATUS → ARM` works again: state 1 → 4 → 2 → cancel 0.
- [x] Clean persistent shutdown works with and without `boot.pending`
      (two reboots, both returned to native Alpine).
- [x] Early boot failure reboots or enters rescue instead of hanging:
      `vm/qemu-system/bootstrap-failure-test.sh` shows `GW_BOOTSTRAP_HALT`
      followed by `reboot: Restarting system` in about one second.

### C4. Remove hardcoded `/nix/store`, `/home/will`, and unpinned `npx`

- [x] Remove hardcoded paths from `os/ui/build-arm.sh`,
      `os/ui/fonts/generate-fonts.sh`, `tools/build/build-arm-binaries.sh`,
      `vm/qemu-arm/run.sh`, `tools/build/gwapp.py`,
      `tools/maintenance/kindle-linux.py`.
- [x] Add `tools/build/toolchain.sh` as the single source of truth.
- [x] Fail if `GMP_LIB`/`MPC_LIB`/`LINUX_HEADERS` are missing; never let
      `dirname ""` become `.` in `LD_LIBRARY_PATH`.
- [x] Pin `lv_font_conv` to an exact version and regenerate fonts.
- [x] Add CI grep for `/nix/store` and `/home/`.

**Exit gate**
- [x] A clean environment fails with a clear missing-tool message.
- [x] The documented Nix environment reproduces committed artifacts:
      `nix-shell tools/build/nix-shell.nix --run "make check CC=cc"` passes, and
      a second font regeneration is byte-identical.

## 5. Workstream D — device reliability

### D1. Stop hardcoding `/dev/input/event1` in the reader

- [x] `gleworks-display.sh`: scan `/sys/class/input/event*/device/name`, match
      `cyttsp4`, export `GW_TOUCH_DEV`.
- [x] `native.lua`: use `os.getenv("GW_TOUCH_DEV")` with the existing path as
      fallback.
- [x] `gleworks-managed-reader.sh`: require the discovered node, not `event1`.

**Exit gate**
- [x] Reader touch works with an extra input device present: reader opened
      `/dev/input/event1` while `event2` existed.

### D2. Stop forking `gwctl` every two seconds from the reader

- [x] Add `gwctl watch` reading `/run/gw-system.evt`.
- [x] Run it once as a background helper and expose lock state through a small
      `/run` file.
- [x] Have the plugin poll that file, or use a native socket if available.
- [x] Fallback: reduce polling to 15–30 s and document the latency.

**Exit gate**
- [x] Idle reader process count/CPU shows no `gwctl` spawn every 2 s
      (40 samples, one PID; lock reaction 2 s).
- [x] Lock response remains acceptable.

### D3. Make `CAPS apps=1` truthful and decide app-runtime scope

**Short term**
- [x] Change `CAPS` to `apps=install-only` or `apps=0`.
- [x] Keep `APP LAUNCH` explicitly `not-accepted` for non-KOReader IDs.
- [x] Fix `gw-app-manifest.h` comments that still say “not integrated”.
- [x] Add a host test that `CAPS` matches accepted launch commands.

**Long term (explicitly deferred to the app-runtime project)**
Not tasks of this remediation; tracked in
[2026-10-31-app-runtime.md](2026-10-31-app-runtime.md):
per-app UID/GID, permission enforcement for display/touch/frontlight/network,
display/touch handoff and crash cleanup, resource limits and data-directory
policy, and physical acceptance.

**Exit gate**
- [x] The public contract matches the implementation.

## 6. Workstream E — organization and performance

### E1. One shared atomic-file implementation

- [x] Create `os/system/gw-file.c/h` with
      `gw_write_atomic(path, data, len, mode)` and `gw_fsync_dir(path)`.
- [x] Move `config_write_atomic()` logic into it.
- [x] Use it from `gw_config_save_int()`, `gw_config_save_string()` and
      `todo_store_save()`.
- [x] Keep `O_CREAT|O_EXCL|O_NOFOLLOW`, fsync file, rename, fsync parent.

**Tests**
- [x] Symlink at temp path is not followed.
- [x] Two rapid saves leave no debris.
- [x] Strace shows fsync before rename.

**Exit gate**
- [x] Exactly one atomic-write implementation in the repo.

### E2. One toolchain/build environment

- [x] Add `tools/build/toolchain.sh` resolving `CC`, `STRIP`, `MUSL_PREFIX`,
      `LINUX_HEADERS`, `GMP_LIB`, `MPC_LIB`; validate paths; never add `.` to
      `LD_LIBRARY_PATH`; fail closed.
- [x] Source it from `os/ui/build-arm.sh`,
      `tools/build/build-arm-binaries.sh`, `vm/qemu-arm/run.sh`.
- [x] Pin the font generator tool version.
- [x] Add CI grep for hardcoded paths.

**Exit gate**
- [x] Only `toolchain.sh` contains toolchain discovery logic.

### E3. One device-preparation script

- [x] Add `os/system/gleworks-device-prepare.sh` with idempotent functions:
      `mount_early_filesystems`, `create_core_devices`, `load_touch_modules`,
      `create_input_nodes`, `load_usb_gadget`.
- [x] Use it from `gleworks-early.sh`, `gleworks-hardware-prepare.sh` and
      `gleworks-display.sh`.
- [x] Keep it POSIX sh, fail closed, avoid double module loads.

**Tests**
- [x] `sh -n`.
- [x] Boot on QEMU (`vm/qemu-system/device-prep-test.sh`): PASS.
- [x] Boot with an already-loaded module (idempotency): PASS
      (`GLEWORKS_DEVICE_PREPARE_PASS insmod_count=2`).

**Exit gate**
- [x] The same device-node/module logic exists in one place.

### E4. Replace the giant command `if/else` with a policy table

- [x] Add a command descriptor table with name/prefix, flags (`ROOT`,
      `SHELL_ONLY`, `LOCKED_DENY`, `READ_ONLY`) and handler.
- [x] Move each `if (strcmp(line, ...))` block into a handler.
- [x] Apply authorization once in the dispatcher.
- [x] Remove duplicated `pid == g_shell_pid` checks from app install/remove.

**Tests**
- [x] Authorization matrix: root/non-root, shell/non-shell, locked/unlocked,
      allowed/denied.
- [x] Prefix commands still parse correctly.
- [x] Unknown command returns the same error as before.

**Exit gate**
- [x] One place to add a command and one place to change its policy.

### E5. Batch IPC and remove N+1 calls

- [x] Add `APP LIST` (all app rows in one response).
- [x] Add `NET SCAN RESULTS` (all APs in one response).
- [x] Update `apps_fetch()` and `network_scan_apply()` to use them.
- [x] Remove repeated `gw_apps_list()` calls from `APP INFO`.
- [x] Remove repeated parsing of `/run/gw-wifi-scan` from `NET RESULT`.

**Tests**
- [x] Host fake returns multiple lines.
- [x] Service returns bounded multi-line responses.
- [x] UI handles partial/truncated lists.
- [x] Measure round trips before/after: `test-ipc-roundtrip` shows 8 commands
      over 1 persistent connection (the old one-shot path used 8 connects).

**Exit gate**
- [x] Startup/scan paths no longer open one socket per item.

### E6. Persistent/multi-command IPC connection

- [x] `service_loop`: allow multiple newline-delimited commands per client;
      close on client close, timeout, invalid input or `QUIT`.
- [x] Client: add `ipc_conn_t` with `open`, `cmd`, `close`.
- [x] UI opens once in `gw_ui_build()` and reconnects on error.
- [x] Keep one-shot behavior for `gwctl` if desired.

**Tests**
- [x] Multiple commands over one socket.
- [x] Idle timeout closes.
- [x] Broken connection triggers reconnect.
- [x] No command crossing between clients.

**Exit gate**
- [x] UI does not connect/disconnect for every `STATUS`, `APP` or `NET` query.

### E7. Collapse the paint/profile/policy stack and fix EPDC batching

- [x] Use `gw_epdc_should_periodic_clean()` in production or delete it.
- [x] Use `cleanup_wave` or delete the field.
- [x] Remove or wire `page_snap`/four-band/fast-mode paths.
- [x] Simplify `gw_paint_rules_decide()` to reachable criteria, or pass real
      `scrolling`/`area_pct`.
- [x] In `gw_epdc_present()`: submit one update per separated dirty rect, or
      merge only nearby rects and update the comment.
- [x] Add a host-sim test with two distant dirty regions and assert intended
      submission count/area.

**Exit gate**
- [x] No rule/flag/path production never exercises.
- [x] EPDC code does what its comments say.

### E8. Organize tests around behavior, not implementation

- [x] Keep pure-function tests only where production calls those functions.
- [x] Add integration tests for open/WPA Wi-Fi config, selected-package install,
      row identity after removal, IPC authorization, and multi-command/persistent
      connection.
- [x] Replace compile-flag assertions with behavior assertions.
- [x] Use snapshot hashes for host-sim geometry/screens where appropriate.
- [x] Make host-sim fail if `geometry.json` cannot be written.

**Exit gate**
- [x] Behavior regression fails a test.
- [x] Dead-helper removal does not require a fake test update.

## 7. Workstream F — moved to a separate plan

The UI/UX hierarchy pass is explicitly a separate product review.  Its
checklist now lives in
[2026-11-02-ui-ux-hierarchy.md](2026-11-02-ui-ux-hierarchy.md) (backlog B24)
so this remediation plan contains no unstarted product tasks.

## 8. Execution order

### Sprint 1 — unblock and fix P0
- [x] A1 tools/lint
- [x] A2 open Wi-Fi
- [x] A3 network rows
- [x] A4 single-package install
- [x] A5 uinput

### Sprint 2 — truth and gates
- [x] B7 CI/lint
- [x] B2 dead code decisions
- [x] B3 acceptance flags
- [x] B4 crash handler
- [x] B5 package durability
- [x] B6 public source

### Sprint 3 — hardening
- [x] C1 IPC permissions
- [x] C2/E1 atomic writer
- [x] C3 guardian/shutdown
- [x] C4/E2 toolchain
- [x] E3 device prep

### Sprint 4 — device reliability
- [x] D1 touch node discovery
- [x] D2 reader polling
- [x] D3 CAPS/app truth

### Sprint 5 — service/UI organization
- [x] E4 command table
- [x] E5 batch IPC
- [x] E6 persistent connection
- [x] B1/E7 UI split + paint cleanup

### Sprint 6 — final verification
- [x] E8 test organization
- [x] Full host/QEMU/device matrix (all in-scope rows).
- [x] Public-source extraction test
- [x] Update code comments/headers (docs separately)

## 9. Verification matrix

| Area | Host | QEMU ARM | Device |
| --- | --- | --- | --- |
| Python/tools | Ruff `F821`, `compileall`, `--help` | — | — |
| System C | `make -C os/system test`, fixtures | `vm/qemu-arm/run.sh` | suspend/light/Wi-Fi |
| UI C | host-sim geometry | ARM build if toolchain | EPDC/touch |
| Wi-Fi | open/WPA config test | — | open + WPA2 + reconnect |
| Apps | fixture install | signature test | two-package selection |
| Packaging | extract + `make test` | — | — |
| IPC | permission matrix | — | lock/unlock, reconnect |
| Guardian | — | — | arm/test/cancel, clean shutdown |
| Reader | Lua syntax/lint if available | — | touch discovery, lock latency |

## 10. PR checklist

- [x] Which finding/work item does this close?
- [x] What is the behavior before/after?
- [x] What test would have caught this?
- [x] Does the default build still avoid hardware?
- [x] Does this add a new abstraction with only one caller? If yes, justify it or
      remove it.
- [x] Does this duplicate logic that already exists in `gw-file`, `toolchain.sh`
      or `device-prepare.sh`?
- [x] Are comments/headers now truthful?
- [x] Device evidence path recorded:
      `docs/evidence/remediation-device-2026-11-01.md`.

Guiding rule: remove work and indirection before adding new abstractions.

## 11. Implementation evidence (2026-11-01)

`vm/qemu-arm/run.sh` passes with the private toolchain and qemu-arm:
power-policy, Stage 3 atomic-write and dynamic signature tests all PASS.

ARM builds were exercised with the private toolchain: the default
`build-arm-binaries.sh` produces stub binaries (no `ACCEPTED=1`/trial strings),
and `GW_ENABLE_HW_TRIAL=1` produces trial binaries containing
`GW_ENABLE_HW_TRIAL=1` and `GW_UI_TRIAL_FLAGS=1`.

`nix-shell tools/build/nix-shell.nix --run "make check CC=cc"` also passes,
and regenerating the fonts twice from the documented Nix shell produced
byte-identical files.

`vm/qemu-system/device-prep-test.sh` boots a Linux initramfs on QEMU virt and
runs the device-preparation functions twice with one touch module simulated as
already loaded; it reports `GLEWORKS_DEVICE_PREPARE_PASS insmod_count=2`.

Host gates run with `make check` (system tests, new network/command-policy/
IPC service/client/EPDC-batch tests, host simulator geometry, crash-handler
timeout, Ruff `F821`, public-source extraction + `make test`) all pass.

Persistent IPC is covered by `test-ipc-service` (multiple commands, idle
timeout, reconnect after a broken client, no cross-client leakage),
`test-ipc-client` (automatic reconnect/retry) and `test-ipc-roundtrip`
(8 commands / 1 connection).

Device acceptance was run over the physical USB console.  Evidence is in
`docs/evidence/remediation-device-2026-11-01.md`: socket permissions, selected
USB package install, WPA2+DHCP, uinput power, touch discovery with an extra
input node, reader polling, guardian TEST→ARM, and shutdown with/without
`boot.pending` all pass.  Open-network DHCP also passed: lease
`10.42.0.217/24` from `10.42.0.1`, `STATUS net=connected`.  The temporary host
DHCP container and firewall rule were removed afterwards.

The UI is now organized as real translation units: `gw-shell.c` plus
`gw_shell_state.c` and one `gw_screen_*.c` per screen, sharing declarations in
`gw_shell_internal.h`.  The host simulator geometry output is unchanged
(2638 checks, 0 failed).  D3 per-app isolation remains deferred to the separate
app-runtime project, as declared in the scope section above.

## 12. Completion and scope note

All **A–E** in-scope implementation checkboxes are closed and the host, Nix and
QEMU gates listed above pass. The original device evidence is recorded in
[`docs/evidence/remediation-device-2026-11-01.md`](../../evidence/remediation-device-2026-11-01.md),
but it covers the prior UI binary. New-UI device verification (Apps/Network
counts, selected Wi-Fi, lock/reader handoff, page-snap/four-band, EPDC) remains
pending; see that evidence file for the explicit boundary.

D3's long-term per-app isolation is **not implemented here**; it is explicitly
deferred to the separate app-runtime project
[2026-10-31-app-runtime.md](2026-10-31-app-runtime.md), matching the scope
note at the top of this plan.

**Workstream F is tracked separately.** It is a product UX pass in
[2026-11-02-ui-ux-hierarchy.md](2026-11-02-ui-ux-hierarchy.md) (backlog B24)
and must not be treated as part of A–E completion.
