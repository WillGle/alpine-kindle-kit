/* SPDX-License-Identifier: GPL-3.0-or-later
 * STATUS: CORE (in use). */
#include "gw-power-policy.h"

int gw_power_usb_present(int usb) {
    /* Unknown is deliberately treated as "possibly present" by callers that
     * gate suspend; this helper only reports a proven cable. */
    return usb == GW_USB_POWER || usb == GW_USB_HOST;
}

/* Cable/charger gate shared by every suspend decision.
 *
 * When /proc/asession proves there is no cable, ignore the charger online bit:
 * the PMIC can report "charging" for several seconds after unplug and that
 * stale flag previously blocked a real button-triggered suspend.  With a
 * cable present, charging wins; with an unknown cable state, never suspend
 * while charging is known or unknown. */
static int suspend_power_ok(const gw_power_state_t *state) {
    if (!state) return 0;
    /* Only a proven absent cable permits suspend.  The charger online bit can
     * stay set for several seconds after unplug; cable presence is the
     * authority, so a stale "charging" value must not keep Wi-Fi and Tailscale
     * awake on battery.  A present or unknown cable keeps the administration
     * path available. */
    return state->usb == GW_USB_NONE;
}

int gw_power_should_suspend(const gw_power_state_t *state) {
    if (!state || state->display_on) return 0;
    return suspend_power_ok(state);
}

int gw_power_on_short_press(const gw_power_state_t *state) {
    if (!state) return GW_POWER_ACTION_NONE;
    if (state->display_on) {
        /* The caller darkens the panel first; the cable gate alone decides
         * whether the press may continue into kernel suspend. */
        int action = GW_POWER_ACTION_LOCK;
        if (suspend_power_ok(state)) action |= GW_POWER_ACTION_SUSPEND;
        return action;
    }
    return GW_POWER_ACTION_WAKE;
}

int gw_power_on_usb_change(const gw_power_state_t *state) {
    if (!state) return 0;
    /* A cable removal cannot create eligibility on its own if the user still
     * sees the UI: only a locked, dark device sleeps. */
    if (state->display_on || !state->locked) return 0;
    return suspend_power_ok(state) ? GW_POWER_ACTION_SUSPEND : 0;
}

int gw_power_charging_changed(int previous, int current) {
    if (previous < 0 || current < 0) return 0;
    return previous != current;
}
