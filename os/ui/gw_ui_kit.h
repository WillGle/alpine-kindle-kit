/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared LVGL UI kit for Gleworks e-ink screens.
 *
 * This is the shell-side adapter over theme/tokens.json.  Page code composes
 * these primitives instead of re-implementing margins, fonts, borders and
 * button styles.  Geometry values are the existing PW3 layout values, so the
 * kit can be introduced without moving anything.
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
    lv_event_cb_t books;
    lv_event_cb_t settings;
} gw_ui_nav_cbs_t;

/* Page / structure */
lv_obj_t *gw_ui_kit_page_create(void);
lv_obj_t *gw_ui_kit_content_create(lv_obj_t *screen, int top);
lv_obj_t *gw_ui_kit_card_create(lv_obj_t *parent, int x, int y,
                                int width, int height);
lv_obj_t *gw_ui_kit_hairline_create(lv_obj_t *parent, int width);
lv_obj_t *gw_ui_kit_row_create(lv_obj_t *parent, int height);
lv_obj_t *gw_ui_kit_checkbox_create(lv_obj_t *parent, int checked,
                                     lv_event_cb_t cb, void *user_data);
lv_obj_t *gw_ui_kit_app_bar_create(lv_obj_t *screen, const char *title,
                                   lv_event_cb_t back_cb);
void gw_ui_kit_bottom_nav_create(lv_obj_t *screen, int active,
                                 const gw_ui_nav_cbs_t *cbs);

/* Typography and controls */
lv_obj_t *gw_ui_kit_section_label_create(lv_obj_t *parent, const char *text);
lv_obj_t *gw_ui_kit_button_create(lv_obj_t *parent, const char *text,
                                  int width, int height,
                                  gw_ui_button_kind_t kind);

/* E-ink control surfaces shared by Notes and Network. */
void gw_ui_kit_style_keyboard(lv_obj_t *keyboard);
void gw_ui_kit_style_dropdown_list(lv_obj_t *list);

#endif /* GW_UI_KIT_H */
