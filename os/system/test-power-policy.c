/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for the suspend/lock/wake policy. */
#include "gw-power-policy.h"

#include <stdio.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static gw_power_state_t state(int display_on, int locked, int usb, int charging) {
    gw_power_state_t s;
    s.display_on = display_on;
    s.locked = locked;
    s.usb = usb;
    s.charging = charging;
    return s;
}

int main(void) {
    gw_power_state_t s;

    /* Short press while reading on battery: lock and sleep. */
    s = state(1, 0, GW_USB_NONE, 0);
    CHECK(gw_power_on_short_press(&s) ==
          (GW_POWER_ACTION_LOCK | GW_POWER_ACTION_SUSPEND));
    CHECK(gw_power_should_suspend(&s) == 0);
    /* A known or unknown charging state blocks suspend; only a proven
     * discharging state permits it. */
    CHECK(gw_power_on_short_press(&(gw_power_state_t){1, 0, GW_USB_NONE, 1}) ==
          GW_POWER_ACTION_LOCK);
    CHECK(gw_power_on_short_press(&(gw_power_state_t){1, 0, GW_USB_NONE, -1}) ==
          GW_POWER_ACTION_LOCK);

    /* Short press while reading on a cable: lock only. */
    s = state(1, 0, GW_USB_HOST, 1);
    CHECK(gw_power_on_short_press(&s) == GW_POWER_ACTION_LOCK);
    s = state(1, 0, GW_USB_POWER, 1);
    CHECK(gw_power_on_short_press(&s) == GW_POWER_ACTION_LOCK);
    s = state(1, 0, GW_USB_UNKNOWN, -1);
    CHECK(gw_power_on_short_press(&s) == GW_POWER_ACTION_LOCK);

    /* Short press while dark always wakes, never re-suspends immediately. */
    s = state(0, 1, GW_USB_NONE, 0);
    CHECK(gw_power_on_short_press(&s) == GW_POWER_ACTION_WAKE);
    s = state(0, 1, GW_USB_HOST, 1);
    CHECK(gw_power_on_short_press(&s) == GW_POWER_ACTION_WAKE);

    /* Suspend gate: dark, no cable, not charging. */
    CHECK(gw_power_should_suspend(&(gw_power_state_t){0, 1, GW_USB_NONE, 0}) == 1);
    /* Contract: charging or unknown charging keeps the device awake. */
    CHECK(gw_power_should_suspend(&(gw_power_state_t){0, 1, GW_USB_NONE, 1}) == 0);
    CHECK(gw_power_should_suspend(&(gw_power_state_t){0, 1, GW_USB_NONE, -1}) == 0);
    CHECK(gw_power_should_suspend(&(gw_power_state_t){0, 1, GW_USB_POWER, 0}) == 0);
    CHECK(gw_power_should_suspend(&(gw_power_state_t){0, 1, GW_USB_HOST, 0}) == 0);
    CHECK(gw_power_should_suspend(&(gw_power_state_t){0, 1, GW_USB_UNKNOWN, 0}) == 0);
    CHECK(gw_power_should_suspend(&(gw_power_state_t){1, 0, GW_USB_NONE, 0}) == 0);

    /* Cable removal on a dark, locked device schedules suspend. */
    s = state(0, 1, GW_USB_NONE, 0);
    CHECK(gw_power_on_usb_change(&s) == GW_POWER_ACTION_SUSPEND);
    s = state(0, 1, GW_USB_NONE, 1);
    CHECK(gw_power_on_usb_change(&s) == 0);
    s = state(0, 1, GW_USB_POWER, 0);
    CHECK(gw_power_on_usb_change(&s) == 0);
    s = state(0, 0, GW_USB_NONE, 0);
    CHECK(gw_power_on_usb_change(&s) == 0);
    s = state(1, 0, GW_USB_NONE, 0);
    CHECK(gw_power_on_usb_change(&s) == 0);

    /* Cable classification. Unknown is not a proven cable. */
    CHECK(gw_power_usb_present(GW_USB_NONE) == 0);
    CHECK(gw_power_usb_present(GW_USB_UNKNOWN) == 0);
    CHECK(gw_power_usb_present(GW_USB_POWER) == 1);
    CHECK(gw_power_usb_present(GW_USB_HOST) == 1);

    /* Charging edges are only meaningful when both samples are known. */
    CHECK(gw_power_charging_changed(0, 1) == 1);
    CHECK(gw_power_charging_changed(1, 1) == 0);
    CHECK(gw_power_charging_changed(-1, 1) == 0);
    CHECK(gw_power_charging_changed(0, -1) == 0);

    /* NULL is rejected without side effects. */
    CHECK(gw_power_on_short_press(NULL) == GW_POWER_ACTION_NONE);
    CHECK(gw_power_should_suspend(NULL) == 0);
    CHECK(gw_power_on_usb_change(NULL) == 0);

    if (failures) {
        printf("Power policy tests: %d failures\n", failures);
        return 1;
    }
    printf("Power policy tests: PASS\n");
    return 0;
}
