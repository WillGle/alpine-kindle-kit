---
status: current
updated: 2026-11-02
verified_build: ecb8f560f415425d59ee40aee5a74234412d30b1167d0954edc4ab76564387f9
supersedes: none
audience: maintainer
---

# UI/UX hierarchy cleanup redeploy (2026-11-02)

Follow-up to
[`pw3-ui-hierarchy-2026-11-02`](../pw3-ui-hierarchy-2026-11-02/README.md).
This redeploy removes the dead `apps_install_usb_cb()` path found in the
post-review cleanup. It does not change the accepted Phase F UI.

## Build

| Item | Value |
| --- | --- |
| Deployed shell | `ecb8f560f415425d59ee40aee5a74234412d30b1167d0954edc4ab76564387f9` |
| Previous deployed shell | `1804ea8fcf32d427ba715e431de79e8cf2f671b6a4413f1d6f3ff2b359fcddaf` |
| Rollback copy on device | `/usr/sbin/gw-shell.before-hierarchy-cleanup-20261102` = `1804ea8f…fcddaf` |
| Device boot | `f29917d9-b633-46ae-98b8-a7c6214c9a92` |
| Build flags | `EPDC_accepted=1 touch_accepted=1 GW_ENABLE_HW_TRIAL=1 GW_UI_DIAG=1` |

The source cleanup removes `apps_install_usb_cb()` from
`os/ui/gw_screen_apps.c` and its declarations from `gw_shell_test.h` and
`gw_shell_internal.h`. No widget referenced it; the selected-package model
remains `Refresh`, one `Scan USB` and one `Install selected`.

## Deployment

The binary was staged over the physical USB CDC-ACM console to
`/usr/sbin/gw-shell.new`, verified by SHA-256, then renamed into place on the
same filesystem. The previous shell was copied first to the rollback path.
`rc-service gw-system restart` restarted the supervisor and the new shell.

## Verification

```text
/usr/sbin/gw-shell  ecb8f560f415425d59ee40aee5a74234412d30b1167d0954edc4ab76564387f9
gw-system           running (PID 14307)
gw-shell            running (PID 14310)
gwctl status        STATUS ... foreground=none ... display=on
gwctl caps          CAPS light-set=1 wifi=trial apps=install-only suspend=1
```

- Home framebuffer: [`home.png`](home.png).
- App manager framebuffer: [`apps.png`](apps.png). It shows one `Refresh`
  action, one `Scan USB` action and one disabled `Install selected` action;
  there is no install-all action.
- Framebuffer captures prove what was written to `/dev/fb0`, not what the
  physical panel showed.

Rollback:

```sh
cp /usr/sbin/gw-shell.before-hierarchy-cleanup-20261102 /usr/sbin/gw-shell
rc-service gw-system restart
```
