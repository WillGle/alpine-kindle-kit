---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Capture evidence

Evidence is only useful if its provenance is explicit: date, scope, build hash,
boot ID and whether it is current or superseded.

## Capture a screen

```sh
python3 tools/maintenance/capture-screen.py
```

The helper reads the current framebuffer. It is not a photograph of the panel;
record that limit in the evidence note. Prefer the generated `work/private/`
path and copy only redacted files into `docs/evidence/`.

## Capture logs

```sh
python3 tools/lib/usb_console.py <<'SH'
cat /proc/sys/kernel/random/boot_id
gwctl status
tail -n 200 /run/gw-system.log
tail -n 200 /run/reader.log
SH
```

Do not put host-specific absolute paths, tokens, keys, serials or private
session data in current evidence. Raw private captures belong in `work/private/`.

## Add a current evidence set

1. Create a dated directory or file under `docs/evidence/`, for example
   `pw3-scope-2026-11-02/` with a `README.md`.
2. Add front matter to the Markdown file:

   ```yaml
   ---
   status: current
   updated: 2026-11-02
   verified_build: none
   supersedes: none
   audience: maintainer
   ---
   ```

3. Add an entry to [`../evidence/INDEX.json`](../evidence/INDEX.json) with
   `path`, `date`, `scope`, `build`, `boot_id`, `status` and `supersedes`.
4. Regenerate and check the index:

   ```sh
   python3 tools/docs/check.py --write
   python3 tools/docs/check.py
   ```

When a set is superseded, move it with `git mv` to
[`../history/evidence/`](../history/evidence/), mark it `status: superseded`,
and point `superseded_by` at the replacement or at `../status/CURRENT.md`.
