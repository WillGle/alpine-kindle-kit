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
| `/usr/sbin/gw-shell` | `a76820790341e9699d1dceba7734057d56a3ff825980012e26e7124bf9e4d1cc` | static; one shell state object, no include seam |

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
