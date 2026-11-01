    }

    {
        /* D02: the hero date above already shows weekday/month/year.  The
         * calendar card adds information instead of repeating it. */
        char day[8], month[48];
        int due_month = 0;
        int due_day = 0;
        char month_prefix[12];
        snprintf(day, sizeof(day), "%d", t.tm_mday);
        snprintf(month_prefix, sizeof(month_prefix), "%04d-%02d",
                 t.tm_year + 1900, t.tm_mon + 1);
        for (i = 0; i < g_todo_count; i++) {
            if (g_todos[i].status != GW_TODO_ACTIVE || !g_todos[i].due[0])
                continue;
            if (!strncmp(g_todos[i].due, month_prefix, 7)) due_month++;
            if (!strncmp(g_todos[i].due, month_prefix, 7) &&
                atoi(g_todos[i].due + 8) == t.tm_mday)
                due_day++;
        }
        if (due_month > 0)
            snprintf(month, sizeof(month), "%d due this month", due_month);
        else
            snprintf(month, sizeof(month), "No due dates this month");
        if (due_day > 0)
            snprintf(month, sizeof(month), "%d due today  |  %d this month",
                     due_day, due_month);
        label_set_if_changed(g_widget_day_label, day);
        label_set_if_changed(g_widget_month_label, month);
        container_set_hidden(g_widget_weekday_label, 1);
    }
    {
        struct tm week = t;
        int offset = (t.tm_wday + 6) % 7;
        int i;
        week.tm_mday -= offset;
        week.tm_hour = 12;
        week.tm_min = 0;
        week.tm_sec = 0;
        mktime(&week);
        for (i = 0; i < 7; i++) {
            lv_obj_t *label = g_widget_week_labels[i];
            char day[8];
            int is_today;
            if (!label) continue;
            snprintf(day, sizeof(day), "%d", week.tm_mday);
            is_today = week.tm_mday == t.tm_mday &&
                       week.tm_mon == t.tm_mon &&
                       week.tm_year == t.tm_year;
            label_set_if_changed(label, day);
            lv_obj_set_style_text_font(label,
                                       is_today ? &gw_font_32_bold
                                                : &gw_font_28,
                                       LV_PART_MAIN);
            lv_obj_set_style_text_decor(label,
                                        is_today ? LV_TEXT_DECOR_UNDERLINE
                                                 : LV_TEXT_DECOR_NONE,
                                        LV_PART_MAIN);
            lv_obj_set_style_text_color(label,
                                        is_today ? gw_ui_c_text()
                                                 : gw_ui_c_secondary(),
                                        LV_PART_MAIN);
            week.tm_mday++;
            mktime(&week);
        }
    }

    {
        int rows = 0;
        if (g_model.event_title[0]) {
            snprintf(text, sizeof(text), "%s | %s",
                     g_model.event_when[0] ? g_model.event_when : GW_STR_TODAY,
                     g_model.event_title);
            label_set_if_changed(g_widget_event_labels[0], text);
            rows = 1;
        } else {
