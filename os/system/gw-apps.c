/* SPDX-License-Identifier: GPL-3.0-or-later
 * Signed app bundle installer. See gw-apps.h for the layout.
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include "gw-apps.h"
#include "gw-app-manifest.h"
#include "gw-signature.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define GW_PACKAGE_MAX_BYTES (128L * 1024 * 1024)
#define GW_PAYLOAD_MAX_BYTES (256L * 1024 * 1024)
#define GW_STAGING_RESERVE   (16L * 1024 * 1024)

static void set_error(char *err, size_t size, const char *message) {
    if (err && size) snprintf(err, size, "%s", message);
}

static int valid_id(const char *id) {
    const char *p = id;
    size_t n;
    if (!p || !*p) return 0;
    n = strlen(p);
    if (n >= 64) return 0;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) return 0;
    for (; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              *p == '-' || *p == '.')) return 0;
    }
    return strcmp(id, ".") != 0 && strcmp(id, "..") != 0;
}

static int valid_version(const char *version) {
    const char *p = version;
    if (!p || !*p || strlen(p) >= 32) return 0;
    for (; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '-' ||
              *p == '_')) return 0;
    }
    return 1;
}

static int read_manifest_file(const char *path, gw_app_manifest_t *app) {
    char data[GW_APP_MANIFEST_MAX + 1];
    FILE *file = fopen(path, "rb");
    size_t n;
    if (!file) return -1;
    n = fread(data, 1, GW_APP_MANIFEST_MAX, file);
    if (ferror(file) || !feof(file)) { fclose(file); return -1; }
    fclose(file);
    return gw_app_manifest_parse(data, n, app);
}

static int app_from_dir(const char *id, const char *dir, gw_app_info_t *out) {
    char manifest[PATH_MAX];
    gw_app_manifest_t app;
    if (snprintf(manifest, sizeof(manifest), "%s/active/manifest", dir) >= (int)sizeof(manifest))
        return -1;
    if (read_manifest_file(manifest, &app) < 0) return -1;
    if (strcmp(app.id, id) != 0) return -1;
    snprintf(out->id, sizeof(out->id), "%s", app.id);
    snprintf(out->version, sizeof(out->version), "%s", app.version);
    snprintf(out->description, sizeof(out->description), "%s", app.description);
    snprintf(out->entrypoint, sizeof(out->entrypoint), "%s", app.entrypoint);
    return 0;
}

int gw_apps_list(gw_app_info_t *out, int max) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    if (!out || max <= 0) return -1;
    dir = opendir(GW_APPS_ROOT);
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL && count < max) {
        char path[PATH_MAX];
        struct stat st;
        gw_app_info_t info;
        int i, j;
        if (!valid_id(entry->d_name)) continue;
        if (snprintf(path, sizeof(path), "%s/%s", GW_APPS_ROOT, entry->d_name) >=
            (int)sizeof(path)) continue;
        if (lstat(path, &st) < 0 || !S_ISDIR(st.st_mode)) continue;
        if (app_from_dir(entry->d_name, path, &info) < 0) continue;
        /* Insertion sort by id for a stable UI order. */
        for (i = 0; i < count; i++)
            if (strcmp(out[i].id, info.id) > 0) break;
        for (j = count; j > i; j--) out[j] = out[j - 1];
        out[i] = info;
        count++;
    }
    closedir(dir);
    return count;
}

int gw_apps_find(const char *id, gw_app_info_t *out) {
    char path[PATH_MAX];
    if (!valid_id(id) || !out) return -1;
    if (snprintf(path, sizeof(path), "%s/%s", GW_APPS_ROOT, id) >= (int)sizeof(path))
        return -1;
    return app_from_dir(id, path, out);
}

/* ── Child process helpers ───────────────────────────────────────────────── */

