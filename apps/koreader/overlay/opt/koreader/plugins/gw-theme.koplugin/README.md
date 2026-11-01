# gw-theme.koplugin

KOReader adapter for the shared Gleworks E-ink tokens.

## What it changes

When enabled (default):

- reader footer font: `NotoSans-Regular.ttf`
- footer size/height/padding from `theme/tokens.json`
- thin progress bar, centered footer

From the reader menu → **Gleworks theme**:

- **Enable theme** — toggle footer styling; restores the previous footer values
  when disabled.
- **Apply reading typography** — one-shot font size, line spacing, L/R and T/B
  margins. This is deliberate: a theme must not silently rewrite every book.

## What it does not change

- page number/time/battery content choices
- KOReader menus, title bar, dialogs, FileManager
- document CSS
- `gw-shell` layout

Global menu/button/dialog styling needs a base-widget patch or a thin fork; it
is phase 2.

## Files

```text
gw-theme.koplugin/
├── _meta.lua
├── main.lua
└── gw_theme.lua   # generated from theme/tokens.json
```

Regenerate tokens from the repository root:

```sh
python3 theme/generate.py
python3 theme/generate.py --check
```

Do not edit `gw_theme.lua` by hand.

## Safety

- No device access at load time.
- All work is host-side until the finished battery session.
- Disable the plugin or toggle the theme off if a KOReader update changes footer
  internals.
