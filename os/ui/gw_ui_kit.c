/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared LVGL UI kit for Gleworks e-ink screens.
 */
#include <stdio.h>
#include <string.h>
#include "gw_ui_kit.h"
#include "gw_theme_tokens.h"
#include "gw_ui_style.h"
#include "gw_ui_strings.h"
#include "gw_icons.h"
#include "gw_fonts.h"

typedef struct {
    lv_obj_t *button;
    char original[96];
    lv_color_t original_colour;
} busy_slot_t;

static busy_slot_t g_busy[8];

static gw_ui_action_cb_t g_full_paint_cb;
static gw_ui_action_cb_t g_modal_close_cb;
static lv_obj_t *g_confirm;
static gw_ui_action_cb_t g_confirm_cb;
static void *g_confirm_user_data;
static lv_obj_t *g_undo;
static lv_timer_t *g_undo_timer;
static gw_ui_action_cb_t g_undo_cb;
static void *g_undo_user_data;

lv_obj_t *gw_ui_kit_page_create(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    gw_ui_style_screen(screen);
    return screen;
}

static lv_obj_t *kit_panel(lv_obj_t *parent, int x, int y, int width,
                           int height) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, y);
    gw_ui_style_plain_panel(panel);
    return panel;
}

lv_obj_t *gw_ui_kit_content_create(lv_obj_t *screen, int top) {
    lv_obj_t *content = kit_panel(screen, GW_UI_MARGIN, top, GW_UI_CONTENT_W,
                                  GW_UI_PANEL_H - top - GW_UI_NAV_BAR_H -
                                      GW_UI_GAP);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, GW_UI_GAP, LV_PART_MAIN);
    return content;
}

