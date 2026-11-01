# Paint profile engine

The shell keeps one EPDC engine and selects a profile by screen/content type.
This matches how mainstream E-ink firmware works: a universal display driver,
per-app/content policy, plus optional periodic full refresh.

## Profiles

| Profile | Content | Live preview | Release/cleanup |
| --- | --- | --- | --- |
| `home` | static | none | no scroll cleanup |
| `settings` | text | **none** (moving glyphs smear) | one DU4 page pass; no second cleanup; full clean every 15 page snaps |
| `network` | list | 2 frames / 160 ms | partial GC16 on the touched area after 800 ms |
| `apps`, `update` | list | 2 frames / 160 ms | partial GC16 on the touched area after 800 ms |
| `notes` | text | none | one GC16 page pass; full clean every 15 page snaps |
| `calendar`, `lock` | static | none | explicit deep clean only |

`gw_paint_profile.c` is pure C and host-tested. `gw-shell.c` maps the active
screen to a profile and applies it before the first paint of that screen.

## Refresh policy

- Only the union of the actually touched area is cleaned; no full-screen GC16
  after scrolling.
- Brightness controls cancel any pending scroll cleanup.
- `GW_UI_FULL_REFRESH_AFTER=N` enables one full clean after N partial updates
  (`0` disables it; profiles default to off).
- Lock/unlock and first Home after boot keep the intentional deep clean.

## Limits

No 16-level waveform is flash-free; text-heavy screens therefore avoid live
drag frames rather than trying to clean them afterwards. Lists keep a small
number of previews for responsiveness.
