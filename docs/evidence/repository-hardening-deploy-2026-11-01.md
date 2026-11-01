# Repository hardening — PW3 deployment (2026-11-01)

- Device: PW3, Alpine 3.24.3, kernel 3.0.35-lab126, PID1 `/bin/busybox`.
- Connection: physical USB CDC-ACM console `0525:a4a7` (`/dev/ttyACM0`).
- Method: staged upload to `/tmp`, SHA-256 verification, same-filesystem rename,
  then `rc-service gw-system restart`. The console shell is independent of the
  GUI service.

## Deployed binaries

| File | SHA-256 | Notes |
| --- | --- | --- |
| `/usr/sbin/gw-system` | `e6ca00b92963bde3e2a06105f719f040b47085c7df1992e686374fae00a8a477` | dynamic against musl; `dlopen(libcrypto)` works |
| `/usr/sbin/gwctl` | `e83ba4148c55e7508ae67e0dbf0da18730c4e1ae2026c98ff03732d575e7d004` | unchanged |
| `/usr/sbin/gw-appctl` | `6361dbb5f88ba13d5dcd460d3f753330933863968bc9c27d0950d67496da995d` | dynamic; verifies signed packages |
| `/usr/sbin/gw-shell` | `3083b5de8680236f004c7a68a0c4414af79e00693d6730eb5536db73a73966e0` | static; one shell state object, no include seam |

Rollback copies: `/usr/sbin/{gw-system,gw-appctl,gw-shell}.before-hardening-20261101`.

## Verification after restart

- `gwctl status` returned a STATUS line; `gwctl caps` returned
  `CAPS light-set=1 wifi=trial apps=1 suspend=1`.
- `gw-system` and `gw-shell` were running as the new processes.
- `gw-appctl verify` returned `OK` for a fixture signed with the production
  release key.
- A corrupt package was rejected (`ERR file-unreadable`).
- A production-key-signed package containing a setuid member was rejected by
  `gw-appctl install` with `ERR link-special-or-oversized-rejected`; no app
  directory and no staging directory remained.
- Framebuffer capture after deployment shows the new Home screen:
  [repository-hardening-deploy-2026-11-01.png](repository-hardening-deploy-2026-11-01.png).

## Still user-acceptance

Physical touch calibration/targeting, power-button suspend/resume, and UI
walkthrough on the panel were not exercised by this automated deployment.

## Follow-up: brightness repaint and scroll cleanup

The first deployed shell still scheduled a full GC16 after a plain `RELEASED`
inside a scrollable settings page, so a brightness tap looked like a
whole-panel black/white repaint. Fixed in the same source tree:

- `scroll_activity_cb()` requests a scroll paint only when
  `gw_eink_scroll_was_moved()` is true.
- automatic scroll idle full GC16 is disabled by default
  (`g_scroll_clean_delay_ms = 0`); opt in with `GW_UI_SCROLL_CLEAN_MS`.
- the cleanup reason is now `scroll_cleanup`, not `lock`.

Deployed shell hash: `3083b5de8680236f004c7a68a0c4414af79e00693d6730eb5536db73a73966e0`. The paint-rule engine selects per-screen/content profiles; text profiles use page snapping with no live drag frames. Scroll cleanup now uses a partial GC16 over the touched area only; brightness controls cancel any pending cleanup.

## Follow-up: static text quality

Settings/Notes now use one GC16 page pass on page-snap (no second cleanup)
and a periodic full clean every 15 page snaps via
`GW_UI_FULL_REFRESH_AFTER=15`. Deployed shell:
`5fef3b22cafa97ec7de7d0274f1a4e5c3bc332876ebc96105aa6cd687ea0272b`.
Rollback: `/usr/sbin/gw-shell.before-quality-text-20261101`. User acceptance: current static-text quality and page-snap behaviour are acceptable (2026-11-01).

Text page pass changed from GC16 to REAGL to reduce latency (one pass per
page; periodic full clean remains every 15 pages). Rollback:
`/usr/sbin/gw-shell.before-reagl-text-20261101`.

Text page passes were changed from REAGL to DU4 after REAGL measured ~711 ms
panel wait. DU4 uses a 4-level grayscale direct update. Rollback to the last
REAGL build: `/usr/sbin/gw-shell.prod-reagl-20261101`.

DU4 was rejected for text: it was faster but dropped/eroded glyph strokes.
The GC16 page-pass build was restored (`5fef3b22…`). DU4/A2 are not suitable
for 16-level antialiased text on this panel.

Split text page repaint deployed as `17d1df39…`; rollback
`/usr/sbin/gw-shell.before-split-page-20261101`. User confirmed the split
repaint is visibly faster (2026-11-01).

Three-band text page repaint deployed as `156def47…`; rollback
`/usr/sbin/gw-shell.before-3bands-20261101`. User confirmed full text,
visibly slightly faster, and no visible band boundaries (2026-11-01).

Four-band text page repaint deployed as `58d32966…`; rollback
`/usr/sbin/gw-shell.before-4bands-20261101`. User acceptance: visibly faster;
faint band boundaries are acceptable (2026-11-01).

Reader launch latency was reduced by removing two one-second waits: the shell
now leaves its render loop immediately after the handoff request, and the
supervisor polls at 20 ms while waiting for the shell to exit.  Deployed as
`gw-shell` `dba2433f…` and `gw-system` `307d8d85…`; rollback copies are
`*.before-launch-latency-20261101`.

Runtime audit (2026-11-01) added a SIGCHLD self-pipe to `gw-system`, so both
reader handoff and reader exit no longer wait for the one-second poll timeout.
A killed `gw-shell` was observed replaced in about 60 ms. The split-page path
now counts one budget unit per page snap so the every-15-page full clean still
applies. Deployed as `gw-shell` `80160c3a…` and `gw-system` `d1c52103…`;
rollback copies are `*.before-audit-20261101`.
