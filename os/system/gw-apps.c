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
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define GW_PACKAGE_MAX_BYTES (128L * 1024 * 1024)
#define GW_PAYLOAD_MAX_BYTES (256L * 1024 * 1024)
#define GW_STAGING_RESERVE   (16L * 1024 * 1024)

#ifndef GW_BUSYBOX_PATH
#define GW_BUSYBOX_PATH "/bin/busybox"
#endif

#ifdef GW_APPS_TESTING
/* Test seam: called after the staged copy is verified, before extraction. */
void gw_apps_test_after_verify(const char *original, const char *staged);
const char *gw_apps_test_root(void);
const char *gw_apps_test_data_root(void);
#undef GW_APPS_ROOT
#undef GW_APP_DATA_ROOT
#define GW_APPS_ROOT gw_apps_test_root()
#define GW_APP_DATA_ROOT gw_apps_test_data_root()
#endif

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
    unsigned long run = 0;
    if (!p || !*p || strlen(p) >= 32) return 0;
    for (; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '-' ||
              *p == '_')) return 0;
        if (*p >= '0' && *p <= '9') {
            run = run * 10 + (unsigned long)(*p - '0');
            if (run > 65535) return 0;
        } else {
            run = 0;
        }
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
                  size_t, void *), void *ctx, int timeout_seconds,
                  char *err, size_t err_size) {
    int pipefd[2] = { -1, -1 };
    pid_t pid;
    int status = 0;
    if (capture && pipe(pipefd) < 0) { set_error(err, err_size, "pipe-failed"); return -1; }
    pid = fork();
    if (pid < 0) {
        if (pipefd[0] >= 0) { close(pipefd[0]); close(pipefd[1]); }
        set_error(err, err_size, "fork-failed");
        return -1;
    }
    if (pid == 0) {
        if (timeout_seconds > 0) alarm((unsigned)timeout_seconds);
        /* Bound any single file written during extraction.  Listing helpers
         * decompress into a temporary file, so only limit non-capture runs. */
        if (!capture) {
            struct rlimit limit;
            limit.rlim_cur = GW_PAYLOAD_MAX_BYTES + 4096;
            limit.rlim_max = GW_PAYLOAD_MAX_BYTES + 4096;
            setrlimit(RLIMIT_FSIZE, &limit);
        }
        if (capture) {
            close(pipefd[0]);
            if (dup2(pipefd[1], STDOUT_FILENO) < 0 ||
                dup2(pipefd[1], STDERR_FILENO) < 0)
                _exit(126);
            /* BusyBox tar reports unsafe member rewrites on stderr; mixing it
             * into the captured stream makes those lines fail name/type
             * validation instead of being silently normalised. */
            close(pipefd[1]);
        }
        execv(argv[0], argv);
        _exit(127);
    }
    if (capture) {
        char line[1024];
        size_t used = 0;
        int too_long = 0;
        char chunk[1024];
        close(pipefd[1]);
        for (;;) {
            ssize_t n = read(pipefd[0], chunk, sizeof(chunk));
            ssize_t i;
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) {
                close(pipefd[0]);
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                set_error(err, err_size, "pipe-read-failed");
                return -1;
            }
            if (n == 0) break;
            for (i = 0; i < n; i++) {
                char c = chunk[i];
                if (c == '\n' || c == '\r') {
                    if (too_long) {
                        close(pipefd[0]);
                        kill(pid, SIGKILL);
                        waitpid(pid, NULL, 0);
                        set_error(err, err_size, "tar-line-too-long");
                        return -1;
                    }
                    line[used] = '\0';
                    if (line_cb) line_cb(line, used, ctx);
                    used = 0;
                } else if (used + 1 >= sizeof(line)) {
                    too_long = 1;
                } else {
                    line[used++] = c;
                }
            }
        }
        if (used && !too_long) {
            line[used] = '\0';
            if (line_cb) line_cb(line, used, ctx);
        }
        close(pipefd[0]);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        set_error(err, err_size, "wait-failed");
        return -1;
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
        set_error(err, err_size, "tool-timeout");
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        char detail[64];
        if (WIFSIGNALED(status))
            snprintf(detail, sizeof(detail), "tar-signal-%d", WTERMSIG(status));
        else
            snprintf(detail, sizeof(detail), "tar-exit-%d", WEXITSTATUS(status));
        set_error(err, err_size, detail);
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
    size_t total_size;
    char **names;
    int names_capacity;
} list_state_t;

