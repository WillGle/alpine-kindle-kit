---
diataxis: how-to
status: draft
updated: 2026-11-02
verified_build: none
supersedes: none
audience: user
---

# Customise the lock screen with KOReader

Status: **trial shell deployed; the KOReader menu export still needs hardware
acceptance.** The shell and plugin are built together; do not deploy only one
half.

Contract: [`../reference/lock-screen.md`](../reference/lock-screen.md).

## 1. Choose the sleep screen in KOReader

Open KOReader → **File manager** or **Reader** menu → **Screensaver** (or
**Sleep screen**) and choose one of:

- book cover;
- custom image or cover;
- random image from a folder;
- reading progress or book status, if available.

This is the image the lock screen will use.

## 2. Export it

In KOReader open **Gleworks lock screen → Use current sleep screen**.

The plugin renders the sleep screen to:

```text
/mnt/us/gleworks/lockscreen.raw
/mnt/us/gleworks/lockscreen.conf
```

A notification confirms the update. If the chosen mode has no image, the plugin
falls back to text only.

## 3. Adjust the text

From the same menu:

- **Show clock** — toggle the time.
- **Show date** — toggle the date.
- **Set message...** — one line under the date.
- **Text only** — remove the image and keep the white card.

Changes are written atomically and the shell reloads them while the lock screen
is visible.

## 4. Verify on the device

Lock the device with the power button, or run:

```sh
gwctl lock
```

Expected:

- the image covers the panel;
- the white card shows clock, date, message and **Tap to unlock**;
- tapping anywhere unlocks;
- no Back control appears.

If the image is missing or the wrong size, the plain lock screen returns.

## 5. Reset

Choose **Gleworks lock screen → Text only** and clear the message. To remove all
customisation, delete both files from a root console:

```sh
rm -f /mnt/us/gleworks/lockscreen.conf /mnt/us/gleworks/lockscreen.raw
```

The shell then uses the default clock and date.

## 6. Troubleshooting

| Symptom | Check |
| --- | --- |
| No image | `use_image=1` in the config; raw file is exactly 1552256 bytes |
| Image stale | Re-run **Use current sleep screen**; the shell caches by mtime |
| Menu missing | `GW_MANAGED_READER=1`; plugin installed under `opt/koreader/plugins/gw-lockscreen.koplugin` |
| Text unreadable | Use **Text only** or a lighter image; the card is white |

## Limits

- The plugin must run in the managed reader; a standalone KOReader is not
  changed.
- `/mnt/us` is FAT, so the image and message are readable over USB. Do not put
  secrets in the message.
- The lock screen is portrait; a landscape image is letterboxed.
