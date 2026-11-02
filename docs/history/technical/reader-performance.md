---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

> Current 2026-10-29 feedback: Reader responsiveness accepted by user; mild
> ghosting prompted refresh **4/4 pages**, confirmed on the connected Kindle.
> The integrated profile now uses four pages too. Actual ghosting improvement
> after this change still needs user observation; no latency benchmark exists.
> Older six-page statements below describe the initial profile.

> Installation update: the user-run installer applied the balanced profile.
> USB readback confirms both refresh counts are 6, animations are false, double
> tap is disabled, and the original settings backup exists. Actual reading and
> response-time comparison remain pending. The earlier source-investigation
> notes below describe the pre-install state.

User acceptance: Reader works with good responsiveness, with light remaining
ghosting. Requested follow-up is four-page full refreshes for day/night; the host
patch in README applies this and backs up preferences before changing them.
Other reading preferences remain unchanged. More frequent full refreshes mean
more frequent flashes; panel latency improvement is not claimed.

# Balanced reader responsiveness

User choice: balanced speed and clean text. These are proposed settings, not
changes already applied to the Kindle.

## Initial profile

Change preferences through KOReader's menus, after backing up
`koreader/settings.reader.lua` with KOReader closed. Record existing values so
they can be restored. Do not replace the whole settings file.

| Preference | Starting value | Reason |
| --- | --- | --- |
| Full refresh rate | Every 4 pages | Partial page updates between periodic ghosting cleanup |
| Page turn animations | Off | Avoid extra animated screen updates |
| Disable double tap | On; restart KOReader if prompted | Single taps need not wait for a possible second tap |

Keep image/chapter refresh behavior and other preferences unchanged initially.
If text ghosts noticeably, try a shorter full-refresh interval. Six pages is a
starting point, not a measured optimum. Full refreshes still flash and take time.
Do not shorten touch debounce timings: missed or duplicate turns would undermine
the responsiveness goal.

Keep our transfer service stopped outside transfer sessions. VPN use is manual;
compare reading with it stopped before deciding whether continuous connectivity
is worthwhile. No keep-awake loop, thumbnails, library indexing service, or
background polling is required for our upload interface.

## Separate the delays

Touch handling, book layout/rendering, and the physical E Ink refresh all add
latency. Software can reduce the first two; the panel still has a physical
refresh limit. Cold conditions can slow E Ink. Scanned PDFs and complex pages
can also render more slowly than a simple EPUB. Replacing Amazon's OS alone
does not establish that these delays will disappear.

Installed KOReader's `koreader.sh` supports `--framework_stop` and includes
restoration of `lab126_gui` on normal exit. This is an optional comparison mode,
not our default: first verify launch, normal exit to Amazon's library, and
sleep/wake on this device. Source inspection cannot establish crash recovery or
a performance benefit. Retain the normal launch path.

## Device comparison before claiming an improvement

1. Use the same simple EPUB, font/layout, frontlight, and room temperature.
   Open it once before comparing, so initial book loading is separate.
2. Record 24 sequential page turns with current preferences, then repeat after
   the three settings above. Tap once per turn and wait until the page settles.
3. If practical, film at 60 fps or higher. Count frames from finger release to
   the first visible change and to settled text. Report ordinary partial turns
   separately from full refreshes; compare median and worst observed times.
4. Note missed/duplicate taps and whether ghosting is acceptable. Compare one
   menu open/close and sleep/wake too. Do not infer a speedup from memory usage.
5. Only then compare the upstream framework-stop mode with identical settings.
   Keep it optional unless recovery works and the benefit is observable.

## Evidence so far

Read the mounted KOReader source on 2026-10-29:

- `frontend/ui/elements/refresh_menu_table.lua`: Every 6 pages is a supported
  menu choice. Active settings are `full_refresh_count` and
  `night_full_refresh_count` in `frontend/ui/uimanager.lua`; `refresh_rate_1`
  and similar keys are custom preset slots.
- `frontend/ui/elements/page_turns.lua`: `swipe_animations` controls animations.
- `frontend/ui/elements/common_settings_menu_table.lua` and
  `frontend/device/gesturedetector.lua`: `disable_double_tap` is supported;
  disabling double tap bypasses its single-tap waiting timer.
- `koreader.sh`: supported framework-stop argument and normal-exit restoration.

No preferences, device services, waveforms, CPU clocks, kernel, or firmware were
changed. No device latency measurement or improvement has been verified yet.