static void list_free(list_state_t *state) {
    int i;
    if (!state->names) return;
    for (i = 0; i < state->entries; i++) free(state->names[i]);
    free(state->names);
    state->names = NULL;
    state->entries = 0;
    state->names_capacity = 0;
}

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

static void list_line(const char *line, size_t length, void *ctx) {
    list_state_t *state = (list_state_t *)ctx;
    int i;
    if (memchr(line, '\0', length) != NULL) { state->unsafe = 1; return; }
    state->entries++;
    state->name_bytes += length;
    if (!strcmp(line, "manifest") || !strcmp(line, "./manifest"))
        state->manifest = 1;
    if (!safe_member_name(line)) { state->unsafe = 1; return; }
    if (state->entries > 512) { state->unsafe = 1; return; }
    /* Reject duplicate members: a later duplicate can overwrite an already
     * validated file (or a directory) during extraction. */
    for (i = 0; i < state->entries - 1 && state->names && state->names[i]; i++)
        if (!strcmp(state->names[i], line)) { state->unsafe = 1; return; }
    if (!state->names || state->names_capacity < state->entries) {
        int capacity = state->names_capacity ? state->names_capacity * 2 : 64;
        char **grown;
        if (capacity < state->entries) capacity = state->entries;
        grown = realloc(state->names, (size_t)capacity * sizeof(*grown));
        if (!grown) { state->unsafe = 1; return; }
        state->names = grown;
        state->names_capacity = capacity;
    }
    state->names[state->entries - 1] = strdup(line);
    if (!state->names[state->entries - 1]) state->unsafe = 1;
}

/* BusyBox "tar -tv" prints the mode string first: regular files start with
 * '-', directories with 'd'. Anything else (symlink, hardlink, device, FIFO)
 * is refused before extraction so a link cannot redirect a later member.
 * Set-ID modes are refused as well. */
static void type_line(const char *line, size_t length, void *ctx) {
    list_state_t *state = (list_state_t *)ctx;
    if (memchr(line, '\0', length) != NULL) { state->unsafe = 1; return; }
    if (!line[0] || (line[0] != '-' && line[0] != 'd')) { state->unsafe = 1; return; }
    if (length < 7) { state->unsafe = 1; return; }
    if (line[3] == 's' || line[3] == 'S' ||
        line[6] == 's' || line[6] == 'S') state->unsafe = 1;
}

/* Reject links, hardlinks, set-ID files and special files anywhere in the
 * extracted tree.  Set-ID bits are also cleared as defence in depth. */
