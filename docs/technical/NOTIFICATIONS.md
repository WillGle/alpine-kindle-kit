# GleWorks notifications and failure journal

Standalone notifications share a centered white panel with a black border,
GleWorks INFO/ERROR heading, action name, fixed content area and page counter.
The panel uses 48 columns and 20 rows; FBInk draws that rectangle without clearing
the screen. Long text continues in the same panel (eight seconds on intermediate
pages, the existing app-specific hold on the last page). UTF-8 characters and
long login links are not split within a character or silently discarded.
If FBInk fails, eips draws a framed panel in a fixed bounded rescue area; it also
never clears the screen. Native KOReader notifications use a fixed centered,
framed, non-draggable InfoMessage and can be dismissed by touch. Physical font,
contrast and placement still require an actual-device check.

Standalone hold times remain: Transfer Key 90 seconds, Status/errors 30 seconds,
VPN login links 180 seconds, simple success feedback ten seconds. Completion
requests Home and redraw. There is no new background service or notification
polling; standalone panels use the existing timed display rather than adding a
touch input handler.

## One persistent location

On USB: `Kindle/gleworks-logs/`
On the device: `/mnt/us/gleworks-logs/`

- `errors.log`: structured failure journal, UTC timestamp, controlled action,
  nonzero numeric exit code and safe category.
- `errors.log.1`: one previous journal, rotated at 256 KiB. Each entry is small;
  current plus previous journal uses about 512 KiB in total.
- `install-*.log` and `app-update-*.log`: detailed installation/update metadata
  for manually invoked operations, now in this same directory. Older historical
  logs at the USB root are left in place.

Example: `2026-10-29T08:33:36Z action=vpn-login exit=1 category=launcher`

The journal records managed launcher/control failures, native callback failures,
HTTP error statuses, failed health checks, activation failures and renderer/Home/
redraw failures. One failure can have multiple entries for its different layers.
It never records popup text, request paths/headers/bodies, login URLs, tokens,
book or calendar titles, account names or peers. Detailed VPN daemon/login logs
stay root-private under `/tmp/gleworks` and are not copied to shared reports.
The device clock supplies timestamps. The journal is outside managed release
folders, so updates, restores and normal reboots retain it.

**GleWorks System Inspection** automatically finds both journal files, checks
size/schema, and includes the latest 100 valid events across them in its existing
`gleworks-inspection-*.txt` report. Invalid lines are omitted, not copied into the
report. Run Inspect, then reconnect USB to review that report and the central
folder. Status remains the shorter service report.

Power loss/SIGKILL cannot run exit cleanup; full/unwritable storage or inaccessible
installed code can prevent logging. These are local diagnostics, not an audit
trail that can establish firmware integrity. No logging of arbitrary commands
entered into the Terminal or instrumentation of every upstream reader action is
added.

## Executed checks for this revision

Disposable probes verified: all 13 launchers' failure paths reach the journal;
INFO/ERROR headings, fixed panel geometry and no full-screen clear; Unicode/long
links and paging; renderer fallback and cleanup; native callbacks without private
payloads; logger argument validation, rotation, concurrent writes and retention
when /tmp is removed; Inspect's filtering and 100-event limit; 12 HTTP behaviors
with real native file IO and socket/LFS adapters; actual control failure traps;
complete update/restore retention and activation-failure logging; copied connected
revision -> 41-file patch -> repeat with preferences/data retained. Python, shell,
Lua and JavaScript syntax checks passed. All scripts/probes ran locally with
appropriate device adapters; no physical rendering or live network acceptance is
claimed. No maintained tests or device writes were added by this session.
