/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared layout constants and LVGL styles for the PW3 shell.
 *
 * Everything here is expressed in physical pixels for the 1072x1448 PW3
 * panel.  Styles are shared objects rather than per-widget local style
 * arrays: the 256 KiB LVGL heap must fit every screen at once.
 */
#ifndef GW_UI_STYLE_H
#define GW_UI_STYLE_H

#include <lvgl/lvgl.h>
#include "gw_theme_tokens.h"

/* E-ink borders need two physical pixels to survive the panel dither. */
#ifndef GW_UI_BORDER_W
#define GW_UI_BORDER_W 2
#endif

static lv_style_t gw_style_screen_s;
static lv_style_t gw_style_plain_s;
static lv_style_t gw_style_card_s;
static lv_style_t gw_style_hairline_s;
static lv_style_t gw_style_btn_primary_s;
static lv_style_t gw_style_btn_secondary_s;
static lv_style_t gw_style_btn_primary_pressed_s;
static lv_style_t gw_style_btn_secondary_pressed_s;
static lv_style_t gw_style_btn_disabled_s;
static lv_style_t gw_style_label_pressed_s;
static lv_style_t gw_style_label_primary_pressed_s;
static lv_style_t gw_style_label_disabled_s;
static lv_style_t gw_style_textarea_s;
static lv_style_t gw_style_flat_s;
static lv_style_t gw_style_flat_checked_s;
static lv_style_t gw_style_card_pressed_s;
static int gw_styles_ready;