static int run_tool(char *const argv[], int capture, void (*line_cb)(const char *,
                  void *), void *ctx, char *err, size_t err_size) {
    int pipefd[2] = { -1, -1 };
    pid_t pid;
    int status;
    if (capture && pipe(pipefd) < 0) { set_error(err, err_size, "pipe-failed"); return -1; }
    pid = fork();
    if (pid < 0) {
        if (pipefd[0] >= 0) { close(pipefd[0]); close(pipefd[1]); }
        set_error(err, err_size, "fork-failed");
        return -1;
    }
    if (pid == 0) {
        if (capture) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            /* BusyBox tar reports unsafe member rewrites on stderr; mixing it
             * into the captured stream makes those lines fail name/type
             * validation instead of being silently normalised. */
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }
        execv(argv[0], argv);
        _exit(127);
    }
    if (capture) {
        FILE *stream;
        char line[1024];
        close(pipefd[1]);
        stream = fdopen(pipefd[0], "r");
        if (!stream) {
            close(pipefd[0]);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            set_error(err, err_size, "pipe-read-failed");
            return -1;
        }
        while (fgets(line, sizeof(line), stream)) {
            size_t n = strlen(line);
            if (n == sizeof(line) - 1 && line[n - 1] != '\n') {
                fclose(stream);
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                set_error(err, err_size, "tar-line-too-long");
                return -1;
            }
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
            if (line_cb) line_cb(line, ctx);
        }
        fclose(stream);
    }
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        set_error(err, err_size, "tar-failed");
        return -1;
    }
    return 0;
}

/* ── Entry validation ────────────────────────────────────────────────────── */

typedef struct {
    int entries;
    int manifest;
    int unsafe;
    size_t name_bytes;
} list_state_t;

static int safe_member_name(const char *name) {
    const char *p = name;
    size_t total = strlen(name);
    if (!total || total >= 512) return 0;
    if (*p == '/') return 0;
    /* Normalise a leading "./" used by some tar writers. */
    if (p[0] == '.' && p[1] == '/') p += 2;
    if (!*p) return 0;
    for (;;) {
        const char *slash = strchr(p, '/');
        size_t n = slash ? (size_t)(slash - p) : strlen(p);
        size_t i;
        if (!n || (n == 1 && p[0] == '.') ||
            (n == 2 && p[0] == '.' && p[1] == '.')) return 0;
        for (i = 0; i < n; i++) {
            char c = p[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                  c == '_' || c == '+')) return 0;
        }
        if (!slash) break;
        p = slash + 1;
    }
    return 1;
}

static void list_line(const char *line, void *ctx) {
    list_state_t *state = (list_state_t *)ctx;
    state->entries++;
    state->name_bytes += strlen(line);
    if (!strcmp(line, "manifest") || !strcmp(line, "./manifest"))
        state->manifest = 1;
    if (!safe_member_name(line)) state->unsafe = 1;
}

/* BusyBox "tar -tv" prints the mode string first: regular files start with
 * '-', directories with 'd'. Anything else (symlink, hardlink, device, FIFO)
 * is refused before extraction so a link cannot redirect a later member. */
static void type_line(const char *line, void *ctx) {
    list_state_t *state = (list_state_t *)ctx;
    if (!line[0] || (line[0] != '-' && line[0] != 'd')) state->unsafe = 1;
}

/* Reject links and special files anywhere in the extracted tree. */
static int walk_payload(const char *path, long *bytes, char *err, size_t err_size) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    if (!dir) { set_error(err, err_size, "payload-unreadable"); return -1; }
    while ((entry = readdir(dir)) != NULL) {
        char child[PATH_MAX];
        struct stat st;
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >=
            (int)sizeof(child)) {
            closedir(dir);
            set_error(err, err_size, "path-too-long");
            return -1;
        }
        if (lstat(child, &st) < 0) {
            closedir(dir);
            set_error(err, err_size, "payload-stat-failed");
            return -1;
        }
        if (S_ISLNK(st.st_mode)) {
            closedir(dir);
            set_error(err, err_size, "symlink-rejected");
            return -1;
        }
        if (S_ISDIR(st.st_mode)) {
            if (walk_payload(child, bytes, err, err_size) < 0) { closedir(dir); return -1; }
        } else if (S_ISREG(st.st_mode)) {
            *bytes += st.st_size;
            if (*bytes > GW_PAYLOAD_MAX_BYTES) {
                closedir(dir);
                set_error(err, err_size, "payload-too-large");
                return -1;
            }
        } else {
            closedir(dir);
            set_error(err, err_size, "special-file-rejected");
            return -1;
        }
    }
    closedir(dir);
    return 0;
}

