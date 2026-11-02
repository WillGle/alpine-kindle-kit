/* SPDX-License-Identifier: GPL-3.0-or-later
 * Gleworks system IPC contract — fixed command set between privileged system
 * service and unprivileged UI/app processes. Both sides must validate every
 * field; neither side executes arbitrary shell text.
 *
 * All messages are newline-terminated ASCII lines ≤ GW_IPC_MAX_LINE bytes.
 * Requests travel UI→service on the command socket; responses travel back
 * on the same connection.  Events travel service→UI on the event socket.
 *
 * Authentication: command and event sockets are created mode 0600, and the
 * service checks SO_PEERCRED on accept.  The supervised-shell PID check is
 * additional restriction, not a sandbox: a same-UID process can present any
 * PID.  Only root can connect to the sockets.  Event delivery is best-effort:
 * a listener that cannot accept a whole event line is dropped and must
 * re-query STATUS.
 *
 * This header is shared between the system service and the UI shell.
 * It must compile with C99 and no external dependencies.
 *
 * STATUS: CORE (in use).
 */
#ifndef GW_IPC_H
#define GW_IPC_H

#include <sys/types.h>

/* Filesystem paths created at runtime under /run/.  Overridable for host
 * integration tests. */
#ifndef GW_IPC_CMD_SOCKET
#define GW_IPC_CMD_SOCKET   "/run/gw-system.cmd"
#endif
#ifndef GW_IPC_EVT_SOCKET
#define GW_IPC_EVT_SOCKET   "/run/gw-system.evt"
#endif
#ifndef GW_IPC_PID_FILE
#define GW_IPC_PID_FILE     "/run/gw-system.pid"
#endif

/* Hard limits. */
#ifndef GW_IPC_MAX_LINE
#define GW_IPC_MAX_LINE     512   /* bytes including newline */
#endif
#ifndef GW_IPC_MAX_CLIENTS
#define GW_IPC_MAX_CLIENTS  4
#endif
/* Idle timeout for a persistent command connection. */
#ifndef GW_IPC_IDLE_SECONDS
#define GW_IPC_IDLE_SECONDS 30
#endif

/* Authorisation policy shared with host tests. */
static inline int gw_ipc_uid_privileged(unsigned long uid) {
    return uid == 0;
}

/* A listener is dropped when a write fails or only part of the event line was
 * accepted: a partial line would corrupt every later event. */
static inline int gw_ipc_drop_listener(ssize_t written, size_t length,
                                       int error) {
    (void)error;
    if (written < 0) return 1;
    return (size_t)written != length;
}

/* ── Commands (UI → service) ─────────────────────────────────────────────── */

/*
 * Power commands — require root or shell UID.
 *
 * SUSPEND
 *   Save foreground-app state, lock input, complete EPDC refresh, enter
 *   kernel mem suspend. Response: OK or ERR <reason>.
 *
 * POWEROFF
 *   Only available after hardware verification that power-button restart works.
 *   Not available until acceptance. Response: OK or ERR not-accepted.
 *
 * REBOOT
 *   Clean shutdown and reboot via proven shutdown helper.
 *   Response: OK (system reboots before a further reply arrives).
 *
 * RESCUE
 *   Remove boot.enabled, sync, reboot into p1 USB rescue.
 *   Response: OK (system reboots).
 */
#define GW_CMD_SUSPEND      "SUSPEND"
#define GW_CMD_POWEROFF     "POWEROFF"
#define GW_CMD_REBOOT       "REBOOT"
#define GW_CMD_RESCUE       "RESCUE"
#define GW_CMD_LOCK         "LOCK"
#define GW_CMD_UNLOCK       "UNLOCK"

/*
 * Frontlight commands.
 *
 * LIGHT SET <0-100>
 *   Set brightness percent. 0 = off. Response: OK or ERR <reason>.
 *
 * LIGHT GET
 *   Response: LIGHT <0-100>
 */
#define GW_CMD_LIGHT_SET    "LIGHT SET"
#define GW_CMD_LIGHT_GET    "LIGHT GET"

/*
 * App lifecycle — require root or shell UID.
 *
 * APP LAUNCH <id>
 *   Grant display/touch to app with given manifest ID; current foreground saves
 *   and yields first. Response: OK or ERR <reason>.
 *
 * APP EXIT
 *   Notify service that the foreground app has exited cleanly.
 *   Response: OK.
 *
 * APP CRASH <id>
 *   Notify service of foreground app crash; service reclaims display/touch and
 *   signals the UI shell to show the error screen.
 *   Response: OK.
 */
#define GW_CMD_APP_LAUNCH   "APP LAUNCH"
#define GW_CMD_APP_EXIT     "APP EXIT"
#define GW_CMD_APP_CRASH    "APP CRASH"