static inline void gw_ui_styles_init(void) {
    if (gw_styles_ready) return;
    gw_styles_ready = 1;

    lv_style_init(&gw_style_screen_s);
    lv_style_set_bg_color(&gw_style_screen_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_screen_s, LV_OPA_COVER);
    lv_style_set_text_color(&gw_style_screen_s, gw_ui_c_text());
    lv_style_set_border_width(&gw_style_screen_s, 0);
    lv_style_set_radius(&gw_style_screen_s, 0);
    lv_style_set_shadow_width(&gw_style_screen_s, 0);
    lv_style_set_outline_width(&gw_style_screen_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_screen_s, LV_GRAD_DIR_NONE);

    lv_style_init(&gw_style_plain_s);
    lv_style_set_bg_color(&gw_style_plain_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_plain_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_plain_s, 0);
    lv_style_set_radius(&gw_style_plain_s, 0);
    lv_style_set_pad_all(&gw_style_plain_s, 0);
    lv_style_set_shadow_width(&gw_style_plain_s, 0);
    lv_style_set_outline_width(&gw_style_plain_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_plain_s, LV_GRAD_DIR_NONE);

    lv_style_init(&gw_style_card_s);
    lv_style_set_bg_color(&gw_style_card_s, gw_ui_c_card());
    lv_style_set_bg_opa(&gw_style_card_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_card_s, GW_UI_BORDER_W);
    lv_style_set_border_color(&gw_style_card_s, gw_ui_c_border());
    lv_style_set_radius(&gw_style_card_s, GW_UI_CARD_RADIUS);
    lv_style_set_pad_all(&gw_style_card_s, GW_UI_CARD_PADDING);
    lv_style_set_shadow_width(&gw_style_card_s, 0);
    lv_style_set_outline_width(&gw_style_card_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_card_s, LV_GRAD_DIR_NONE);

    lv_style_init(&gw_style_hairline_s);
    lv_style_set_bg_color(&gw_style_hairline_s, gw_ui_c_border());
    lv_style_set_bg_opa(&gw_style_hairline_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_hairline_s, 0);
    lv_style_set_radius(&gw_style_hairline_s, 0);
    lv_style_set_pad_all(&gw_style_hairline_s, 0);
    lv_style_set_shadow_width(&gw_style_hairline_s, 0);
    lv_style_set_outline_width(&gw_style_hairline_s, 0);

    lv_style_init(&gw_style_btn_primary_s);
    lv_style_set_bg_color(&gw_style_btn_primary_s, gw_ui_c_text());
    lv_style_set_bg_opa(&gw_style_btn_primary_s, LV_OPA_COVER);
    lv_style_set_text_color(&gw_style_btn_primary_s, gw_ui_c_white());
    lv_style_set_border_width(&gw_style_btn_primary_s, 0);
    lv_style_set_radius(&gw_style_btn_primary_s, GW_UI_CARD_RADIUS);
    lv_style_set_shadow_width(&gw_style_btn_primary_s, 0);
    lv_style_set_pad_all(&gw_style_btn_primary_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_btn_primary_s, LV_GRAD_DIR_NONE);

    lv_style_init(&gw_style_btn_secondary_s);
    lv_style_set_bg_color(&gw_style_btn_secondary_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_btn_secondary_s, LV_OPA_COVER);
    lv_style_set_text_color(&gw_style_btn_secondary_s, gw_ui_c_text());
    lv_style_set_border_width(&gw_style_btn_secondary_s, GW_UI_BORDER_W);
    lv_style_set_border_color(&gw_style_btn_secondary_s, gw_ui_c_border());
    lv_style_set_radius(&gw_style_btn_secondary_s, GW_UI_CARD_RADIUS);
    lv_style_set_shadow_width(&gw_style_btn_secondary_s, 0);
    lv_style_set_pad_all(&gw_style_btn_secondary_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_btn_secondary_s, LV_GRAD_DIR_NONE);

    lv_style_init(&gw_style_btn_primary_pressed_s);
    /* A5: primary stays black; only the border/label signal the press.  A
     * white fill on a large Add/Connect/Unlock button reads as a flash. */
    lv_style_set_bg_color(&gw_style_btn_primary_pressed_s, gw_ui_c_text());
    lv_style_set_bg_opa(&gw_style_btn_primary_pressed_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_btn_primary_pressed_s, 4);
    lv_style_set_border_color(&gw_style_btn_primary_pressed_s, gw_ui_c_white());
    lv_style_set_text_color(&gw_style_btn_primary_pressed_s, gw_ui_c_white());
    lv_style_set_anim_time(&gw_style_btn_primary_pressed_s, 0);
    lv_style_set_color_filter_opa(&gw_style_btn_primary_pressed_s,
                                  LV_OPA_TRANSP);

    lv_style_init(&gw_style_btn_secondary_pressed_s);
    lv_style_set_bg_color(&gw_style_btn_secondary_pressed_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_btn_secondary_pressed_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_btn_secondary_pressed_s, 4);
    lv_style_set_border_color(&gw_style_btn_secondary_pressed_s, gw_ui_c_text());
    lv_style_set_text_color(&gw_style_btn_secondary_pressed_s, gw_ui_c_text());
    lv_style_set_anim_time(&gw_style_btn_secondary_pressed_s, 0);
    lv_style_set_color_filter_opa(&gw_style_btn_secondary_pressed_s,
                                  LV_OPA_TRANSP);

    lv_style_init(&gw_style_btn_disabled_s);
    lv_style_set_bg_color(&gw_style_btn_disabled_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_btn_disabled_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_btn_disabled_s, GW_UI_HAIRLINE);
    lv_style_set_border_color(&gw_style_btn_disabled_s, gw_ui_c_border());
    lv_style_set_text_color(&gw_style_btn_disabled_s, gw_ui_c_muted());

    lv_style_init(&gw_style_label_pressed_s);
    lv_style_set_text_color(&gw_style_label_pressed_s, gw_ui_c_text());

    lv_style_init(&gw_style_label_primary_pressed_s);
    lv_style_set_text_color(&gw_style_label_primary_pressed_s, gw_ui_c_white());

    lv_style_init(&gw_style_label_disabled_s);
    lv_style_set_text_color(&gw_style_label_disabled_s, gw_ui_c_muted());

    lv_style_init(&gw_style_flat_s);
    lv_style_set_bg_color(&gw_style_flat_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_flat_s, LV_OPA_COVER);
    lv_style_set_border_width(&gw_style_flat_s, 0);
    lv_style_set_radius(&gw_style_flat_s, 0);
    lv_style_set_shadow_width(&gw_style_flat_s, 0);
    lv_style_set_pad_all(&gw_style_flat_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_flat_s, LV_GRAD_DIR_NONE);

    lv_style_init(&gw_style_card_pressed_s);
    lv_style_set_border_width(&gw_style_card_pressed_s, 4);
    lv_style_set_border_color(&gw_style_card_pressed_s, gw_ui_c_text());
    lv_style_set_radius(&gw_style_card_pressed_s, GW_UI_CARD_RADIUS);

    lv_style_init(&gw_style_flat_checked_s);
    lv_style_set_bg_color(&gw_style_flat_checked_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_flat_checked_s, LV_OPA_COVER);
    lv_style_set_color_filter_opa(&gw_style_flat_checked_s, LV_OPA_TRANSP);

    lv_style_init(&gw_style_textarea_s);
    lv_style_set_bg_color(&gw_style_textarea_s, gw_ui_c_white());
    lv_style_set_bg_opa(&gw_style_textarea_s, LV_OPA_COVER);
    lv_style_set_text_color(&gw_style_textarea_s, gw_ui_c_text());
    lv_style_set_border_width(&gw_style_textarea_s, GW_UI_BORDER_W);
    lv_style_set_border_color(&gw_style_textarea_s, gw_ui_c_border());
    lv_style_set_radius(&gw_style_textarea_s, GW_UI_TEXTAREA_RADIUS);
    lv_style_set_pad_all(&gw_style_textarea_s, 16);
    lv_style_set_shadow_width(&gw_style_textarea_s, 0);
    lv_style_set_bg_grad_dir(&gw_style_textarea_s, LV_GRAD_DIR_NONE);
}

