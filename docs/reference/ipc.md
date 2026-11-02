---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# System service IPC

Status: contract in progress. The current service is a host-tested stub and
must not be enabled as the hardware owner yet.

The command endpoint is `/run/gw-system.cmd`, a Unix stream socket created
mode 0600. One connection carries one or more ASCII requests, each terminated
by newline; `QUIT` closes the connection. The complete line, including the
newline, is at most 256 bytes. The service rejects control characters,
overlong lines and requests with no newline. Clients must handle partial reads
and writes. A maximum of four command connections are served at once; an idle
connection expires after approximately 30 seconds. Request data, especially
Wi-Fi credentials and book names, must not be written to logs. The shell keeps
one connection for its lifetime and reconnects after an error.

The command names and response shapes are in `gw-ipc.h`. There is no arbitrary
command execution. Unsupported operations must return an error, never a false
success. Status values unavailable on this kernel must be represented as
unknown rather than guessed. Calls that alter power, networking, applications
or recovery must remain disabled until their individual paths are accepted.

The Kindle hub link is a separate socket (`/run/gw-hub.sock`, mode 0600) and is
not part of this command/event protocol; see
[`kindle-hub-protocol.md`](kindle-hub-protocol.md).

On each connection the service obtains kernel `SO_PEERCRED` credentials. The
0600 socket already limits connections to UID 0. Command policy is declared in
one table (`os/system/gw-command-policy.h`) and applied once by the dispatcher:
root-only, supervised-shell-only and locked-state-denied. The service launches
and supervises the shell, retains its PID and grants the narrowly defined
shell commands to that exact live process. PID equality is defense in depth,
not a sandbox: a same-UID process can present any PID. Do not trust a PID
supplied inside a message. A compromised root process is outside this UID
boundary.

`/run/gw-system.evt` is reserved for service-to-shell notifications. Event
delivery, listener limits, backpressure, reconnection and shell authentication
are not implemented. The shell must recover current state with a status query
after reconnecting; events must not be treated as durable storage. No app may
consume a power-button event as evidence of a short press until real press and
release timing has been captured and validated on PW3.

## Wi-Fi scan contract

`NET SCAN RESULTS` returns the whole scan in one bounded multi-line response:
`NET RESULTS <count>`, then one `NET SSID <ssid_hex> <signal_dbm> <secure 0|1>`
line per AP, then `NET END`. `NET RESULT <n>` remains for compatibility and
returns one AP per index. SSIDs are hex-encoded UTF-8 so spaces and non-ASCII
names survive the line protocol. `signal_dbm` is the `wpa_cli scan_results`
signal level and `secure` is 1 for WPA/WPA2/RSN/WEP. The shell rejects
malformed hex. `NET STATUS` is separate and already returns a hex SSID with
the IPv4 address.

`APP LIST` returns every installed app in one response (`APP <id> <version>
<description>` per line, terminated by `APP END`). The older `APP COUNT` and
`APP INFO <n>` commands remain for diagnostics.

The PID file is also an advisory exclusive lock; the service holds it while it
owns the socket paths. It remains on disk after exit so a new process cannot
replace the lock inode while the old instance is shutting down. The process ID
inside it is diagnostic and is not itself proof that a process is alive.
