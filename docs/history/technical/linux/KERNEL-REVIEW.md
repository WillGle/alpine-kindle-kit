---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# PW3 public kernel source review — 2026-10-29

Status: public audit baseline pinned; acquisition blocked by host DNS. No
complete PW3 port selected, built or deployed. Ponder installers/images and
inaccessible smallsolar patches are excluded under the updated plan. No
Solarcene account, registration or subscription is a prerequisite.

## Public baseline

Use upstream [Linux v6.18](https://github.com/torvalds/linux/releases/tag/v6.18)
at [7d0a66e4bb9081d75c82ec4957c50034cb0ea449](https://github.com/torvalds/linux/commit/7d0a66e4bb9081d75c82ec4957c50034cb0ea449)
as a reproducible source-audit reference. The 6.18 series is listed as longterm
by [kernel.org](https://www.kernel.org/releases.html). This initial release is
not a security-updated deployment selection: stable fixes and port patches
must be reviewed before selecting a target build.

`sh ours/linux/fetch-kernel.sh` fetches only this public source tag, checks its
full commit ID and records a SHA256 of a retained source snapshot. It refuses
to overwrite acquisitions. No build or device operation occurs. A commit/hash
match records provenance; it is not trusted signature verification or proof of
hardware support. The host currently cannot resolve GitHub or cdn.kernel.org;
the web tool could read selected public pages but cannot substitute for a
complete source checkout/build input. See execution-checks.txt for actual runs.

## Source findings and limits

The upstream v6.18 [i.MX device-tree build list](https://raw.githubusercontent.com/torvalds/linux/v6.18/arch/arm/boot/dts/nxp/imx/Makefile)
lists i.MX6SL EVK, Kobo, Tolino and Warp targets, with no Kindle/Muscat/PW3
target. Generic i.MX6SL support therefore does not supply a ready PW3 DTB.
This is a finding about that revision/build list, not proof that no public
out-of-tree PW3 port exists.

Full driver-source inspection remains pending checkout acquisition. A request
for upstream MFD Kconfig returned HTTP 429, and the attempted CYTTSP4 source
path returned 404; neither establishes driver absence. Public EPDC work such
as [akemnade/linux](https://github.com/akemnade/linux) remains a research lead;
no branch or revision from it has been acquired or approved. Do not substitute
Kobo panel/regulator settings for this Kindle's hardware parameters.

## Requirements for a complete public PW3 port

| Area | Existing local evidence | Required audit and acceptance |
| --- | --- | --- |
| Boot | Captured stock uImage uses load/entry 0x80008000; stock kernel uses ATAGs, with OF disabled | Audit image wrapping, DTB handoff and command line for installed U-Boot; a new kernel's format/size must match the actual boot path |
| Display | Stock uses proprietary mxc_epdc_eink | Obtain compatible public EPDC support; preserve panel waveform/calibration/VCOM; verify timings and KOReader refresh interface |
| Touch | CYTTSP4 observed on stock | Obtain compatible public support; audit I2C, interrupts, reset and orientation; validate taps and terminal keyboard |
| Power | Stock MAX77696 and suspend configured | Obtain/adapt public PMIC drivers; audit regulators, charger, battery, frontlight, button, shutdown and suspend; measure repeated sleep/wake and battery behavior |
| Wi-Fi | Stock ath6kl_sdio, vendor 0271/device 0301 | Audit SDIO power/reset, firmware and board calibration preservation; validate reconnect after wake |
| Alpine | ARMv7 and VFPv3 configured | Select a supported release after ABI, libc/syscalls, init, device nodes, filesystems and packages are validated against the chosen kernel |
| Tailscale | Stock CONFIG_TUN disabled | Normal kernel networking requires working TUN. Userspace mode requires a separate inbound/outbound design; verify tailnet-only SSH/HTTP and rejection of LAN access |

The TUN distinction follows [Tailscale's documentation](https://tailscale.com/docs/concepts/userspace-networking).
Userspace mode is not declared incapable of inbound services. The
[KOReader platform discussion](https://github.com/koreader/koreader/issues/12226)
distinguishes framebuffer and DRM integration; Kobo results are not PW3 acceptance.

## Remaining execution

Acquire the public baseline when host DNS/network access permits. Inspect the
actual source against the hardware requirements above, identify public patches
or implement missing support, and pin the resulting complete port before
building it out of tree with compiler/config/log/hash records. Official public
Alpine sources remain the userspace track. A host build may proceed before
recovery is proven, but no persistent boot change is allowed until ordinary-USB
recovery has been demonstrated. No rootfs, service configuration or private
credentials should be installed on the device as a substitute for that gate.

## Actual checkout inspection after user acquisition

Acquisition blocker for this baseline is resolved. Verified local HEAD and
v6.18^{commit} both equal 7d0a66e4bb9081d75c82ec4957c50034cb0ea449; git status
is clean and the retained source-snapshot.tar passes its SHA256 manifest.
Earlier network-failure notes above remain historical.

Direct source findings:

- i.MX DTB Makefile has i.MX6SL EVK/Kobo/Tolino/Warp, no PW3 target.
- Case-insensitive content scans of drivers/, include/ and DT bindings found
  no MAX77696 or CYTTSP4 references. Kconfig provides other MAX77xxx and
  CYTTSP/CYTTSP5 devices; those are not substitutes for verified hardware support.
- No mxc_epdc/imx EPDC driver found in drivers/gpu/drm/imx or fbdev. EPDC pin
  names and unrelated acronym matches elsewhere are not an e-ink driver.
- ath6kl/sdio.c includes AR6003_01; include/linux/mmc/sdio_ids.h maps it to
  vendor 0x0271/device 0x0301, matching the captured device. Firmware, board
  calibration, SDIO wiring and power sequencing still require verification.
- arch/arm/Kconfig:1441 onward retains ARM_APPENDED_DTB and ARM_ATAG_DTB_COMPAT.
  This supports investigating the existing-loader route. The converter is not
  a general translator for Amazon-specific tags; board identity/calibration
  and root command-line handling need explicit design and validation.

Conclusion: vanilla v6.18 is not a complete PW3 target. Do not generate a
flashable image or label a generic imx build as device-ready. Obtain public
vendor driver/board references and an EPDC development branch, audit their
licenses/dependencies and exact commits, then estimate/implement missing ports.
No kernel build or device operation performed in this inspection.

## Vendor reference acquired

Public vendor source now exists locally at linux-vendor-reference, clean commit
7ac2585f384573da8cbdcc987a3503e2e6ff1fb4. See
[VENDOR-REFERENCE-REVIEW.md](VENDOR-REFERENCE-REVIEW.md) for concrete PMIC/touch
port inputs, older firmware provenance and the missing REAGL implementation
boundary. This resolves reference acquisition, not the modern driver port.