/*
 * Network — require root or shell UID.
 *
 * NET CONNECT <ssid_hex> <passphrase_hex>
 *   SSID and passphrase are hex-encoded UTF-8 to avoid shell quoting issues.
 *   SSID is 1–32 bytes; passphrase is 8–63 printable ASCII bytes.
 *   OK means connection was requested, not that Wi-Fi is connected.
 *
 * NET DISCONNECT
 *   Response: OK.
 *
 * NET STATUS
 *   Response: NET <connected|disconnected|unknown> [<ssid_hex>] [<ip>]
 *
 * NET SCAN
 *   Start an asynchronous scan. Response: OK or ERR <reason>.
 *
 * NET RESULT <n>
 *   Response: NET SSID <ssid_hex> <signal_dbm> <secure 0|1> for a scanned
 *   network, or NET END at the end of the list.  Older services returned the
 *   plain SSID in one field; the shell accepts both forms.
 *
 * NET FORGET
 *   Remove the saved network. Response: OK or ERR <reason>.
 */
#define GW_CMD_NET_CONNECT  "NET CONNECT"
#define GW_CMD_NET_DISCO    "NET DISCONNECT"
#define GW_CMD_NET_STATUS   "NET STATUS"
#define GW_CMD_NET_STATE    "NET STATE"
#define GW_CMD_NET_ENABLE   "NET ENABLE"
#define GW_CMD_NET_DISABLE  "NET DISABLE"
#define GW_CMD_NET_FORGET   "NET FORGET"

#define GW_CMD_TIME_GET     "TIME GET"
#define GW_CMD_TIME_SET     "TIME SET"
#define GW_CMD_TIMEZONE_GET "TIMEZONE GET"
#define GW_CMD_TIMEZONE_SET "TIMEZONE SET"
#define GW_CMD_TIME_FORMAT  "TIME FORMAT"
#define GW_CMD_TIME_SYNC    "TIME SYNC"

/*
 * Status query — no privilege required.
 *
 * STATUS
 *   Response: STATUS version=<ver> boot=<boot_id> power=<charging|battery|unknown>
 *             battery=<-1|0-100> light=<-1|0-100> foreground=<app_id|none|unknown>
 *             net=<connected|disconnected|unknown>
 *             usb=<none|power|host-class|unknown>
 *   -1 denotes an unavailable numeric measurement.
 *   host-class is the PMIC charger classification, not proof that a USB
 *   console has enumerated or that file transfer is active.
 */
#define GW_CMD_STATUS       "STATUS"

/* CAPS reports features enabled for normal use, never temporary spike flags. */
#define GW_CMD_CAPS         "CAPS"

/* ── Events (service → UI, broadcast) ───────────────────────────────────── */

/*
 * Events are broadcast to all connected event listeners. Each event is a
 * single newline-terminated line. Listeners must tolerate unknown events.
 *
 * POWER BUTTON SHORT     — short power-button press received
 * POWER CHARGING         — charger attached
 * POWER DISCHARGING      — charger removed
 * POWER BATTERY <0-100>  — periodic battery level update
 * POWER SUSPEND          — system is about to suspend (save state now)
 * POWER RESUME           — system resumed (restore display/input)
 * POWER LOCK             — display is dark and the lock screen must show
 * POWER WAKE             — frontlight restored, lock screen still required
 * POWER UNLOCK           — lock released; return to Home
 * NET UP <ssid_hex> <ip> — Wi-Fi associated
 * NET DOWN               — Wi-Fi disconnected
 * APP FOREGROUND <id>    — app has been granted display/touch
 * APP BACKGROUND         — no app has display/touch; shell is foreground
 * USB CONNECTED          — admin USB cable attached
 * USB DISCONNECTED       — admin USB cable removed
 */
#define GW_EVT_PWR_BTN_SHORT  "POWER BUTTON SHORT"
#define GW_EVT_PWR_CHARGING   "POWER CHARGING"
#define GW_EVT_PWR_DISCHARG   "POWER DISCHARGING"
#define GW_EVT_PWR_BATTERY    "POWER BATTERY"
#define GW_EVT_PWR_SUSPEND    "POWER SUSPEND"
#define GW_EVT_PWR_RESUME     "POWER RESUME"
#define GW_EVT_PWR_LOCK       "POWER LOCK"
#define GW_EVT_PWR_WAKE       "POWER WAKE"
#define GW_EVT_PWR_UNLOCK     "POWER UNLOCK"
#define GW_EVT_NET_UP         "NET UP"
#define GW_EVT_NET_DOWN       "NET DOWN"
#define GW_EVT_APP_FOREGROUND "APP FOREGROUND"
#define GW_EVT_APP_BACKGROUND "APP BACKGROUND"
#define GW_EVT_USB_CONNECTED  "USB CONNECTED"
#define GW_EVT_USB_DISCONNECTED "USB DISCONNECTED"

/* ── Response tokens ─────────────────────────────────────────────────────── */

#define GW_RESP_OK  "OK"
#define GW_RESP_ERR "ERR"

#endif /* GW_IPC_H */