static void scroll_visibility_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);
    if (code == LV_EVENT_SCROLL_BEGIN)
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_ACTIVE);
    else if (code == LV_EVENT_SCROLL_END)
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t *gw_ui_kit_scroll_content_create(lv_obj_t *screen, int top, int bottom) {
    lv_obj_t *content = kit_panel(screen, GW_UI_MARGIN, top, GW_UI_CONTENT_W,
                                  GW_UI_PANEL_H - top - bottom);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, GW_UI_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(content, GW_UI_SPACE_3, LV_PART_MAIN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(content, scroll_visibility_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(content, scroll_visibility_cb, LV_EVENT_SCROLL_END, NULL);
    lv_obj_set_style_width(content, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(content, gw_ui_c_text(), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, LV_PART_SCROLLBAR);
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
    lv_obj_set_style_pad_ver(row, GW_UI_SPACE_1, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    return row;
}

lv_obj_t *gw_ui_kit_row_trailing_create(lv_obj_t *row) {
    lv_obj_t *trailing = lv_obj_create(row);
    lv_obj_set_size(trailing, LV_SIZE_CONTENT, LV_PCT(100));
    gw_ui_style_plain_panel(trailing);
    lv_obj_set_flex_flow(trailing, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(trailing, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(trailing, GW_UI_SPACE_2, LV_PART_MAIN);
    return trailing;
}

/* Checkbox: 56 x 56 visible indicator inside a 96 x 96 target. */
static void checkbox_refresh(lv_obj_t *box) {
    lv_obj_t *glyph = (lv_obj_t *)lv_obj_get_user_data(box);
    if (lv_obj_has_state(box, LV_STATE_CHECKED)) {
        if (!glyph) {
            lv_obj_t *field = lv_obj_get_child(box, 0);
            glyph = gw_ui_label(field, LV_SYMBOL_OK, &gw_font_32,
                                gw_ui_c_text());
            lv_obj_center(glyph);
            lv_obj_set_user_data(box, glyph);
        }
    } else if (glyph) {
        lv_obj_del(glyph);
        lv_obj_set_user_data(box, NULL);
    }
}

static void checkbox_clicked(lv_event_t *event) {
    lv_obj_t *box = lv_event_get_target(event);
    if (lv_obj_has_state(box, LV_STATE_CHECKED))
        lv_obj_clear_state(box, LV_STATE_CHECKED);
    else
        lv_obj_add_state(box, LV_STATE_CHECKED);
    checkbox_refresh(box);
    lv_event_send(box, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *gw_ui_kit_checkbox_create(lv_obj_t *parent, int checked,
                                     lv_event_cb_t cb, void *user_data) {
    lv_obj_t *box = lv_btn_create(parent);
    lv_obj_t *field, *glyph;
    lv_obj_set_size(box, GW_UI_TOUCH_MIN, GW_UI_TOUCH_MIN);
    lv_obj_set_style_bg_color(box, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(box, 0, LV_PART_MAIN);
    gw_ui_style_flat(box);
    lv_obj_add_style(box, &gw_style_flat_checked_s,
                     LV_PART_MAIN | LV_STATE_CHECKED);

    field = lv_obj_create(box);
    lv_obj_set_size(field, 56, 56);
    lv_obj_center(field);
    lv_obj_set_style_bg_color(field, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(field, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(field, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_radius(field, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(field, 0, LV_PART_MAIN);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_CLICKABLE);

    glyph = NULL;
    if (checked) {
        glyph = gw_ui_label(field, LV_SYMBOL_OK, &gw_font_32, gw_ui_c_text());
        lv_obj_center(glyph);
    }
    lv_obj_set_user_data(box, glyph);
    if (checked) lv_obj_add_state(box, LV_STATE_CHECKED);
    lv_obj_add_event_cb(box, checkbox_clicked, LV_EVENT_CLICKED, NULL);
    if (cb) lv_obj_add_event_cb(box, cb, LV_EVENT_VALUE_CHANGED, user_data);
    return box;
}

static lv_obj_t *kit_button_common(lv_obj_t *parent, const char *icon,
                                   const char *text, int width, int height,
                                   gw_ui_button_kind_t kind) {
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;
    char copy[128];
    lv_obj_set_size(button, width, height);
    if (kind == GW_UI_BUTTON_SECONDARY) {
        gw_ui_style_button_secondary(button);
        label = gw_ui_label(button, "", &gw_font_32, gw_ui_c_text());
    } else {
        gw_ui_style_button(button);
        label = gw_ui_label(button, "", &gw_font_32, lv_color_white());
    }
    if (icon && icon[0] && text && text[0])
        snprintf(copy, sizeof(copy), "%s  %s", icon, text);
    else if (icon && icon[0])
        snprintf(copy, sizeof(copy), "%s", icon);
    else
        snprintf(copy, sizeof(copy), "%s", text ? text : "");
    lv_label_set_text(label, copy);
    lv_obj_center(label);
    gw_ui_style_button_feedback(button, kind == GW_UI_BUTTON_PRIMARY);
    gw_ui_style_button_label_feedback(label, kind == GW_UI_BUTTON_PRIMARY);
    return button;
}

lv_obj_t *gw_ui_kit_button_create(lv_obj_t *parent, const char *text,
                                  int width, int height,
                                  gw_ui_button_kind_t kind) {
    return kit_button_common(parent, NULL, text, width, height, kind);
}

lv_obj_t *gw_ui_kit_button_icon_create(lv_obj_t *parent, const char *icon,
                                       const char *text, int width, int height,
                                       gw_ui_button_kind_t kind) {
    return kit_button_common(parent, icon, text, width, height, kind);
}

static busy_slot_t *busy_find(lv_obj_t *button) {
    int i;
    for (i = 0; i < 8; i++)
        if (g_busy[i].button == button) return &g_busy[i];
    return NULL;
}

void gw_ui_kit_busy_set(lv_obj_t *button, int busy, const char *busy_text) {
    busy_slot_t *slot;
    lv_obj_t *label;
    if (!button) return;
    slot = busy_find(button);
    if (!slot) {
        int i;
        for (i = 0; i < 8; i++) {
            if (!g_busy[i].button) { slot = &g_busy[i]; break; }
        }
        if (!slot) return;
        slot->button = button;
        label = lv_obj_get_child(button, 0);
        snprintf(slot->original, sizeof(slot->original), "%s",
                 label ? lv_label_get_text(label) : "");
        slot->original_colour = label
            ? lv_obj_get_style_text_color(label, LV_PART_MAIN)
            : gw_ui_c_text();
    }
    label = lv_obj_get_child(button, 0);
    if (busy) {
        if (label) {
            lv_label_set_text(label, busy_text && busy_text[0] ? busy_text
                                                               : "...");
            lv_obj_set_style_text_color(label, gw_ui_c_muted(), LV_PART_MAIN);
        }
        lv_obj_add_state(button, LV_STATE_DISABLED);
    } else {
        if (label) {
            lv_label_set_text(label, slot->original);
            lv_obj_remove_local_style_prop(label, LV_STYLE_TEXT_COLOR,
                                           LV_PART_MAIN);
        }
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        slot->button = NULL;
        slot->original[0] = '\0';
    }
}

int gw_ui_kit_button_is_busy(lv_obj_t *button) {
    return button && lv_obj_has_state(button, LV_STATE_DISABLED);
}

static lv_obj_t *kit_nav_item(lv_obj_t *nav, const char *icon,
                              const char *text, int index, int active,
                              lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(nav);
    lv_obj_t *label;
    char copy[64];
    snprintf(copy, sizeof(copy), "%s\n%s", icon, text);
    lv_obj_set_size(btn, GW_UI_PANEL_W / 5, GW_UI_NAV_BAR_H);
    gw_ui_style_flat(btn);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, index * (GW_UI_PANEL_W / 5), 0);
    label = gw_ui_label(btn, copy,
                        active ? &gw_font_24_bold : &gw_font_24,
                        gw_ui_c_text());
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -6);
    if (active) {
        lv_obj_t *underline = lv_obj_create(btn);
        lv_obj_set_size(underline, 112, 4);
        lv_obj_set_style_bg_color(underline, gw_ui_c_text(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(underline, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(underline, 0, LV_PART_MAIN);
        lv_obj_clear_flag(underline, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(underline, LV_ALIGN_BOTTOM_MID, 0, -2);
    }
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

void gw_ui_kit_bottom_nav_create(lv_obj_t *screen, int active,
                                 const gw_ui_nav_cbs_t *cbs) {
    lv_obj_t *nav = lv_obj_create(screen);
    lv_obj_set_size(nav, GW_UI_PANEL_W, GW_UI_NAV_BAR_H);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    gw_ui_style_plain_panel(nav);
    {
        lv_obj_t *line = gw_ui_kit_hairline_create(nav, GW_UI_PANEL_W);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 0);
    }
    kit_nav_item(nav, LV_SYMBOL_HOME, GW_STR_HOME, 0, active == 0,
                 cbs ? cbs->home : NULL);
    kit_nav_item(nav, GW_ICON_BOOK, GW_STR_LIBRARY, 1, active == 1,
                 cbs ? cbs->library : NULL);
    kit_nav_item(nav, GW_ICON_CALENDAR, GW_STR_CALENDAR, 2, active == 2,
                 cbs ? cbs->calendar : NULL);
    kit_nav_item(nav, GW_ICON_LIST, GW_STR_TODOS, 3, active == 3,
                 cbs ? cbs->todos : NULL);
    kit_nav_item(nav, LV_SYMBOL_SETTINGS, GW_STR_SETTINGS, 4, active == 4,
                 cbs ? cbs->settings : NULL);
}

lv_obj_t *gw_ui_kit_title_bar_create(lv_obj_t *screen, const char *title) {
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_t *heading, *line;
    lv_obj_set_size(bar, GW_UI_PANEL_W, GW_UI_APP_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    gw_ui_style_plain_panel(bar);
    lv_obj_set_style_pad_left(bar, GW_UI_MARGIN, LV_PART_MAIN);
    heading = gw_ui_label(bar, title, &gw_font_44_bold, gw_ui_c_text());
    lv_obj_align(heading, LV_ALIGN_LEFT_MID, 0, 0);
    line = gw_ui_kit_hairline_create(screen, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, GW_UI_APP_BAR_H - GW_UI_HAIRLINE);
    return bar;
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
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    back = gw_ui_kit_button_icon_create(bar, LV_SYMBOL_LEFT, GW_STR_BACK, 170,
                                        GW_UI_TOUCH_MIN,
                                        GW_UI_BUTTON_SECONDARY);
    if (back_cb) lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    heading = gw_ui_label(bar, title, &gw_font_44_bold, gw_ui_c_text());
    lv_obj_set_style_pad_left(heading, GW_UI_GAP, LV_PART_MAIN);

    line = gw_ui_kit_hairline_create(screen, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, GW_UI_APP_BAR_H - GW_UI_HAIRLINE);
    return bar;
}

lv_obj_t *gw_ui_kit_section_label_create(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = gw_ui_label(parent, text, &gw_font_28, gw_ui_c_muted());
    lv_obj_set_style_text_letter_space(label, 1, LV_PART_MAIN);
    return label;
}

/* ── Confirm modal ─────────────────────────────────────────────────────── */

static void confirm_cancel_cb(lv_event_t *event) {
    (void)event;
    gw_ui_kit_confirm_close();
}

static void confirm_accept_cb(lv_event_t *event) {
    gw_ui_action_cb_t cb = g_confirm_cb;
    void *user = g_confirm_user_data;
    (void)event;
    gw_ui_kit_confirm_close();
    if (cb) cb(user);
}

void gw_ui_kit_set_full_paint_cb(gw_ui_action_cb_t cb) {
    g_full_paint_cb = cb;
}

void gw_ui_kit_set_modal_close_cb(gw_ui_action_cb_t cb) {
    g_modal_close_cb = cb;
}

void gw_ui_kit_confirm_create(const char *title, const char *message,
                              const char *confirm_text,
                              gw_ui_action_cb_t on_confirm, void *user_data) {
    lv_obj_t *modal, *column, *heading, *body, *row, *cancel, *confirm;
    if (g_confirm) gw_ui_kit_confirm_close();
    g_confirm_cb = on_confirm;
    g_confirm_user_data = user_data;

    modal = lv_obj_create(lv_layer_top());
    g_confirm = modal;
    lv_obj_set_size(modal, GW_UI_PANEL_W, GW_UI_PANEL_H);
    lv_obj_align(modal, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(modal, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(modal, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_radius(modal, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(modal, GW_UI_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(modal, 0, LV_PART_MAIN);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    column = lv_obj_create(modal);
    lv_obj_set_size(column, GW_UI_CONTENT_W, 600);
    lv_obj_center(column);
    gw_ui_style_plain_panel(column);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column, GW_UI_GAP, LV_PART_MAIN);

    heading = gw_ui_label(column, title, &gw_font_44_bold, gw_ui_c_text());
    lv_obj_set_width(heading, LV_PCT(100));
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    body = gw_ui_label(column, message, &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(body, 800);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    row = lv_obj_create(column);
    lv_obj_set_size(row, LV_SIZE_CONTENT, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    cancel = gw_ui_kit_button_create(row, GW_STR_CANCEL, 240,
                                     GW_UI_TOUCH_MIN, GW_UI_BUTTON_SECONDARY);
    lv_obj_add_event_cb(cancel, confirm_cancel_cb, LV_EVENT_CLICKED, NULL);
    confirm = gw_ui_kit_button_create(row,
                                      confirm_text && confirm_text[0]
                                          ? confirm_text : GW_STR_CONFIRM,
                                      240, GW_UI_TOUCH_MIN,
                                      GW_UI_BUTTON_PRIMARY);
    lv_obj_add_event_cb(confirm, confirm_accept_cb, LV_EVENT_CLICKED, NULL);

    /* The modal is one full paint for the whole lifecycle.  Closing uses a
     * partial/action update; it must never force a second GC16. */
    if (g_full_paint_cb) g_full_paint_cb(NULL);
}

void gw_ui_kit_confirm_close(void) {
    if (!g_confirm) return;
    lv_obj_del(g_confirm);
    g_confirm = NULL;
    g_confirm_cb = NULL;
    g_confirm_user_data = NULL;
    /* The modal covered the whole panel; repaint it with the same DU4
     * partial path instead of letting the next frame be promoted to GC16. */
    if (g_modal_close_cb) g_modal_close_cb(NULL);
}

int gw_ui_kit_confirm_is_open(void) {
    return g_confirm != NULL;
}

/* ── Undo strip ────────────────────────────────────────────────────────── */

static void undo_timer_cb(lv_timer_t *timer) {
    (void)timer;
    gw_ui_kit_undo_close();
}

static void undo_action_cb(lv_event_t *event) {
    gw_ui_action_cb_t cb = g_undo_cb;
    void *user = g_undo_user_data;
    (void)event;
    gw_ui_kit_undo_close();
    if (cb) cb(user);
}

void gw_ui_kit_undo_create(const char *text, const char *action_text,
                           gw_ui_action_cb_t on_action, void *user_data) {
    lv_obj_t *strip, *label, *action;
    gw_ui_kit_undo_close();
    g_undo_cb = on_action;
    g_undo_user_data = user_data;
    strip = lv_obj_create(lv_layer_top());
    g_undo = strip;
    lv_obj_set_size(strip, 976, GW_UI_TOUCH_MIN);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -GW_UI_NAV_BAR_H - 8);
    lv_obj_set_style_bg_color(strip, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(strip, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(strip, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_radius(strip, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(strip, GW_UI_SPACE_1, LV_PART_MAIN);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label = gw_ui_label(strip, text, &gw_font_32, gw_ui_c_text());
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    action = gw_ui_kit_button_create(strip,
                                     action_text && action_text[0]
                                         ? action_text : GW_STR_UNDO,
                                     160, GW_UI_TOUCH_MIN,
                                     GW_UI_BUTTON_SECONDARY);
    lv_obj_add_event_cb(action, undo_action_cb, LV_EVENT_CLICKED, NULL);
    g_undo_timer = lv_timer_create(undo_timer_cb, 5000, NULL);
}

void gw_ui_kit_undo_close(void) {
    g_undo_cb = NULL;
    g_undo_user_data = NULL;
    if (g_undo_timer) {
        lv_timer_del(g_undo_timer);
        g_undo_timer = NULL;
    }
    if (g_undo) {
        lv_obj_del(g_undo);
        g_undo = NULL;
    }
}

int gw_ui_kit_undo_is_open(void) {
    return g_undo != NULL;
}

lv_obj_t *gw_ui_kit_undo_action_button(void) {
    return g_undo ? lv_obj_get_child(g_undo, 1) : NULL;
}

/* ── Signal bars ───────────────────────────────────────────────────────── */

static void signal_bars_draw_cb(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_target(event);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(event);
    lv_area_t coords;
    lv_area_t bar;
    lv_draw_rect_dsc_t dsc;
    int level = (int)(intptr_t)lv_obj_get_user_data(obj);
    static const int heights[4] = { 10, 18, 26, 34 };
    int i;
    lv_obj_get_coords(obj, &coords);
    for (i = 0; i < 4; i++) {
        bar.x1 = coords.x1 + i * 16;
        bar.x2 = bar.x1 + 10;
        bar.y2 = coords.y2 - 2;
        bar.y1 = bar.y2 - heights[i] + 1;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = i < level ? gw_ui_c_text() : gw_ui_c_white();
        dsc.bg_opa = LV_OPA_COVER;
        dsc.border_width = GW_UI_HAIRLINE;
        dsc.border_color = gw_ui_c_border();
        dsc.radius = 0;
        lv_draw_rect(draw_ctx, &dsc, &bar);
    }
}

lv_obj_t *gw_ui_kit_signal_bars_create(lv_obj_t *parent, int level) {
    lv_obj_t *bars = lv_obj_create(parent);
    lv_obj_set_size(bars, 64, 40);
    gw_ui_style_plain_panel(bars);
    lv_obj_set_user_data(bars, (void *)(intptr_t)level);
    lv_obj_add_event_cb(bars, signal_bars_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return bars;
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
