#if !GW_UI_HOST_SIM
GW_SHELL_INTERNAL int todo_store_save(void) {
    char tmp[128];
    FILE *f;
    int i;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", GW_TODO_PATH, (long)getpid()) >=
        (int)sizeof(tmp)) return -1;
    f = fopen(tmp, "w");
    if (!f) return -1;
    for (i = 0; i < g_todo_count; i++)
        if (fprintf(f, "%d\t%d\t%ld\t%ld\t%s\t%s\n",
                    g_todos[i].id, g_todos[i].status,
                    g_todos[i].created, g_todos[i].updated,
                    g_todos[i].due, g_todos[i].text) < 0) {
            fclose(f);
            unlink(tmp);
            return -1;
        }
    if (fflush(f) || fsync(fileno(f))) {
        fclose(f);
        unlink(tmp);
        return -1;
    }
    if (fclose(f) || rename(tmp, GW_TODO_PATH) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}
#else
GW_SHELL_INTERNAL int todo_store_save(void) { return 0; }
#endif

/* Tolerant TSV parser: five fields (legacy) or six fields with due date. */
GW_SHELL_INTERNAL int todo_parse_line(const char *line, gw_todo_t *out) {
    char copy[512];
    char *fields[7];
    int nf = 0;
    char *p;
    size_t n;
    if (!line || !out) return -1;
    snprintf(copy, sizeof(copy), "%s", line);
    n = strlen(copy);
    while (n && (copy[n - 1] == '\n' || copy[n - 1] == '\r')) copy[--n] = '\0';
    fields[nf++] = copy;
    for (p = copy; *p && nf < 7; p++) {
        if (*p == '\t') { *p = '\0'; fields[nf++] = p + 1; }
    }
    if (nf < 5) return -1;
    out->id = atoi(fields[0]);
    out->status = atoi(fields[1]);
    out->created = atol(fields[2]);
    out->updated = atol(fields[3]);
    if (out->id <= 0 || out->status < GW_TODO_ACTIVE ||
        out->status > GW_TODO_DELETED)
        return -1;
    todo_copy_due(out->due, sizeof(out->due), nf >= 6 ? fields[4] : "");
    todo_sanitize(out->text, sizeof(out->text), nf >= 6 ? fields[5] : fields[4]);
    return out->text[0] ? 0 : -1;
}

GW_SHELL_INTERNAL void todo_store_load(void) {
    g_todo_count = 0;
    g_todo_next_id = 1;
    g_todo_last_deleted = -1;
#if GW_UI_HOST_SIM
    {
        struct { int status; const char *due; const char *text; } mock[] = {
            { GW_TODO_ACTIVE,  "today",    "Mua pin CR2032 cho đồng hồ" },
            { GW_TODO_ACTIVE,  "today",    "Kiểm tra lịch sạc PW3" },
            { GW_TODO_ACTIVE,  "tomorrow", "Đọc 20 trang sách" },
            { GW_TODO_DONE,    "",         "Sạc Kindle PW3" },
            { GW_TODO_DELETED, "",         "Gọi điện cho Minh" }
        };
        unsigned i;
        for (i = 0; i < sizeof(mock) / sizeof(mock[0]) && g_todo_count < GW_TODO_MAX; i++) {
            gw_todo_t *t = &g_todos[g_todo_count++];
            t->id = g_todo_next_id++;
            t->status = mock[i].status;
            t->created = t->updated = (long)i;
            if (!strcmp(mock[i].due, "today")) today_iso(t->due, sizeof(t->due));
            else if (!strcmp(mock[i].due, "tomorrow"))
                iso_relative_days(1, t->due, sizeof(t->due));
            else t->due[0] = '\0';
            snprintf(t->text, sizeof(t->text), "%s", mock[i].text);
        }
    }
#else
    {
        FILE *f = fopen(GW_TODO_PATH, "r");
        char line[512];
        if (!f) return;
        while (fgets(line, sizeof(line), f) && g_todo_count < GW_TODO_MAX) {
            gw_todo_t parsed;
            if (todo_parse_line(line, &parsed) != 0) continue;
            g_todos[g_todo_count] = parsed;
            if (parsed.id >= g_todo_next_id) g_todo_next_id = parsed.id + 1;
            g_todo_count++;
        }
        fclose(f);
    }
#endif
}

GW_SHELL_INTERNAL gw_todo_t *todo_find(int id) {
    int i;
    for (i = 0; i < g_todo_count; i++)
        if (g_todos[i].id == id) return &g_todos[i];
    return NULL;
}

GW_SHELL_INTERNAL int todo_due_on(int year, int month, int day) {
    char iso[11];
