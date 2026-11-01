/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure power-button/suspend policy for the PW3 system service.
 *
 * This module contains no hardware access and no OS calls, so the decisions
 * can be unit tested on the host.  gw-system.c performs the resulting
 * actions through gw-power.h.
 *
 * STATUS: CORE (in use).
 */
#ifndef GW_POWER_POLICY_H
#define GW_POWER_POLICY_H

/*
 * Charger/USB classification, matching gw_power_read_usb():
 *   -1 unknown, 0 no cable, 1 power-only, 2 USB host (console class).
 */
#define GW_USB_UNKNOWN (-1)
#define GW_USB_NONE     0
#define GW_USB_POWER    1
#define GW_USB_HOST     2

typedef struct {
    int display_on;   /* 1: frontlight on and UI visible */
    int locked;       /* 1: lock screen is showing */
    int usb;          /* GW_USB_* */
    int charging;     /* -1 unknown, 0 discharging, 1 charging */
} gw_power_state_t;

typedef enum {
    GW_POWER_ACTION_NONE    = 0,
    GW_POWER_ACTION_LOCK    = 1 << 0,  /* turn light off, lock input */
    GW_POWER_ACTION_WAKE    = 1 << 1,  /* restore light, keep lock */
    GW_POWER_ACTION_SUSPEND = 1 << 2   /* kernel mem suspend is allowed */
} gw_power_action_t;

/*
 * Decide what a validated short power press should do.
 *
 * - Display on: lock.  If the cable is absent and the battery is not
 *   charging, also allow the caller to schedule kernel suspend.
 * - Display off: wake the panel/frontlight, but keep the lock screen.
 *
 * An unknown cable state never permits suspend: losing the administration
 * console is worse than one extra awake cycle, and an unknown charger state
 * is treated as "may be charging" for the same reason.
 */
int gw_power_on_short_press(const gw_power_state_t *state);

/*
 * Kernel suspend requires: display already off, no cable (or unknown only
 * when explicitly forced), and no known charging.  A power-only cable and a
 * USB-host cable both keep the device awake.
 */
int gw_power_should_suspend(const gw_power_state_t *state);

/* A cable attaches/detaches: returns 1 when a detached cable makes an
 * already-dark device eligible for suspend. */
int gw_power_on_usb_change(const gw_power_state_t *state);

/* True when the USB classification proves a cable is present. */
int gw_power_usb_present(int usb);

/* True when a state change is worth waking the service loop for. */
int gw_power_charging_changed(int previous, int current);

#endif /* GW_POWER_POLICY_H */
