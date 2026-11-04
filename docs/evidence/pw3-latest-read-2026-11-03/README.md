---
status: current
updated: 2026-11-03
verified_build: 990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069
supersedes: none
audience: maintainer
---

# Home latest-read metadata (2026-11-03)

Status: **device-verified.**

## Problem

Home always showed **No book open** after a PDF was opened in KOReader. The
shell had no book metadata provider, so the empty state was hard-coded.

## Fix

The shell now reads the latest book from KOReader, in this order:

1. `settings.reader.lua` → `lastfile`, when it names a supported book;
2. newest supported entry in `history.lua`;
3. newest sidecar metadata as a last-resort fallback.

The selected book's `.sdr/metadata.<ext>.lua` supplies title, author and
`percent_finished`. Images are excluded from the book selection so a viewed
PNG does not replace the last book. No Lua is executed; only the documented
string/number keys are parsed.

## Device result

After opening `Shin cậu bé bút chì T01.pdf` and returning to Home:

```text
title    Shin cậu bé bút chì T01
author   Icecream PDF Converter
progress 24%
```

The card is shown in
[`screenshots/home-latest-read.png`](screenshots/home-latest-read.png).

## Build

`gw_shell` SHA-256:
`990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069`

Boot ID `6986b82c-62b7-4ef5-ae98-390e55cf339f` was unchanged.

## Limits

- The sidecar format is KOReader-owned. A future KOReader version that renames
  the keys silently falls back to the newest sidecar or the empty state.
- The card is a snapshot taken when the shell starts; it refreshes after the
  reader exits and the supervisor restarts the shell.
- No cover thumbnail or reading position beyond the percentage.