static int walk_payload(const char *path, long *bytes, int *files,
                        char *err, size_t err_size) {
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
            if (walk_payload(child, bytes, files, err, err_size) < 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (st.st_nlink != 1) {
                closedir(dir);
                set_error(err, err_size, "hardlink-rejected");
                return -1;
            }
            if (st.st_mode & (S_ISUID | S_ISGID)) {
                int fd = open(child, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
                if (fd < 0 || fchmod(fd, st.st_mode & 0777) < 0) {
                    if (fd >= 0) close(fd);
                    closedir(dir);
                    set_error(err, err_size, "setid-clear-failed");
                    return -1;
                }
                close(fd);
            }
            (*files)++;
            if (*files > 512) {
                closedir(dir);
                set_error(err, err_size, "too-many-members");
                return -1;
            }
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

static int ensure_dir(const char *path, mode_t mode, char *err, size_t err_size) {
    struct stat st;
    if (mkdir(path, mode) < 0 && errno != EEXIST) {
        set_error(err, err_size, "directory-unavailable");
        return -1;
    }
    if (lstat(path, &st) < 0 || !S_ISDIR(st.st_mode)) {
        set_error(err, err_size, "directory-invalid");
        return -1;
    }
    if (geteuid() == 0 && st.st_uid != 0) {
        set_error(err, err_size, "directory-not-root-owned");
        return -1;
    }
    return 0;
}

static int remove_tree(const char *path) {
    char *argv[] = { (char *)GW_BUSYBOX_PATH, (char *)"rm", (char *)"-rf",
                     (char *)path, NULL };
    return run_tool(argv, 0, NULL, NULL, 60, NULL, 0);
}

/* Copy a caller-supplied file into the private staging directory.  The source
 * is opened without following links and the copy is fsynced before anything
 * reads it.  A later change to the original path cannot affect verification
 * or extraction. */
static int copy_into_staging(const char *source, const char *destination,
                             long *size, char *err, size_t err_size) {
    int in = -1, out = -1;
    struct stat st;
    char buffer[32768];
    ssize_t n;
    int result = -1;
    in = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (in < 0 || fstat(in, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        set_error(err, err_size, "package-invalid");
        goto done;
    }
    if (size) *size = (long)st.st_size;
    out = open(destination, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
               0600);
    if (out < 0) { set_error(err, err_size, "staging-copy-failed"); goto done; }
    for (;;) {
        n = read(in, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { set_error(err, err_size, "staging-read-failed"); goto done; }
        if (n == 0) break;
        {
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(out, buffer + written, (size_t)(n - written));
                if (w < 0 && errno == EINTR) continue;
                if (w <= 0) { set_error(err, err_size, "staging-write-failed"); goto done; }
                written += w;
            }
        }
    }
    if (fsync(out) < 0) { set_error(err, err_size, "staging-sync-failed"); goto done; }
    result = 0;

done:
    if (in >= 0) close(in);
    if (out >= 0) close(out);
    return result;
}

static int sync_dir(const char *path) {
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int result;
    if (fd < 0) return -1;
    result = fsync(fd);
    close(fd);
    return result;
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

static void manifest_line(const char *line, size_t length, void *ctx) {
    manifest_buffer_t *buffer = (manifest_buffer_t *)ctx;
    if (memchr(line, '\0', length) != NULL) { buffer->overflow = 1; return; }
    if (buffer->used + length + 1 >= sizeof(buffer->data)) {
        buffer->overflow = 1;
        return;
    }
    memcpy(buffer->data + buffer->used, line, length);
    buffer->used += length;
    buffer->data[buffer->used++] = '\n';
    buffer->data[buffer->used] = '\0';
}

int gw_apps_verify_package(const char *package, char *err, size_t err_size) {
    char sig_path[PATH_MAX];
    if (!package || !*package) { set_error(err, err_size, "invalid-argument"); return -1; }
    if (snprintf(sig_path, sizeof(sig_path), "%s.sig", package) >=
        (int)sizeof(sig_path)) { set_error(err, err_size, "path-too-long"); return -1; }
    return gw_signature_verify_file(package, sig_path, GW_APP_KEY_PATH,
                                    err, err_size);
}

int gw_apps_package_info(const char *package, gw_app_info_t *out) {
    manifest_buffer_t buffer;
    gw_app_manifest_t app;
    char *argv[] = { (char *)GW_BUSYBOX_PATH, (char *)"tar", (char *)"-xzOf",
                     (char *)package, (char *)"manifest", NULL };
    char err[96] = "";
    if (!package || !out) return -1;
    memset(&buffer, 0, sizeof(buffer));
    if (run_tool(argv, 1, manifest_line, &buffer, 30, err, sizeof(err)) < 0)
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
            if (parts[i] > 65535U) { *rest = text; return 0; }
            parts[i] = parts[i] * 10 + (unsigned)(*p - '0');
            p++;
        }
        if (parts[i] > 65535U) { *rest = text; return 0; }
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

static void type_size_line(const char *line, size_t length, void *ctx) {
    list_state_t *state = (list_state_t *)ctx;
    const char *p, *end;
    unsigned long value = 0;
    type_line(line, length, ctx);
    if (state->unsafe) return;
    p = line;
    end = line + length;
    /* Skip mode, then owner/group, then read the decimal size field. */
    while (p < end && *p != ' ') p++;
    while (p < end && *p == ' ') p++;
    while (p < end && *p != ' ') p++;
    while (p < end && *p == ' ') p++;
    if (p >= end || *p < '0' || *p > '9') { state->unsafe = 1; return; }
    while (p < end && *p >= '0' && *p <= '9') {
        if (value > GW_PAYLOAD_MAX_BYTES) { state->unsafe = 1; return; }
        value = value * 10 + (unsigned long)(*p++ - '0');
    }
    if (value > (unsigned long)GW_PAYLOAD_MAX_BYTES ||
        state->total_size > (size_t)GW_PAYLOAD_MAX_BYTES - (size_t)value) {
        state->unsafe = 1;
        return;
    }
    state->total_size += (size_t)value;
}

int gw_apps_install(const char *package, const char *signature,
                    char *err, size_t err_size) {
    char sig_path[PATH_MAX], staging[PATH_MAX], payload_dir[PATH_MAX];
    char staged_package[PATH_MAX], staged_signature[PATH_MAX];
    char destination[PATH_MAX], tmp_link[PATH_MAX], active[PATH_MAX];
    char data_dir[PATH_MAX], manifest_path[PATH_MAX], version_dir[PATH_MAX];
    char root_parent[PATH_MAX];
    struct stat st;
    long package_size = 0;
    gw_app_manifest_t app;
    list_state_t list;
    long payload_bytes = 0;
    int payload_files = 0;
    int sig_result;
    char *tar_argv[10];
    int result = -1;

    memset(&list, 0, sizeof(list));
    staging[0] = '\0';
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
    if (snprintf(root_parent, sizeof(root_parent), "%s", GW_APPS_ROOT) >=
        (int)sizeof(root_parent)) { set_error(err, err_size, "path-too-long"); return -1; }
    {
        char *slash = strrchr(root_parent, '/');
        if (!slash || slash == root_parent) { set_error(err, err_size, "path-too-long"); return -1; }
        *slash = '\0';
    }
    if (ensure_dir(root_parent, 0700, err, err_size) < 0) return -1;
    if (ensure_dir(GW_APPS_ROOT, 0755, err, err_size) < 0) return -1;
    if (free_space_ok(GW_APPS_ROOT, package_size, err, err_size) < 0) return -1;

    if (snprintf(staging, sizeof(staging), "%s/.staging-XXXXXX", GW_APPS_ROOT) >=
        (int)sizeof(staging)) { set_error(err, err_size, "path-too-long"); return -1; }
    if (!mkdtemp(staging) || lstat(staging, &st) < 0 ||
        !S_ISDIR(st.st_mode) || (geteuid() == 0 && st.st_uid != 0)) {
        set_error(err, err_size, "staging-failed");
        staging[0] = '\0';
        return -1;
    }
    if (chmod(staging, 0700) < 0) { set_error(err, err_size, "staging-mode-failed"); goto done; }
    if (snprintf(staged_package, sizeof(staged_package), "%s/package", staging) >=
        (int)sizeof(staged_package) ||
        snprintf(staged_signature, sizeof(staged_signature), "%s/package.sig", staging) >=
        (int)sizeof(staged_signature) ||
        snprintf(payload_dir, sizeof(payload_dir), "%s/payload", staging) >=
        (int)sizeof(payload_dir)) {
        set_error(err, err_size, "path-too-long");
        goto done;
    }
    /* Work only on private copies.  The caller-supplied paths stop being used
     * after this point, so swapping them cannot race verification. */
    if (copy_into_staging(package, staged_package, &package_size, err, err_size) < 0)
        goto done;
    if (copy_into_staging(signature, staged_signature, NULL, err, err_size) < 0)
        goto done;
    sig_result = gw_signature_verify_file(staged_package, staged_signature,
                                          GW_APP_KEY_PATH, err, err_size);
    if (sig_result != 0) goto done;
#ifdef GW_APPS_TESTING
    gw_apps_test_after_verify(package, staged_package);
#endif
    if (mkdir(payload_dir, 0700) < 0) {
        set_error(err, err_size, "staging-payload-failed");
        goto done;
    }

    /* Validate every tar member name before extracting anything. */
    tar_argv[0] = (char *)GW_BUSYBOX_PATH;
    tar_argv[1] = (char *)"tar";
    tar_argv[2] = (char *)"-tzf";
    tar_argv[3] = staged_package;
    tar_argv[4] = NULL;
    if (run_tool(tar_argv, 1, list_line, &list, 60, err, err_size) < 0) goto done;
    if (!list.manifest || list.unsafe || list.entries <= 0 || list.entries > 512 ||
        list.name_bytes > 1024 * 1024) {
        set_error(err, err_size, "unsafe-or-incomplete-package");
        goto done;
    }
    /* Second pass: reject non-file/non-directory member types, set-ID modes
     * and an over-large declared decompressed size. */
    tar_argv[2] = (char *)"-tvzf";
    list.unsafe = 0;
    list.total_size = 0;
    if (run_tool(tar_argv, 1, type_size_line, &list, 60, err, err_size) < 0) goto done;
    if (list.unsafe) {
        set_error(err, err_size, "link-special-or-oversized-rejected");
        goto done;
    }

    tar_argv[2] = (char *)"-xzf";
    tar_argv[4] = (char *)"--no-same-owner";
    tar_argv[5] = (char *)"--no-same-permissions";
    tar_argv[6] = (char *)"-C";
    tar_argv[7] = payload_dir;
    tar_argv[8] = NULL;
    if (run_tool(tar_argv, 0, NULL, NULL, 120, err, err_size) < 0) goto done;

    if (walk_payload(payload_dir, &payload_bytes, &payload_files, err, err_size) < 0)
        goto done;
    if (snprintf(manifest_path, sizeof(manifest_path), "%s/manifest", payload_dir) >=
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
        if (snprintf(entry, sizeof(entry), "%s/%s", payload_dir, app.entrypoint) >=
            (int)sizeof(entry) || lstat(entry, &st) < 0 ||
            !S_ISREG(st.st_mode) || !(st.st_mode & 0111) || st.st_nlink != 1) {
            set_error(err, err_size, "entrypoint-invalid");
            goto done;
        }
    }

    if (snprintf(destination, sizeof(destination), "%s/%s", GW_APPS_ROOT, app.id) >=
        (int)sizeof(destination)) { set_error(err, err_size, "path-too-long"); goto done; }
    if (ensure_dir(destination, 0755, err, err_size) < 0) goto done;
    if (snprintf(version_dir, sizeof(version_dir), "%s/%s", destination, app.version) >=
        (int)sizeof(version_dir)) { set_error(err, err_size, "path-too-long"); goto done; }
    if (lstat(version_dir, &st) == 0) {
        set_error(err, err_size, "version-already-installed");
        goto done;
    }
    if (rename(payload_dir, version_dir) < 0) {
        set_error(err, err_size, "activate-rename-failed");
        goto done;
    }
    payload_dir[0] = '\0';   /* moved */

    /* Persist the new version before any link points at it. */
    if (sync_dir(version_dir) < 0 || sync_dir(destination) < 0) {
        set_error(err, err_size, "activate-sync-failed");
        goto rollback_version;
    }
    if (ensure_dir(GW_APP_DATA_ROOT, 0700, err, err_size) < 0)
        goto rollback_version;
    {
        char data_parent[PATH_MAX];
        if (snprintf(data_parent, sizeof(data_parent), "%s", GW_APP_DATA_ROOT) >=
            (int)sizeof(data_parent)) { set_error(err, err_size, "path-too-long"); goto rollback_version; }
        {
            char *slash = strrchr(data_parent, '/');
            if (slash && slash != data_parent) {
                *slash = '\0';
                if (ensure_dir(data_parent, 0700, err, err_size) < 0)
                    goto rollback_version;
            }
        }
    }
    if (snprintf(data_dir, sizeof(data_dir), "%s/%s", GW_APP_DATA_ROOT, app.id) >=
        (int)sizeof(data_dir)) { set_error(err, err_size, "path-too-long"); goto rollback_version; }
    if (ensure_dir(data_dir, 0700, err, err_size) < 0) goto rollback_version;

    if (snprintf(active, sizeof(active), "%s/active", destination) >=
        (int)sizeof(active) ||
        snprintf(tmp_link, sizeof(tmp_link), "%s/.active.%ld", destination,
                 (long)getpid()) >= (int)sizeof(tmp_link)) {
        set_error(err, err_size, "path-too-long");
        goto rollback_version;
    }
    unlink(tmp_link);
    if (symlink(app.version, tmp_link) < 0 || rename(tmp_link, active) < 0) {
        unlink(tmp_link);
        set_error(err, err_size, "active-link-failed");
        goto rollback_version;
    }
    if (sync_dir(destination) < 0) {
        set_error(err, err_size, "active-sync-failed");
        /* The link is live and points at a complete version; a failed sync is
         * reported but the previous version stays available for resume. */
        result = -1;
        goto done;
    }
    result = 0;
    goto done;

rollback_version:
    /* No active link points at the new version yet: remove the partial
     * activation so a retry starts from a clean state. */
    if (remove_tree(version_dir) < 0 && err[0] == '\0')
        set_error(err, err_size, "rollback-failed");

done:
    list_free(&list);
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
