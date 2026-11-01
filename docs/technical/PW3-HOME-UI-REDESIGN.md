# PW3 Home UI redesign — design specification

Date: 2026-10-31  
Target: Kindle Paperwhite 3, 1072 × 1448, 300 PPI, 8 bpp EDC framebuffer  
Status: **host implementation complete; physical acceptance deferred until the
Phase 4D battery session ends.**

> No physical Kindle was touched for this work. There were no USB commands,
> deployments, device captures, SSH sessions, Wi-Fi changes, service restarts
> or device-side telemetry.

## 1. Goals

The redesigned shell is a reading-first E-ink interface rather than a
diagnostic panel:

- one clear reading action on every Home visit;
- strong type hierarchy with generous whitespace;
- minimal borders, no shadows, gradients, blur or animation;
- four functional quick actions, never a nonfunctional placeholder;
- consistent 48 px page margins and 24 px gutters;
- every primary touch target at least 96 × 96 px;
- Vietnamese glyph support for dates, book titles and tasks.

## 2. Screen map

| Screen | Entry points | Purpose |
|---|---|---|
| Home | bottom nav, Back | Reading home |
| Settings | bottom nav, Light shortcut, Back | Frontlight, Wi-Fi, lock, device facts |
| Calendar | Home calendar card | Month grid and upcoming events |
| To-dos | Home TODOs card / New task | Active list, completed/deleted history |
| Network | Home Network shortcut, Settings Wi-Fi | Scan, connect, disconnect |
| Lock | Home Lock shortcut, Settings Lock now | Screen lock |

Screens are created exactly once at startup and switched with `lv_scr_load()`.
Status updates change label text only; they never rebuild a screen.

## 3. Home geometry (physical pixels)

```
0 ─────────────────────────────────────────────── 1072
  status bar 0..87            time · Wi-Fi · battery
88
  hero: "Today."             top 108, 72 px bold
        localized date       top 208, 36 px
290  Continue Reading card    976 × 320   x 48..1023
638  Calendar card            476 × 360   x 48..523
     To-dos card              476 × 360   x 548..1023
1030 QUICK ACCESS label
1070 Books | Network | Light | Lock   4 × 226 × 170, 24 px gaps
1344 bottom navigation        1072 × 104
1448
```

Rules:

- Page margin: 48 px. Content width: 976 px.
- Gutter: 24 px. Section gap: 32 px (28 px card-to-card rhythm on Home).
- Card radius: 10 px; single 1 px `#B0B0B0` border; card fill `#F8F8F8`.
- Bottom navigation is 104 px tall with a 1 px top hairline and a 4 px active
  underline.
- The app bar on secondary screens is 136 px tall with a 150 × 96 Back button.

## 4. Typography and colour

| Token | Value |
|---|---|
| Hero | DejaVu Sans Bold 72 px |
| Screen / book title | DejaVu Sans Bold 44 px |
| Section / quick-action title | DejaVu Sans Bold 36 px |
| Date | DejaVu Sans 36 px |
| Body | DejaVu Sans 28 px |
| Eyebrow / status / secondary | DejaVu Sans 24 px |
| Primary text | `#141414` |
| Secondary text | `#545454` |
| Muted text | `#808080` |
| Border | `#B0B0B0` |
| Card | `#F8F8F8` |
| Background | `#FFFFFF` |

Fonts are generated as 4 bpp LVGL bitmap fonts with coverage for ASCII,
Latin-1, Latin Extended-A/B, Vietnamese Latin Extended Additional
(U+1EA0..U+1EF9) and combining marks. The generated C files are checked in, so
builds do not need node/npm or a font package.

## 5. Component specification

### Status bar

Time on the left, Wi-Fi state and battery on the right. No decorative icons.
Values are updated only when the displayed string changes.

### Hero

`Today.` plus a Vietnamese date, for example `Thứ Bảy, 31 tháng 10 2026`.
The date is formatted from `localtime()`; no locale package is required.

### Continue Reading

- Whole card is one ≥96 px tap target that launches KOReader.
- Populated state: book title, optional author, progress bar and `N% read`.
- Invalid progress: progress bar hidden, `Progress unavailable` shown.
- Empty state: `No book open`, guidance and a `Browse Books` button.
- The current IPC contract has no book-metadata command. On the device the
  empty state is used; the host preview supplies mock metadata. No filesystem
  scraping or invented backend was added.

### Calendar widget

