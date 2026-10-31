# System service IPC

Status: contract in progress. The current service is a host-tested stub and
must not be enabled as the hardware owner yet.

The command endpoint is `/run/gw-system.cmd`, a Unix stream socket. One
connection carries one ASCII request terminated by newline. The complete line,
including the newline, is at most 256 bytes. The service closes the connection
after one response. It rejects control characters, overlong lines and extra
data received in the same read after the newline. Clients must handle partial
reads and writes. A maximum of four command connections are served at once;
incomplete requests expire after approximately five seconds. Request data,
especially Wi-Fi credentials and book names, must not be written to logs.

The command names and response shapes are in `gw-ipc.h`. There is no arbitrary
command execution. Unsupported operations must return an error, never a false
success. Status values unavailable on this kernel must be represented as
unknown rather than guessed. Calls that alter power, networking, applications
or recovery must remain disabled until their individual paths are accepted.

On each connection the service obtains kernel `SO_PEERCRED` credentials. The
stub requires a valid peer PID and permits privileged requests only from UID 0;
it does **not** authorize the future shell. Before shell integration, the
service must launch and supervise the shell, retain its PID and expected UID,
and authorize that exact live process for the narrowly defined shell commands.
The service must revoke that authorization on exit, before accepting a new
shell. PID alone is not an identity: account for PID reuse and process exit;
do not trust a PID supplied inside a message. Define which commands root and
the foreground application may issue before enabling them. A compromised root
process is outside this UID boundary.

`/run/gw-system.evt` is reserved for service-to-shell notifications. Event
delivery, listener limits, backpressure, reconnection and shell authentication
are not implemented. The shell must recover current state with a status query
after reconnecting; events must not be treated as durable storage. No app may
consume a power-button event as evidence of a short press until real press and
release timing has been captured and validated on PW3.

## Wi-Fi scan contract

`NET RESULT <n>` returns one scanned network per index.  The current response
is `NET SSID <ssid_hex> <signal_dbm> <secure 0|1>`; `NET END` terminates the
list.  SSIDs are hex-encoded UTF-8 so spaces and non-ASCII names survive the
line protocol.  `signal_dbm` is the `wpa_cli scan_results` signal level and
`secure` is 1 for WPA/WPA2/RSN/WEP.  For compatibility the shell still accepts
the older one-field form `NET SSID <plain ssid>`, and rejects malformed hex in
the three-field form.  `NET STATUS` is separate and already returns a hex SSID
with the IPv4 address.

The PID file is also an advisory exclusive lock; the service holds it while it
owns the socket paths. It remains on disk after exit so a new process cannot
replace the lock inode while the old instance is shutting down. The process ID
inside it is diagnostic and is not itself proof that a process is alive.
