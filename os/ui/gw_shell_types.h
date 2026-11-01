/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared shell types and bounds.  Kept in one header so the host test binary
 * can compile against the same definitions without including gw-shell.c. */
#ifndef GW_SHELL_TYPES_H
#define GW_SHELL_TYPES_H

#include <lvgl/lvgl.h>

#define GW_TODO_MAX 64
#define GW_TODO_TEXT_MAX 160
#define GW_APP_MAX 32
#define GW_NET_MAX 16
#define GW_CAL_ROWS 6
#define GW_CAL_COLS 7
#define GW_CAL_CELLS (GW_CAL_ROWS * GW_CAL_COLS)

enum { GW_TODO_ACTIVE = 0, GW_TODO_DONE = 1, GW_TODO_DELETED = 2 };

typedef struct {
    int id;
    int status;
    long created;
    long updated;
    char due[11];                  /* YYYY-MM-DD, empty = no due date */
    char text[GW_TODO_TEXT_MAX];
} gw_todo_t;

/* Retained row widgets: the To-do list is diffed by task id, never cleaned. */
typedef struct {
    int id;
    lv_obj_t *row;
    lv_obj_t *check;
    lv_obj_t *text;
    lv_obj_t *due;
    lv_obj_t *status;
} gw_todo_row_t;

typedef enum {
    GW_SCREEN_HOME = 0,
    GW_SCREEN_SETTINGS,
    GW_SCREEN_CALENDAR,
    GW_SCREEN_NOTES,
    GW_SCREEN_NETWORK,
    GW_SCREEN_APPS,
    GW_SCREEN_UPDATE,
    GW_SCREEN_LOCK,
    GW_SCREEN_COUNT
} gw_screen_t;

typedef struct {
    char book_title[160];
    char book_author[96];
    int  book_progress;             /* -1 = unavailable/invalid */
    char event_title[160];
    char event_when[64];
} gw_ui_model_t;

typedef struct {
    char id[64];
    char version[32];
    char description[128];
} gw_app_entry_t;

typedef struct {
    char ssid[65];
    int signal;
    int secure;
} gw_net_ap_t;

#endif /* GW_SHELL_TYPES_H */
