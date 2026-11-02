/* SPDX-License-Identifier: GPL-3.0-or-later
 * Date and calendar helpers for the gw-shell UI. */
#include "gw_shell_internal.h"

GW_SHELL_INTERNAL void gw_ui_now(struct tm *out) {
#if GW_UI_HOST_SIM
    /* Deterministic preview clock: Saturday, 31 October 2026, 09:41. */
    memset(out, 0, sizeof(*out));
    out->tm_year = 126;      /* 2026 */
    out->tm_mon = 9;         /* October */
    out->tm_mday = 31;
    out->tm_hour = 9;
    out->tm_min = 41;
    out->tm_wday = 6;        /* Saturday */
    out->tm_yday = 303;
#else
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) *out = *t;
    else memset(out, 0, sizeof(*out));
#endif
}

GW_SHELL_INTERNAL void format_hero_date(char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    if (t.tm_year == 0) {
        snprintf(buf, size, "%s", GW_STR_DATE_UNAVAILABLE);
        return;
    }
    snprintf(buf, size, "%s, %d %s %d",
             gw_str_weekday[t.tm_wday], t.tm_mday,
             gw_str_month[t.tm_mon], t.tm_year + 1900);
}

GW_SHELL_INTERNAL void format_month_title(int year, int month, char *buf, size_t size) {
    if (month < 0 || month > 11) {
        snprintf(buf, size, "%d", year);
        return;
    }
    snprintf(buf, size, "%s %d", gw_str_month[month], year);
}

GW_SHELL_INTERNAL void format_iso_date(int year, int month, int day, char *buf, size_t size) {
    snprintf(buf, size, "%04d-%02d-%02d", year, month + 1, day);
}

GW_SHELL_INTERNAL void today_iso(char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    format_iso_date(t.tm_year + 1900, t.tm_mon, t.tm_mday, buf, size);
}

GW_SHELL_INTERNAL void iso_relative_days(int days, char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    t.tm_mday += days;
    t.tm_hour = 12;
    t.tm_min = 0;
    t.tm_sec = 0;
    mktime(&t);
    format_iso_date(t.tm_year + 1900, t.tm_mon, t.tm_mday, buf, size);
}

GW_SHELL_INTERNAL void format_due_label(const char *iso, char *buf, size_t size) {
    char today[11], tomorrow[11];
    int year, month, day;
    if (!iso || !iso[0]) {
        snprintf(buf, size, "%s", GW_STR_NO_DATE);
        return;
    }
    today_iso(today, sizeof(today));
    iso_relative_days(1, tomorrow, sizeof(tomorrow));
    if (!strcmp(iso, today)) {
        snprintf(buf, size, "%s", GW_STR_TODAY);
        return;
    }
    if (!strcmp(iso, tomorrow)) {
        snprintf(buf, size, "%s", GW_STR_TOMORROW);
        return;
    }
    if (sscanf(iso, "%d-%d-%d", &year, &month, &day) == 3 &&
        month >= 1 && month <= 12) {
        snprintf(buf, size, "%d %s", day, gw_str_month_short[month - 1]);
        return;
    }
    snprintf(buf, size, "%s", GW_STR_NO_DATE);
}

GW_SHELL_INTERNAL int days_in_month(int year, int month) {
    static const int days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month < 0 || month > 11) return 0;
    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month];
}
