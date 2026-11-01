# PW3 Alpine — Phase 3 CREngine Cache / Reopen Investigation

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
KOReader: pinned `v2026.07.1` (kindlepw2 bundle)  
Kernel: `3.0.35-lab126`  
Final boot ID: `a3028309-3468-4340-b81f-6b749caae64f`  
Scope: P0/P1 only; no CREngine source change deployed.

**Result: PASS as investigation, with a corrected baseline. The valid cache works.
The earlier 18.17 s "second opening" was not a warm open: it followed a forced
process kill that left a zero-byte cache file. A normal close writes an 829,440-byte
cache and reduces the same large EPUB to 5.4–6.4 s, with CRE render at 0.06–0.08 s.
No CREngine patch is justified.**

## 1. Runtime configuration

`DataStorage:getDataDir()` returns `"."` on this build, so CRE uses:

```
cache directory : ./cache/cr3cache  = /opt/koreader/cache/cr3cache
cache index     : cr3cache.inx
cache file      : P2-Moby-Dick.epub.f1467125.1.cr3
max cache       : 64 MB (default; no override in settings.reader.lua)
compression     : enabled (default)
storage factor  : 40 (default)
sidecar         : /mnt/us/documents/<book>.sdr/metadata.epub.lua
DOM version     : 20240114
```

The cache is on the ext3 root image, not on FAT. It is not user data and can be
regenerated. The cache filename contains the document/settings hash `f1467125`.

## 2. Instrumentation

A temporary user patch performed the same close path as the reader menu:

```lua
ReaderUI.instance:onClose(false)
UIManager:quit()
```

The patch logged `getCacheFilePath()`, `hasCacheFile()` and
`isCacheFileStale()` immediately before the close, and was removed after the
experiment. It is preserved as
`tools/maintenance/pw3-koreader-bench-patch.lua` but is not installed on the
device.

The device-side launcher is `tools/maintenance/pw3-reader-bench.sh`.

Measured markers:

- `opening file`
- `loading took ... seconds`
- `rendering took ... seconds`
- `Restoring user input handling`
- `BENCH_CLOSE cache=... has=... stale=...`
- cache file size and SHA-256
- sidecar `copt_font_size` and `cache_file_path`

## 3. Trials

The document was the same 814,023-byte EPUB
`1bc88e79ef7cf9d5325abe0071e6302265b708df56d3af07db124c81e84197f6`
at every trial. Times are launch-to-`Restoring user input`, i.e. first readable
page and input restored.

| Trial | Condition | Ready | CRE load | CRE render | Cache after close |
|---|---|---:|---:|---:|---|
| A | No cache; normal close | **22.26 s** | 1.278 s | **11.820 s** | 829,440 B |
| B | Warm; immediate restart | **5.89 s** | 0.204 s | **0.063 s** | 829,440 B |
| C | Warm; after clean reboot | **6.43 s** | 0.236 s | **0.076 s** | 829,440 B |
| D | Warm; unchanged typography/CSS | **5.57 s** | 0.204 s | **0.063 s** | 829,440 B |
| E | Font 22 → 24 | **18.12 s** | 0.226 s | **12.546 s** | rewritten |
| F1 | Valid cache, then `SIGKILL` | **5.59 s** | 0.213 s | 0.073 s | still 829,440 B |
| F2 | No cache, then `SIGKILL` at ready | 18.40 s | 1.280 s | 11.860 s | **0 B + index** |
| F3 | Reopen the 0-byte cache | 18.38 s | 1.280 s | 11.876 s | 829,440 B after normal close |

Notes:

- A was the first cold open immediately after manually clearing the cache; later
  cold opens measured 18.05–18.40 s. The 22.26 s sample includes extra
  first-run filesystem/library work.
- B, C and D establish cache persistence across application restart and reboot.
- E proves an intentional typography change invalidates/re-renders rather than
  silently reusing an incompatible cache.
- F1 proves a valid cache is not destroyed by a later forced kill.
- F2/F3 prove the failure mode: a kill during the **first** untracked open leaves
  a 0-byte `.cr3` plus an index entry; the next open logs
  `CRE: failed reading index from cache file` and must render again.

## 4. Cache lifecycle

Observed sequence:

1. No cache: CRE creates a destination file at first open, but does not write
   valid contents until close. If the process is killed before close, the file
   remains 0 bytes.
2. Normal `ReaderUI:onClose()`: `CloseDocument` runs, CRE writes the cache,
   updates `cr3cache.inx`, and writes the document sidecar. The final file was
   829,440 bytes.