static int remove_tree(const char *path) {
    char *argv[] = { (char *)"/bin/busybox", (char *)"rm", (char *)"-rf",
                     (char *)path, NULL };
    return run_tool(argv, 0, NULL, NULL, NULL, 0);
}

static int free_space_ok(const char *root, long package_size, char *err, size_t err_size) {
    struct statvfs st;
    unsigned long long need;
    if (statvfs(root, &st) < 0) { set_error(err, err_size, "space-check-failed"); return -1; }
    need = (unsigned long long)package_size * 2 + GW_STAGING_RESERVE;
    if ((unsigned long long)st.f_bavail * st.f_frsize < need) {
        set_error(err, err_size, "insufficient-space");
        return -1;
    }
    return 0;
}

/* ── Package metadata (selection before install) ─────────────────────────── */

typedef struct {
    char data[GW_APP_MANIFEST_MAX + 1];
    size_t used;
    int overflow;
} manifest_buffer_t;

static void manifest_line(const char *line, void *ctx) {
    manifest_buffer_t *buffer = (manifest_buffer_t *)ctx;
    size_t n = strlen(line);
    if (buffer->used + n + 1 >= sizeof(buffer->data)) {
        buffer->overflow = 1;
        return;
    }
    memcpy(buffer->data + buffer->used, line, n);
    buffer->used += n;
    buffer->data[buffer->used++] = '\n';
    buffer->data[buffer->used] = '\0';
}

int gw_apps_package_info(const char *package, gw_app_info_t *out) {
    manifest_buffer_t buffer;
    gw_app_manifest_t app;
    char *argv[] = { (char *)"/bin/busybox", (char *)"tar", (char *)"-xzOf",
                     (char *)package, (char *)"manifest", NULL };
    char err[96] = "";
    if (!package || !out) return -1;
    memset(&buffer, 0, sizeof(buffer));
    if (run_tool(argv, 1, manifest_line, &buffer, err, sizeof(err)) < 0)
        return -1;
    if (buffer.overflow || !buffer.used) return -1;
    if (gw_app_manifest_parse(buffer.data, buffer.used, &app) < 0) return -1;
    memset(out, 0, sizeof(*out));
    snprintf(out->id, sizeof(out->id), "%s", app.id);
    snprintf(out->version, sizeof(out->version), "%s", app.version);
    snprintf(out->description, sizeof(out->description), "%s", app.description);
    snprintf(out->entrypoint, sizeof(out->entrypoint), "%s", app.entrypoint);
    return 0;
}

static int parse_version(const char *text, unsigned parts[3], const char **rest) {
    const char *p = text;
    int i;
    parts[0] = parts[1] = parts[2] = 0;
    for (i = 0; i < 3; i++) {
        if (*p < '0' || *p > '9') {
            if (i == 0) { *rest = text; return 0; }
            *rest = p;
            return 1;
        }
        while (*p >= '0' && *p <= '9') {
            parts[i] = parts[i] * 10 + (unsigned)(*p - '0');
            p++;
        }
        if (i < 2 && *p == '.') { p++; continue; }
        *rest = p;
        return 1;
    }
    *rest = p;
    return 1;
}

int gw_apps_version_compare(const char *left, const char *right) {
    unsigned l[3], r[3];
    const char *lrest = "", *rrest = "";
    int i, ln, rn;
    if (!left || !right) return 0;
    ln = parse_version(left, l, &lrest);
    rn = parse_version(right, r, &rrest);
    if (!ln || !rn) return strcmp(left, right);
    for (i = 0; i < 3; i++) {
        if (l[i] != r[i]) return l[i] < r[i] ? -1 : 1;
    }
    if (!*lrest && !*rrest) return 0;
    /* A "-prerelease" suffix is older than the same numeric release. */
    if (*lrest == '-' && !*rrest) return -1;
    if (*rrest == '-' && !*lrest) return 1;
    return strcmp(lrest, rrest);
}

/* ── Install ─────────────────────────────────────────────────────────────── */

