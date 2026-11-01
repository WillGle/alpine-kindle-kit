/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared LVGL UI kit for Gleworks e-ink screens.
 *
 * This is the shell-side adapter over theme/tokens.json.  Page code composes
 * these primitives instead of re-implementing margins, fonts, borders and
 * button styles.  All touch targets are at least 96 x 96 px and transitions
 * are disabled: e-ink feedback is a border change within one refresh.
 */
#ifndef GW_UI_KIT_H
#define GW_UI_KIT_H

#include <lvgl/lvgl.h>

typedef enum {
    GW_UI_BUTTON_PRIMARY = 0,
    GW_UI_BUTTON_SECONDARY = 1
} gw_ui_button_kind_t;

typedef struct {
    lv_event_cb_t home;
    lv_event_cb_t library;
    lv_event_cb_t calendar;
    lv_event_cb_t todos;
    lv_event_cb_t settings;
} gw_ui_nav_cbs_t;

typedef void (*gw_ui_action_cb_t)(void *user_data);

/* Page / structure */
lv_obj_t *gw_ui_kit_page_create(void);
lv_obj_t *gw_ui_kit_content_create(lv_obj_t *screen, int top);
lv_obj_t *gw_ui_kit_scroll_content_create(lv_obj_t *screen, int top, int bottom);
lv_obj_t *gw_ui_kit_card_create(lv_obj_t *parent, int x, int y,
                                int width, int height);
lv_obj_t *gw_ui_kit_hairline_create(lv_obj_t *parent, int width);
lv_obj_t *gw_ui_kit_row_create(lv_obj_t *parent, int height);
lv_obj_t *gw_ui_kit_row_trailing_create(lv_obj_t *row);
lv_obj_t *gw_ui_kit_checkbox_create(lv_obj_t *parent, int checked,
                                     lv_event_cb_t cb, void *user_data);
void gw_ui_kit_checkbox_set_checked(lv_obj_t *box, int checked);
lv_obj_t *gw_ui_kit_title_bar_create(lv_obj_t *screen, const char *title);
lv_obj_t *gw_ui_kit_app_bar_create(lv_obj_t *screen, const char *title,
                                   lv_event_cb_t back_cb);
void gw_ui_kit_bottom_nav_create(lv_obj_t *screen, int active,
                                 const gw_ui_nav_cbs_t *cbs);

/* Typography and controls */
lv_obj_t *gw_ui_kit_section_label_create(lv_obj_t *parent, const char *text);
lv_obj_t *gw_ui_kit_button_create(lv_obj_t *parent, const char *text,
                                  int width, int height,
                                  gw_ui_button_kind_t kind);
lv_obj_t *gw_ui_kit_button_icon_create(lv_obj_t *parent, const char *icon,
                                       const char *text, int width, int height,
                                       gw_ui_button_kind_t kind);
void gw_ui_kit_busy_set(lv_obj_t *button, int busy, const char *busy_text);
int gw_ui_kit_button_is_busy(lv_obj_t *button);
void gw_ui_kit_primary_set_enabled(lv_obj_t *button, int enabled);

/* E-ink control surfaces shared by To-dos and Network. */
void gw_ui_kit_style_keyboard(lv_obj_t *keyboard);
void gw_ui_kit_style_dropdown_list(lv_obj_t *list);

/* Confirm modal.  One full paint per lifecycle: open only. */
void gw_ui_kit_set_full_paint_cb(gw_ui_action_cb_t cb);
void gw_ui_kit_set_modal_close_cb(gw_ui_action_cb_t cb);
void gw_ui_kit_confirm_create(const char *title, const char *message,
                              const char *confirm_text,
                              gw_ui_action_cb_t on_confirm, void *user_data);
void gw_ui_kit_confirm_close(void);
int gw_ui_kit_confirm_is_open(void);

/* Undo strip, one instance at a time, 5 s timer. */
void gw_ui_kit_undo_create(const char *text, const char *action_text,
                           gw_ui_action_cb_t on_action, void *user_data);
void gw_ui_kit_undo_close(void);
int gw_ui_kit_undo_is_open(void);
lv_obj_t *gw_ui_kit_undo_action_button(void);

/* Non-blocking toast on the top layer; auto-dismisses after 3 s. */
void gw_ui_kit_toast_create(const char *text);
void gw_ui_kit_toast_close(void);
int gw_ui_kit_toast_is_open(void);

/* Four deterministic signal rectangles, level 0-4. */
lv_obj_t *gw_ui_kit_signal_bars_create(lv_obj_t *parent, int level);

#endif /* GW_UI_KIT_H */
