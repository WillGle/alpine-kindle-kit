/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared date-name tables.  Kept in one translation unit so headers no
 * longer duplicate read-only arrays in every includer. */
#include "gw_ui_strings.h"

/* Sunday-first, matching struct tm.tm_wday and the calendar's 0..6 loop. */
const char *const gw_str_weekday[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
    "Saturday"
};
const char *const gw_str_weekday_short[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
const char *const gw_str_month[12] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};
const char *const gw_str_month_short[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
    "Oct", "Nov", "Dec"
};
