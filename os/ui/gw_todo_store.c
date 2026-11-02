/* SPDX-License-Identifier: GPL-3.0-or-later
 * To-do storage and parsing helpers for the gw-shell UI. */
#include "gw_shell_internal.h"

#ifndef GW_UI_HOST_SIM
#define GW_UI_HOST_SIM 0
#endif

#define GW_TODO_PATH "/var/lib/gleworks/todos.tsv"

GW_SHELL_INTERNAL void todo_sanitize(char *dst, size_t dst_size, const char *src) {
    size_t j = 0;
    if (!dst || dst_size == 0) return;
    while (src && *src && j + 1 < dst_size) {
        char c = *src++;
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        dst[j++] = c;
    }
    while (j > 0 && dst[j - 1] == ' ') j--;
    dst[j] = '\0';
}

GW_SHELL_INTERNAL void todo_copy_due(char *dst, size_t dst_size, const char *src) {
    int year, month, day;
    if (!src || !src[0] || sscanf(src, "%d-%d-%d", &year, &month, &day) != 3 ||
        month < 1 || month > 12 || day < 1 || day > 31) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%04d-%02d-%02d", year, month, day);
}

#if !GW_UI_HOST_SIM
GW_SHELL_INTERNAL int todo_store_save(void) {
    char buffer[GW_TODO_MAX * (GW_TODO_TEXT_MAX + 96)];
    size_t used = 0;
    int i;
    for (i = 0; i < g_todo_count; i++) {
        int n = snprintf(buffer + used, sizeof(buffer) - used,
                         "%d\t%d\t%ld\t%ld\t%s\t%s\n",
                         g_todos[i].id, g_todos[i].status,
                         g_todos[i].created, g_todos[i].updated,
                         g_todos[i].due, g_todos[i].text);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) return -1;
        used += (size_t)n;
    }
    return gw_write_atomic(GW_TODO_PATH, buffer, used, 0600);
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
        int fd = open(GW_TODO_PATH, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        FILE *f = fd >= 0 ? fdopen(fd, "r") : NULL;
        char line[512];
        if (!f) { if (fd >= 0) close(fd); return; }
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
    int i, n = 0;
    format_iso_date(year, month, day, iso, sizeof(iso));
    for (i = 0; i < g_todo_count; i++)
        if (g_todos[i].status == GW_TODO_ACTIVE && !strcmp(g_todos[i].due, iso))
            n++;
    return n;
}

GW_SHELL_INTERNAL int todo_add(const char *text, const char *due) {
    char clean[GW_TODO_TEXT_MAX];
    gw_todo_t *t;
    if (!text) return -1;
    todo_sanitize(clean, sizeof(clean), text);
    if (!clean[0]) return -1;
    if (g_todo_count >= GW_TODO_MAX) return -2;
    t = &g_todos[g_todo_count++];
    t->id = g_todo_next_id++;
    t->status = GW_TODO_ACTIVE;
    t->created = t->updated = (long)time(NULL);
    todo_copy_due(t->due, sizeof(t->due), due);
    snprintf(t->text, sizeof(t->text), "%s", clean);
    return 0;
}

GW_SHELL_INTERNAL void todo_toggle(int id) {
    gw_todo_t *t = todo_find(id);
    if (!t || t->status == GW_TODO_DELETED) return;
    t->status = t->status == GW_TODO_ACTIVE ? GW_TODO_DONE : GW_TODO_ACTIVE;
    t->updated = (long)time(NULL);
}

GW_SHELL_INTERNAL void todo_delete(int id) {
    gw_todo_t *t = todo_find(id);
    if (!t) return;
    t->status = GW_TODO_DELETED;
    t->updated = (long)time(NULL);
    g_todo_last_deleted = id;
}

GW_SHELL_INTERNAL void todo_restore(int id) {
    gw_todo_t *t = todo_find(id);
    if (!t) return;
    t->status = GW_TODO_ACTIVE;
    t->updated = (long)time(NULL);
}

GW_SHELL_INTERNAL int todo_commit(void) {
    int save_rc = todo_store_save();
    todo_rebuild_list();
    apply_model();
    if (save_rc < 0)
        label_set_if_changed(g_notes_status, GW_STR_TODO_SAVE_FAILED);
    return save_rc;
}