3. Warm open: CRE reads the 97-byte index, validates the `.cr3`, loads it, and
   renders in 0.063–0.076 s.
4. Reboot: cache file and index hash were unchanged across
   `f4c54f56… → a3028309…`; warm open remained 6.43 s.
5. Font change: `copt_font_size` 22→24 produced a new hash/size and a full
   12.546 s render; restoring 22 later rebuilt the original cache.
6. A valid cache survives a subsequent forced kill; a cache that was never
   completed does not.

This is correct cache behavior, not a stale-cache bug. The zero-byte file is the
artifact of an interrupted first cache write.

## 5. Cache size and memory trade-off

| Item | Value |
|---|---:|
| Cache file | 829,440 B |
| Index | 97 B |
| Sidecar | 3,078 B |
| Warm RSS | 28,444 kB |
| Cold/warm peak RSS (Phase 2) | 70,176 kB / about 28.4 MB |
| Default cache cap | 64 MB |
| Root free at measurement | 74,006 kB |

At roughly 0.8 MB per large book, the default 64 MB cap could hold about 77
similar books. The root filesystem has only about 74 MB free, so accumulating a
full 64 MB cache would leave very little headroom. No cache-size increase was
made. A lower cap (for example 32 MB) would reduce cache hit rate but protect
the root; this is a storage-policy decision, not a measured Phase 3 bottleneck.

## 6. Targeted fix decision

**No CREngine or reader patch is justified by these measurements.**

- Normal exit already creates a correct persistent cache.
- Warm openings are 5.4–6.4 s and CRE render is below 0.08 s.
- The cold path remains 18–22 s because CRE must build the DOM/layout once.
- A zero-byte cache already falls back safely; deleting it at startup would not
  make the next open faster because there is no valid layout to reuse.
- The only real fix for an interrupted first open is to complete a normal close
  once, which writes the cache for all later openings.

A future, low-risk cleanup could remove zero-byte `.cr3` entries before launch to
avoid the one failed-index log line, but it would have no measurable latency
benefit and was not deployed.

## 7. Correctness checks

- Reading position and TOC: KOReader sidecar was written on normal close; no
  position/TOC errors were observed.
- Pagination: the same 1,359-page document count was used before and after the
  cache/font trials.
- Vietnamese: a separate 3-page EPUB containing `ă â đ ê ô ơ ư` and all five tone
  marks rendered correctly in the framebuffer (`/tmp/p3-vi2.png`); the UI
  language remains `vi`.
- No book content was modified. Temporary test books and their sidecars/caches
  were removed after measurement; the original library remains intact.

## 8. Reproduction

Install the temporary patch only for controlled trials:

```sh
mkdir -p /opt/koreader/patches
cp tools/maintenance/pw3-koreader-bench-patch.lua \
   /opt/koreader/patches/2-bench-exit.lua
```

Then, with `gw-system` stopped:

```sh
# cold: remove only the generated CRE cache first
rm -f /opt/koreader/cache/cr3cache/*
GW_BENCH_EXIT_AFTER=25 \
  sh tools/maintenance/pw3-reader-bench.sh -d /mnt/us/documents/P2-Moby-Dick.epub

# warm, immediate restart
GW_BENCH_EXIT_AFTER=25 \
  sh tools/maintenance/pw3-reader-bench.sh -d /mnt/us/documents/P2-Moby-Dick.epub
```

Remove `/opt/koreader/patches/2-bench-exit.lua` after the experiment.

Raw evidence:

- `PW3-PHASE3-CRE-cache-audit.txt`
- `PW3-PHASE3-CRE-cache-detail.txt`
- `PW3-PHASE3-CRE-trial-A.txt`
- `PW3-PHASE3-CRE-trial-B.txt`
- `PW3-PHASE3-CRE-trial-C.txt`
- `PW3-PHASE3-CRE-trial-D.txt`
- `PW3-PHASE3-CRE-trial-E.txt`
- `PW3-PHASE3-CRE-trial-F-warm.txt`
- `PW3-PHASE3-CRE-trial-F-invalid.txt`
- `PW3-PHASE3-CRE-warm-io.txt`

## 9. Status

**P0: PASS.** The repeated slow-open cause is identified: no valid cache was
persisted because the process was killed before a normal close.

**P1: PASS, no code change.** Valid cache warm opens are 5.4–6.4 s, a 66–70 %
reduction from 18.2 s; cold open remains 18–22 s by design. Cache size is
bounded by the existing 64 MB default and protected by the root free-space
constraint.