/* ── Object styles ──────────────────────────────────────────────────────── */

static inline void gw_ui_style_screen(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_screen_s, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static inline void gw_ui_style_plain_panel(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_plain_s, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static inline void gw_ui_style_card(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_card_s, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static inline void gw_ui_style_hairline(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_hairline_s, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

/* Clickable card feedback: the 2 px border becomes 4 px while pressed. */
static inline void gw_ui_style_card_feedback(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_card_pressed_s,
                     LV_PART_MAIN | LV_STATE_PRESSED);
}

/* Flat white surface used by nav items, calendar cells and list rows. */
static inline void gw_ui_style_flat(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_flat_s, LV_PART_MAIN);
}

/* Primary action: solid near-black, white label, 96 px minimum height. */
static inline void gw_ui_style_button(lv_obj_t *button) {
    gw_ui_styles_init();
    lv_obj_add_style(button, &gw_style_btn_primary_s, LV_PART_MAIN);
}

/* Secondary action: white with a single readable hairline. */
static inline void gw_ui_style_button_secondary(lv_obj_t *button) {
    gw_ui_styles_init();
    lv_obj_add_style(button, &gw_style_btn_secondary_s, LV_PART_MAIN);
}

/* Pressed and disabled feedback shared by primary and secondary buttons.
 * Gray pressed fills are avoided because they leave residue on partial
 * updates; the border carries the feedback instead.  A primary button keeps
 * its black surface so a tap never flashes a large white rectangle. */
static inline void gw_ui_style_button_feedback(lv_obj_t *button, int primary) {
    gw_ui_styles_init();
    lv_obj_add_style(button,
                     primary ? &gw_style_btn_primary_pressed_s
                             : &gw_style_btn_secondary_pressed_s,
                     LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(button, &gw_style_btn_disabled_s,
                     LV_PART_MAIN | LV_STATE_DISABLED);
}

/* Apply the pressed/disabled text colour to a button label. */
static inline void gw_ui_style_button_label_feedback(lv_obj_t *label, int primary) {
    gw_ui_styles_init();
    lv_obj_add_style(label,
                     primary ? &gw_style_label_primary_pressed_s
                             : &gw_style_label_pressed_s,
                     LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(label, &gw_style_label_disabled_s,
                     LV_PART_MAIN | LV_STATE_DISABLED);
}

static inline void gw_ui_style_textarea(lv_obj_t *obj) {
    gw_ui_styles_init();
    lv_obj_add_style(obj, &gw_style_textarea_s, LV_PART_MAIN);
}

typedef struct {
    const lv_font_t *font;
    lv_color_t colour;
    lv_style_t style;
} gw_label_style_t;

#define GW_LABEL_STYLE_MAX 20
static gw_label_style_t gw_label_styles[GW_LABEL_STYLE_MAX];
static int gw_label_style_count;

static inline lv_style_t *gw_ui_label_style(const lv_font_t *font,
                                            lv_color_t colour) {
    int i;
    unsigned value = lv_color_to8(colour);
    for (i = 0; i < gw_label_style_count; i++)
        if (gw_label_styles[i].font == font &&
            lv_color_to8(gw_label_styles[i].colour) == value)
            return &gw_label_styles[i].style;
    if (gw_label_style_count >= GW_LABEL_STYLE_MAX) return NULL;
    i = gw_label_style_count++;
    gw_label_styles[i].font = font;
    gw_label_styles[i].colour = colour;
    lv_style_init(&gw_label_styles[i].style);
    lv_style_set_text_font(&gw_label_styles[i].style, font);
    lv_style_set_text_color(&gw_label_styles[i].style, colour);
    return &gw_label_styles[i].style;
}

static inline lv_obj_t *gw_ui_label(lv_obj_t *parent, const char *text,
                                     const lv_font_t *font, lv_color_t colour) {
    lv_obj_t *label = lv_label_create(parent);
    lv_style_t *style = gw_ui_label_style(font, colour);
    if (style) lv_obj_add_style(label, style, LV_PART_MAIN);
    lv_label_set_text(label, text);
    return label;
}

#endif /* GW_UI_STYLE_H */
