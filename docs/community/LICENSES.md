# Source and redistribution

The public-source packager includes project source and documentation only.
It includes no compiled kernel module, reader binary, root image, captured
Amazon library/firmware, waveform, stock init script, device key or token.

- New installer Python tooling and accompanying community documentation are
  provided under GPL-3.0-or-later. The full [GPL v3 text](licenses/GPL-3.0.txt)
  is included. Existing per-file notices remain authoritative.
- `bootstrap/bootstrap.c` and `shutdown.c` carry GPL-3.0-or-later notices.
  Bootstrap's pivot pattern acknowledges ChrisPei/PaperOS, commit
  `8dc24d6278c120eb9c69ceddc470f62eb15cbb79`,
  `https://github.com/ChrisPei/PaperOS`; retain its attribution.
- `gleworks_pid1_reset.c`, `reset-protocol.h`, `sys.h` explicitly specify
  GPL-2.0-only. Related guardian support retains those notices; this separate
  kernel module is not relicensed as GPL v3. The included
  [Linux COPYING](licenses/Linux-COPYING.txt) records the GPL v2 text and
  syscall boundary. Aggregating separate sources does not change their licenses.
- KOReader (AGPL v3, as recorded in its release COPYING) is acquired separately from its official release, with its
  COPYING, dependency/font/translation licenses intact. The native Lua wrapper
  adapts that reader; preserve the applicable KOReader licensing obligations.
- Alpine, BusyBox, musl, GCC and the vendor kernel sources each retain their
  upstream terms. Record the exact versions/checksums you use. They are build
  inputs, not embedded in this public source archive.
- Capturing a component from your own device is not a grant to redistribute
  it. Private candidate images include captured compatibility libraries and
  modules; do not publish them without a separate distribution review.

No project-wide license statement overrides a third party's rights. Before
publishing new binary packages, include the corresponding source and notices
required by their licenses, and resolve any remaining vendor distribution
rights. The source-only package avoids treating today's private image as a
public binary release.
