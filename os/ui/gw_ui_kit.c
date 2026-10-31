/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared LVGL UI kit for Gleworks e-ink screens.
 */
#include <stdio.h>
#include "gw_ui_kit.h"
#include "gw_theme_tokens.h"
#include "gw_ui_style.h"
#include "gw_fonts.h"

lv_obj_t *gw_ui_kit_page_create(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    gw_ui_style_screen(screen);
    return screen;
}

lv_obj_t *gw_ui_kit_content_create(lv_obj_t *screen, int top) {
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_size(content, GW_UI_CONTENT_W,
                    GW_UI_PANEL_H - top - GW_UI_NAV_BAR_H - GW_UI_GAP);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, GW_UI_MARGIN, top);
    gw_ui_style_plain_panel(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, GW_UI_GAP, LV_PART_MAIN);
    return content;
}

lv_obj_t *gw_ui_kit_card_create(lv_obj_t *parent, int x, int y,
                                int width, int height) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, width, height);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    gw_ui_style_card(card);
    return card;
}

lv_obj_t *gw_ui_kit_hairline_create(lv_obj_t *parent, int width) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, width, GW_UI_HAIRLINE);
    gw_ui_style_hairline(line);
    return line;
}

lv_obj_t *gw_ui_kit_row_create(lv_obj_t *parent, int height) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, height);
    gw_ui_style_plain_panel(row);
    lv_obj_set_style_border_width(row, GW_UI_HAIRLINE, LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_pad_ver(row, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    return row;
}

lv_obj_t *gw_ui_kit_checkbox_create(lv_obj_t *parent, int checked,
                                     lv_event_cb_t cb, void *user_data) {
    lv_obj_t *box = lv_checkbox_create(parent);
    lv_checkbox_set_text(box, "");
    lv_obj_set_size(box, GW_UI_TOUCH_MIN, GW_UI_TOUCH_MIN);
    lv_obj_set_style_pad_all(box, 26, LV_PART_MAIN);
    lv_obj_set_style_text_color(box, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(box, gw_ui_c_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(box, GW_UI_BORDER_W, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(box, gw_ui_c_border(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(box, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(box, gw_ui_c_text(),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(box, lv_color_white(),
                                LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (checked) lv_obj_add_state(box, LV_STATE_CHECKED);
    if (cb) lv_obj_add_event_cb(box, cb, LV_EVENT_VALUE_CHANGED, user_data);
    return box;
}

lv_obj_t *gw_ui_kit_button_create(lv_obj_t *parent, const char *text,
                                  int width, int height,
                                  gw_ui_button_kind_t kind) {
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;
    lv_obj_set_size(button, width, height);
    if (kind == GW_UI_BUTTON_SECONDARY) {
        gw_ui_style_button_secondary(button);
        label = gw_ui_label(button, text, &gw_font_28, gw_ui_c_text());
    } else {
        gw_ui_style_button(button);
        label = gw_ui_label(button, text, &gw_font_28, lv_color_white());
    }
    lv_obj_center(label);
    return button;
}

static lv_obj_t *kit_nav_item(lv_obj_t *nav, const char *text, int index,
                              int active, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(nav);
    lv_obj_t *label;
    lv_obj_set_size(btn, GW_UI_PANEL_W / 3, GW_UI_NAV_BAR_H);
    lv_obj_set_style_bg_color(btn, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, index * (GW_UI_PANEL_W / 3), 0);
    label = gw_ui_label(btn, text, &gw_font_28, gw_ui_c_text());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -4);
    if (active) {
        lv_obj_t *underline = lv_obj_create(btn);
        lv_obj_set_size(underline, 112, 4);
        lv_obj_set_style_bg_color(underline, gw_ui_c_text(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(underline, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(underline, 0, LV_PART_MAIN);
        lv_obj_align(underline, LV_ALIGN_BOTTOM_MID, 0, -18);
    }
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

void gw_ui_kit_bottom_nav_create(lv_obj_t *screen, int active,
                                 const gw_ui_nav_cbs_t *cbs) {
    lv_obj_t *nav = lv_obj_create(screen);
    lv_obj_t *line;
    lv_obj_set_size(nav, GW_UI_PANEL_W, GW_UI_NAV_BAR_H);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    gw_ui_style_plain_panel(nav);
    line = gw_ui_kit_hairline_create(nav, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 0);
    kit_nav_item(nav, "Home", 0, active == 0, cbs ? cbs->home : NULL);
    kit_nav_item(nav, "Books", 1, active == 1, cbs ? cbs->books : NULL);
    kit_nav_item(nav, "Settings", 2, active == 2, cbs ? cbs->settings : NULL);
}

lv_obj_t *gw_ui_kit_app_bar_create(lv_obj_t *screen, const char *title,
                                   lv_event_cb_t back_cb) {
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_t *back, *heading, *line;
    lv_obj_set_size(bar, GW_UI_PANEL_W, GW_UI_APP_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    gw_ui_style_plain_panel(bar);
    lv_obj_set_style_pad_left(bar, GW_UI_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(bar, GW_UI_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    back = gw_ui_kit_button_create(bar, "< Back", 150, GW_UI_TOUCH_MIN,
                                   GW_UI_BUTTON_SECONDARY);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    heading = gw_ui_label(bar, title, &gw_font_44_bold, gw_ui_c_text());
    lv_obj_set_style_pad_left(heading, GW_UI_GAP, LV_PART_MAIN);

    line = gw_ui_kit_hairline_create(screen, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, GW_UI_APP_BAR_H - GW_UI_HAIRLINE);
    return bar;
}

lv_obj_t *gw_ui_kit_section_label_create(lv_obj_t *parent, const char *text) {
    return gw_ui_label(parent, text, &gw_font_24_bold, gw_ui_c_muted());
}

void gw_ui_kit_style_keyboard(lv_obj_t *keyboard) {
    lv_obj_set_style_bg_color(keyboard, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(keyboard, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(keyboard, &gw_font_28, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, gw_ui_c_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, gw_ui_c_text(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard, GW_UI_HAIRLINE, LV_PART_ITEMS);
    lv_obj_set_style_border_color(keyboard, gw_ui_c_border(), LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 4, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(keyboard, 0, LV_PART_ITEMS);
    lv_obj_set_style_color_filter_opa(keyboard, LV_OPA_TRANSP,
                                      LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(keyboard, gw_ui_c_text(),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(keyboard, lv_color_white(),
                                LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(keyboard, gw_ui_c_text(),
                                  LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(keyboard, LV_OPA_TRANSP,
                                      LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(keyboard, lv_color_make(220, 220, 220),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(keyboard, gw_ui_c_text(),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(keyboard, gw_ui_c_white(),
                              LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_text_color(keyboard, gw_ui_c_text(),
                                LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(keyboard, 0,
                                   LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_bg_color(keyboard, gw_ui_c_white(),
                              LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_text_color(keyboard, gw_ui_c_text(),
                                LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_outline_width(keyboard, 0,
                                   LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_color_filter_opa(keyboard, LV_OPA_TRANSP,
                                      LV_PART_ITEMS | LV_STATE_DISABLED);
    lv_obj_set_style_text_color(keyboard, gw_ui_c_muted(),
                                LV_PART_ITEMS | LV_STATE_DISABLED);
}

void gw_ui_kit_style_dropdown_list(lv_obj_t *list) {
    lv_obj_set_style_bg_color(list, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_radius(list, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_anim_time(list, 0, LV_PART_MAIN);
    lv_obj_set_style_max_height(list, 520, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, &gw_font_28, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, &gw_font_28, LV_PART_SELECTED);
    lv_obj_set_style_text_color(list, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, gw_ui_c_text(), LV_PART_SELECTED);
    lv_obj_set_style_text_line_space(list, 16, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, lv_color_make(220, 220, 220),
                              LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, lv_color_make(200, 200, 200),
                              LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER,
                            LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(list, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_color_filter_opa(list, LV_OPA_TRANSP,
                                      LV_PART_SELECTED | LV_STATE_PRESSED);
}