int gw_apps_install(const char *package, const char *signature,
                    char *err, size_t err_size) {
    char sig_path[PATH_MAX], staging[PATH_MAX], destination[PATH_MAX];
    char tmp_link[PATH_MAX], active[PATH_MAX], data_dir[PATH_MAX];
    char manifest_path[PATH_MAX];
    struct stat st;
    struct statvfs vfs;
    long package_size;
    gw_app_manifest_t app;
    list_state_t list = { 0, 0, 0, 0 };
    long payload_bytes = 0;
    int sig_result;
    char *tar_argv[7];
    int result = -1;

    if (!package || !*package) { set_error(err, err_size, "invalid-argument"); return -1; }
    if (!signature) {
        if (snprintf(sig_path, sizeof(sig_path), "%s.sig", package) >=
            (int)sizeof(sig_path)) { set_error(err, err_size, "path-too-long"); return -1; }
        signature = sig_path;
    }
    if (lstat(package, &st) < 0 || !S_ISREG(st.st_mode) ||
        st.st_size <= 0 || st.st_size > GW_PACKAGE_MAX_BYTES) {
        set_error(err, err_size, "package-invalid");
        return -1;
    }
    package_size = (long)st.st_size;
    if (lstat(signature, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size > 4096) {
        set_error(err, err_size, "signature-invalid");
        return -1;
    }
    if (mkdir("/var/lib/gleworks", 0700) < 0 && errno != EEXIST) {
        set_error(err, err_size, "apps-parent-unavailable");
        return -1;
    }
    if (mkdir(GW_APPS_ROOT, 0755) < 0 && errno != EEXIST) {
        set_error(err, err_size, "apps-root-unavailable");
        return -1;
    }
    if (free_space_ok(GW_APPS_ROOT, package_size, err, err_size) < 0) return -1;

    sig_result = gw_signature_verify_file(package, signature, GW_APP_KEY_PATH,
                                          err, err_size);
    if (sig_result != 0) return -1;
    if (snprintf(staging, sizeof(staging), "%s/.staging-XXXXXX", GW_APPS_ROOT) >=
        (int)sizeof(staging)) { set_error(err, err_size, "path-too-long"); return -1; }
    if (!mkdtemp(staging)) { set_error(err, err_size, "staging-failed"); return -1; }

    /* Validate every tar member name before extracting anything. */
    tar_argv[0] = (char *)"/bin/busybox";
    tar_argv[1] = (char *)"tar";
    tar_argv[2] = (char *)"-tzf";
    tar_argv[3] = (char *)package;
    tar_argv[4] = NULL;
    if (run_tool(tar_argv, 1, list_line, &list, err, err_size) < 0) goto done;
    if (!list.manifest || list.unsafe || list.entries <= 0 || list.entries > 512 ||
        list.name_bytes > 1024 * 1024) {
        set_error(err, err_size, "unsafe-or-incomplete-package");
        goto done;
    }
    /* Second pass: reject non-file/non-directory member types. */
    tar_argv[2] = (char *)"-tvzf";
    list.unsafe = 0;
    if (run_tool(tar_argv, 1, type_line, &list, err, err_size) < 0) goto done;
    if (list.unsafe) {
        set_error(err, err_size, "link-or-special-file-rejected");
        goto done;
    }

    tar_argv[2] = (char *)"-xzf";
    tar_argv[4] = (char *)"-C";
    tar_argv[5] = (char *)staging;
    tar_argv[6] = NULL;
    if (run_tool(tar_argv, 0, NULL, NULL, err, err_size) < 0) goto done;

    if (walk_payload(staging, &payload_bytes, err, err_size) < 0) goto done;
    if (snprintf(manifest_path, sizeof(manifest_path), "%s/manifest", staging) >=
        (int)sizeof(manifest_path)) { set_error(err, err_size, "path-too-long"); goto done; }
    if (read_manifest_file(manifest_path, &app) < 0) {
        set_error(err, err_size, "manifest-invalid");
        goto done;
    }
    if (!valid_id(app.id) || !valid_version(app.version)) {
        set_error(err, err_size, "manifest-id-or-version-invalid");
        goto done;
    }
    if (!gw_app_manifest_compatible(&app, "0.1")) {
        set_error(err, err_size, "manifest-incompatible");
        goto done;
    }
    {
        char entry[PATH_MAX];
        if (snprintf(entry, sizeof(entry), "%s/%s", staging, app.entrypoint) >=
            (int)sizeof(entry) || lstat(entry, &st) < 0 ||
            !S_ISREG(st.st_mode) || !(st.st_mode & 0111)) {
            set_error(err, err_size, "entrypoint-invalid");
            goto done;
        }
    }

    if (snprintf(destination, sizeof(destination), "%s/%s", GW_APPS_ROOT, app.id) >=
        (int)sizeof(destination)) { set_error(err, err_size, "path-too-long"); goto done; }
    if (mkdir(destination, 0755) < 0 && errno != EEXIST) {
        set_error(err, err_size, "app-dir-failed");
        goto done;
    }
    {
        char version_dir[PATH_MAX];
        if (snprintf(version_dir, sizeof(version_dir), "%s/%s", destination, app.version) >=
            (int)sizeof(version_dir)) { set_error(err, err_size, "path-too-long"); goto done; }
        if (lstat(version_dir, &st) == 0) {
            set_error(err, err_size, "version-already-installed");
            goto done;
        }
        if (rename(staging, version_dir) < 0) {
            set_error(err, err_size, "activate-rename-failed");
            goto done;
        }
        staging[0] = '\0';   /* moved; nothing to clean up */
        if (snprintf(active, sizeof(active), "%s/active", destination) >=
            (int)sizeof(active) ||
            snprintf(tmp_link, sizeof(tmp_link), "%s/.active.%ld", destination,
                     (long)getpid()) >= (int)sizeof(tmp_link)) {
            set_error(err, err_size, "path-too-long");
            goto done;
        }
        unlink(tmp_link);
        if (symlink(app.version, tmp_link) < 0 || rename(tmp_link, active) < 0) {
            unlink(tmp_link);
            set_error(err, err_size, "active-link-failed");
            goto done;
        }
    }
    if (mkdir(GW_APP_DATA_ROOT, 0700) < 0 && errno != EEXIST) {
        set_error(err, err_size, "data-root-failed");
        goto done;
    }
    if (snprintf(data_dir, sizeof(data_dir), "%s/%s", GW_APP_DATA_ROOT, app.id) <
        (int)sizeof(data_dir)) {
        if (mkdir(data_dir, 0700) < 0 && errno != EEXIST) {
            set_error(err, err_size, "appdata-failed");
            goto done;
        }
    }
    (void)vfs;
    result = 0;

done:
    if (staging[0]) remove_tree(staging);
    return result;
}

/* ── Remove / USB scan ───────────────────────────────────────────────────── */

int gw_apps_remove(const char *id, char *err, size_t err_size) {
    char path[PATH_MAX];
    if (!valid_id(id)) { set_error(err, err_size, "invalid-id"); return -1; }
    if (!strcmp(id, "koreader")) {
        set_error(err, err_size, "builtin-app");
        return -1;
    }
    if (snprintf(path, sizeof(path), "%s/%s", GW_APPS_ROOT, id) >= (int)sizeof(path)) {
        set_error(err, err_size, "path-too-long");
        return -1;
    }
    {
        struct stat st;
        if (lstat(path, &st) < 0 || !S_ISDIR(st.st_mode)) {
            set_error(err, err_size, "not-installed");
            return -1;
        }
    }
    if (remove_tree(path) < 0) { set_error(err, err_size, "remove-failed"); return -1; }
    return 0;
}

int gw_apps_usb_scan(char names[][GW_APP_NAME_MAX], int max) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    if (!names || max <= 0) return -1;
    dir = opendir(GW_APPS_USB_DIR);
    if (!dir) return -1;
    while ((entry = readdir(dir)) != NULL && count < max) {
        char path[PATH_MAX];
        struct stat st;
        size_t n = strlen(entry->d_name);
        if (n <= 6 || n >= GW_APP_NAME_MAX - 5) continue;
        if (strcmp(entry->d_name + n - 6, ".gwapp")) continue;
        if (strchr(entry->d_name, '/') || strchr(entry->d_name, '\\')) continue;
        if (snprintf(path, sizeof(path), "%s/%s", GW_APPS_USB_DIR, entry->d_name) >=
            (int)sizeof(path)) continue;
        if (lstat(path, &st) < 0 || !S_ISREG(st.st_mode)) continue;
        snprintf(path, sizeof(path), "%s/%s.sig", GW_APPS_USB_DIR, entry->d_name);
        if (lstat(path, &st) < 0 || !S_ISREG(st.st_mode)) continue;
        memcpy(names[count], entry->d_name, n + 1);
        count++;
    }
    closedir(dir);
    return count;
}
