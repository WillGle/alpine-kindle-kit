/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared layout constants and LVGL styles for the PW3 Home UI.
 *
 * Everything here is expressed in physical pixels for the 1072x1448 PW3
 * panel.  Keep typography, spacing and colour decisions in one place so the
 * Home, Calendar, Notes, Network and Settings screens stay visually
 * consistent.
 */
#ifndef GW_UI_STYLE_H
#define GW_UI_STYLE_H

#include <lvgl/lvgl.h>
#include "gw_theme_tokens.h"

/* E-ink borders need two physical pixels to survive the panel dither. */
#ifndef GW_UI_BORDER_W
#define GW_UI_BORDER_W 2
#endif

/* Layout and grayscale colours are generated from theme/tokens.json.
 * Keep only object styles in this header. */

/* ── Object styles ──────────────────────────────────────────────────────── */

static inline void gw_ui_style_reset_box(lv_obj_t *obj) {
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_zoom(obj, LV_IMG_ZOOM_NONE, LV_PART_MAIN);
}

static inline void gw_ui_style_screen(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    gw_ui_style_reset_box(obj);
}

static inline void gw_ui_style_plain_panel(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    gw_ui_style_reset_box(obj);
}

static inline void gw_ui_style_card(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, gw_ui_c_card(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_radius(obj, GW_UI_CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, GW_UI_CARD_PADDING, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* Hairline separator used instead of boxed rows. */
static inline void gw_ui_style_hairline(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    gw_ui_style_reset_box(obj);
}

/* Primary action: solid near-black, white label, 96 px minimum height. */
static inline void gw_ui_style_button(lv_obj_t *button) {
    lv_obj_set_style_bg_color(button, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, GW_UI_CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(button, LV_GRAD_DIR_NONE, LV_PART_MAIN);
}

/* Secondary action: white with a single readable hairline. */
static inline void gw_ui_style_button_secondary(lv_obj_t *button) {
    lv_obj_set_style_bg_color(button, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_radius(button, GW_UI_CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(button, LV_GRAD_DIR_NONE, LV_PART_MAIN);
}

static inline void gw_ui_style_textarea(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_radius(obj, GW_UI_TEXTAREA_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, LV_PART_MAIN);
}

static inline lv_obj_t *gw_ui_label(lv_obj_t *parent, const char *text,
                                     const lv_font_t *font, lv_color_t colour) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, colour, LV_PART_MAIN);
    lv_label_set_text(label, text);
    return label;
}

#endif /* GW_UI_STYLE_H */
