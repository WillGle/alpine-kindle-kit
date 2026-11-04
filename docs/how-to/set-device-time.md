---
diataxis: how-to
status: current
updated: 2026-11-03
verified_build: none
supersedes: none
audience: user
---

# Set device name, time and time zone

Use this when the clock, date or network name is wrong.

## Device name

**Settings → Device → Device name** opens a text editor. Save applies the name
with `sethostname` and stores it in `/var/lib/gleworks/device.conf`.

## Time zone

**Settings → Time & date → Time zone** opens a text editor. Enter a region
name such as `Asia/Ho_Chi_Minh`, then **Save**.

The image has no `tzdata` package. Region names are mapped to POSIX TZ strings
inside the service. Supported examples:

```text
UTC
Asia/Ho_Chi_Minh
Asia/Bangkok
Asia/Jakarta
Asia/Singapore
Asia/Shanghai
Asia/Hong_Kong
Asia/Tokyo
Asia/Seoul
Asia/Kolkata
Asia/Dubai
Europe/London
Europe/Paris
Europe/Berlin
America/New_York
America/Chicago
America/Los_Angeles
Australia/Sydney
Pacific/Auckland
```

An unknown region is rejected.

## Set date and time manually

**Settings → Time & date → Date and time** opens the current value in the form:

```text
YYYY-MM-DD HH:MM
```

Edit it and tap **Save**. The service interprets the value in the selected time
zone, sets the kernel clock and persists the epoch.

Use this when the device RTC is wrong and the network is unavailable.

## Sync from the network

**Settings → Time & date → Sync now** starts the existing `TIME SYNC` path.
It requires a connected network and returns immediately; the display refreshes
after the helper finishes.

The manual setting remains the fallback when NTP is unreachable.

## Verify

From the USB console:

```sh
gwctl time-get
gwctl status
```

`time-get` prints the epoch, time zone, 12/24-hour format and formatted local
time. `status` includes `time=` and `timezone=`.
