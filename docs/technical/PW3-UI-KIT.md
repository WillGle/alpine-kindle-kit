# PW3 UI kit and visual system

Date: 2026-10-31  
Status: shell kit deployed; UI frozen for stability work.  
Scope: `os/ui`, `theme/tokens.json`, KOReader theme adapter.

## 1. Design principle

The device is an appliance, not a desktop. The UI should be:

- predictable: one obvious reading action;
- offline-first: no network dependence for Home/Library;
- low-anxiety: few screens, large targets, no decorative animation;
- consistent: one token source, one component vocabulary;
- recoverable: every deployed build has a known-good predecessor.

Cosmetic changes are frozen. Only correctness, readability, power and
recovery fixes should change the UI from here.

## 2. One source of truth

`theme/tokens.json` drives:

```text
theme/generate.py
  ├── os/ui/gw_theme_tokens.h          (LVGL)
  └── apps/koreader/.../gw_theme.lua   (KOReader)
```

Tokens cover:

- grayscale colours
- type scale
- spacing
- radii
- stroke width
- reader typography metrics

Regenerate:

```sh
python3 theme/generate.py
python3 theme/generate.py --check
```

The `--check` mode is the drift gate: C and Lua adapters must match the JSON.

## 3. Shell UI kit

`os/ui/gw_ui_kit.c/.h` is the LVGL adapter. Pages compose kit primitives instead
of repeating margins, fonts, borders and button styles.

| Primitive | Purpose |
|---|---|
| `gw_ui_kit_page_create` | full-window page with token background |
| `gw_ui_kit_content_create` | standard 48 px margin and 24 px row rhythm |
| `gw_ui_kit_card_create` | card fill, 2 px border, radius |
| `gw_ui_kit_hairline_create` | low-noise separator |
| `gw_ui_kit_app_bar_create` | Back + title + hairline |
| `gw_ui_kit_bottom_nav_create` | Home / Books / Settings |
| `gw_ui_kit_section_label_create` | bold 24 px section headings |
| `gw_ui_kit_button_create` | primary/secondary 96 px minimum target |
| `gw_ui_kit_row_create` | 96+ px list row with bottom hairline |
| `gw_ui_kit_checkbox_create` | 96 px checkbox target with E-ink states |
| `gw_ui_kit_style_keyboard` | E-ink keyboard states |
| `gw_ui_kit_style_dropdown_list` | bounded popup list styling |

`gw-shell.c` keeps page-specific composition only: cards, reading state,
calendar grid, to-do list and network controls.

## 4. Visibility pass

The E-ink ramp was increased without moving any element:

| Token | Before | After |
|---|---|---|
| primary text | `#141414` | `#0A0A0A` |
| secondary | `#545454` | `#303030` |
| muted | `#808080` | `#404040` |
| border | `#B0B0B0` | `#707070` |
| card fill | `#F8F8F8` | `#EEEEEE` |

Additional changes:

- section labels use bold 24 px `gw_font_24_bold`;
- cards, secondary buttons, text areas and popup lists use a 2 px border;
- weekday headers use the bold caption font;
- primary/secondary text remains high contrast in 8-bit grayscale.

The layout, sizes and positions are unchanged. All seven host render hashes
matched the pre-kit baseline exactly after the kit migration.

## 5. Cross-app strategy

Do not force one toolkit across all apps.

- **LVGL apps**: link `gw_ui_kit.c` and the generated C tokens.
- **KOReader/Lua apps**: use the generated `gw_theme.lua`; share colours,
  spacing, type and component metrics, not LVGL widgets.
- **Non-GUI apps**: no dependency.

KOReader remains the reading engine. The shell owns Home, Library, Calendar,
To-dos and Network. The return-to-Home plugin keeps navigation predictable.

## 6. Verification

- Host preview: `os/ui/sim/run-preview.sh`
- Geometry/state checks: **596 checks, 0 failures**
- Render comparison: Home, Settings, Calendar, To-dos, Network, Home empty,
  To-dos history and Network popup are byte-identical to the pre-kit baseline.
- ARM build: static ARM/musl, no dynamic section.
- Deployed with `/usr/sbin/gw-shell.before-ui-kit` rollback retained.

## 7. Next work: stability, not visual churn

1. Verify suspend/wake and battery behaviour with the frozen shell.
2. Keep Wi-Fi optional; do not make Home depend on it.
3. Add a real Library/Continue Reading source from KOReader sidecar metadata
   only if it can be bounded and read-only.
4. Keep the current shell token/component set stable.
5. Every future build: one change set, host checks, ARM hash, backup, device
   smoke test, then stop.