Day number (72 px), Vietnamese weekday/month, and the nearest event or
`No upcoming events`. Text is single-line with ellipsis when needed. The card
opens the Calendar screen. No event provider exists today, so the device shows
the empty state; the host preview uses mock data.

### To-dos widget

First active task preview, or `No tasks yet.`, and a 96 px `New task` action.
The preview is single-line with ellipsis. The card and button open the To-dos
screen. Tasks are stored in a bounded atomic TSV file with active/done/deleted
states; completed and deleted tasks are visible in the History view.

### Quick Access

Four functional shortcuts:

| Shortcut | Action |
|---|---|
| Books | Existing `APP LAUNCH koreader` path (KOReader opens its library) |
| Network | Network screen (`NET SCAN` / `NET CONNECT` / `NET DISCONNECT`) |
| Light | Settings frontlight control (`LIGHT GET` / `LIGHT SET`) |
| Lock | Existing `LOCK` / `UNLOCK` path |

`Files` and `Browser` are omitted because no such app or IPC capability exists
in this repository. They are not rendered as disabled or fake controls.

### Bottom navigation

`Home`, `Books`, `Settings`. Each item is 357 × 104 px. `Books` uses the same
existing KOReader launch path as Continue Reading.

### Network

Connection state, network dropdown, password field and Scan / Connect /
Disconnect actions. The keyboard is hidden until the password field is focused.
This preserves the existing Wi-Fi command set and hex encoding. Password
characters hide immediately, the cursor does not blink, a pressed key inverts
briefly, and key/text updates use separate A2/DU dirty regions. See
[PW3-TYPING-RESPONSIVENESS.md](PW3-TYPING-RESPONSIVENESS.md).

### Calendar / To-dos screens

Calendar is a Monday-first Vietnamese month grid with a filled today cell and
an upcoming-events line. To-dos has a new-task input, Active/History filter,
checkbox rows, soft delete and restore. Storage is
`/var/lib/gleworks/todos.tsv` (64 tasks, 160 bytes each, atomic replace).

## 6. State and failure handling

- Status unavailable: battery `--`, Wi-Fi `Offline`, network `Status unavailable`.
- Empty reading state and empty task state are explicit, not blank cards.
- Scan in progress: `Scanning...`; no results: `No networks found`; failure:
  `Scan failed`.
- Long labels use ellipsis or controlled wrapping, never unclipped overflow.
- Reader launch failure is reported in a bounded status label.
- Lock/unlock failures are reported without changing the active screen.

## 7. Performance design

- Screens built once; updates are label text changes only.
- Clock changes only when the displayed minute changes; the date changes only
  when the day changes.
- One 30 s status poll (existing cadence), skipped while the lock screen is in
  front. Wi-Fi scan poll is the only immediate refresh.
- No animations, shadows, gradients or blur; cursor blinking is disabled.
- Focused text entry uses DU for the field and A2 for key highlights, each on
  its own small dirty region. Key A2 frames are prioritized; text DU is
  throttled. Dropdown popups use their own DU region.
- Distant dirty rectangles are never merged into one screen-sized bounding box.
- EPDC update failures retry on the next frame with bounded logging.
- Touch press/release frames are processed one per render cycle so key
  highlights are not overwritten before display.  A localized REAGL cleanup
  removes text-field ghosting after a short typing pause.
- LVGL color depth remains 8 bpp; the EPDC draw buffer remains 32 rows; dirty
  rectangle coalescing and waveform selection are unchanged.
- Launching KOReader still makes the shell exit, so Home cannot generate EPDC
  work while KOReader is foreground.
- LVGL heap is raised from the 48 KiB default to 256 KiB. Host measurement:
  119,192 bytes used (46 %), 1 % fragmentation after building all five screens.
  This is static service memory on a 512 MiB device.
- The ARM artifact grows from ~427 KiB to ~2.2 MiB because it embeds six
  Vietnamese-capable fonts. Fonts are read-only mapped text, not heap.
- Screen switches repaint the full screen once (expected for E-ink); steady
  idle state performs no EPDC update.

## 8. Verification assets

- Host simulator source: `os/ui/host_sim.c`
- Host port: `GW_UI_HOST_SIM` branch in `os/ui/lv_port_epdc.h`
- Preview command: `os/ui/sim/run-preview.sh` (fixed host clock:
  Saturday 31 October 2026, 09:41; device builds use `localtime()`)
- ARM build command: `os/ui/build-arm.sh`
- Screenshots, geometry report and artifact details:
  `docs/evidence/pw3-ui-redesign-2026-10-31/README.md`
