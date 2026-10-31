# Gleworks theme tokens

One token source for the two UI stacks on the PW3:

- `os/ui/` — C/LVGL shell (`gw-shell`)
- `apps/koreader/overlay/` — KOReader plugins and patches

The tokens are the design contract. Do not copy colour or spacing literals into
either adapter; add a token here and regenerate.

## Source of truth

`theme/tokens.json` holds:

- canvas and spacing
- grayscale colours
- type scale
- radii and hairlines
- font families
- KOReader reader defaults

All colours must be grayscale (`#RRGGBB` with `R == G == B`). The generator
rejects anything else so the E-ink UI cannot accidentally depend on colour.

## Generate

```sh
python3 theme/generate.py
python3 theme/generate.py --check
```

Outputs:

```text
os/ui/gw_theme_tokens.h
apps/koreader/overlay/opt/koreader/plugins/gw-theme.koplugin/gw_theme.lua
```

`--check` is suitable for CI or a pre-commit hook. It fails when either adapter
is stale.

## How the adapters use tokens

### gw-shell

`os/ui/gw_ui_style.h` includes `gw_theme_tokens.h` and keeps only the LVGL style
helpers. Existing `GW_UI_*` layout and `gw_ui_c_*()` colour names remain stable.

### KOReader

`gw-theme.koplugin` requires its generated `gw_theme.lua` and applies:

- footer font, height, padding, thin progress bar
- optional one-shot reading typography (font size, line spacing, margins)

Reader-side scaling uses the physical design width, not raw KOReader units:

```lua
local function px(n)
    return Screen:scaleBySize(n * 600 / 1072)
end
```

That keeps 48 px in the design equal to 48 px on the PW3 instead of relying on
KOReader's density-independent scale.

## Precedence

A theme must not silently win over an explicit user choice:

```text
theme defaults
  < user global settings
  < per-book settings
  < explicit per-widget overrides
```

The KOReader plugin therefore:

- applies footer presentation automatically while enabled;
- keeps content options (page number, time, battery) under KOReader's control;
- makes reading typography a deliberate one-shot action, not a forced rewrite;
- can be disabled from the reader menu.

## Scope

Phase 1 (this foundation):

- shared tokens and generated adapters
- KOReader footer look
- opt-in reading typography
- no changes to `gw-shell` layout or behaviour

Phase 2 (not in this change):

- KOReader title bar, menu, button and dialog adapters
- FileManager row styling
- optional custom dashboard

Global KOReader skinning requires base-widget patches or a thin fork. Do not
pretend a plugin can restyle every third-party screen.

## Verification

```sh
python3 theme/generate.py --check
make -C os/ui host-sim
make -C os/ui preview
```

The host simulator renders the real LVGL tree at 1072 × 1448 and does not touch
`/dev/fb0`, evdev, EPDC, or the physical Kindle. KOReader host rendering needs a
separate emulator/harness; device acceptance remains pending until the battery
session is finished.
