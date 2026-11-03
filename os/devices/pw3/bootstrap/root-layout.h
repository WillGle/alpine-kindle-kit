/* SPDX-License-Identifier: GPL-3.0-or-later
 * Alpine root-layout validation shared by the rescue bootstrap and its host
 * test.  Kept separate so the check can run without the build-private
 * guardian token or the ARM toolchain.
 */
#ifndef GW_ROOT_LAYOUT_H
#define GW_ROOT_LAYOUT_H

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int path_mode_ok(const struct stat *st, int want_dir, int need_exec,
                        int need_read) {
    int ok = want_dir ? S_ISDIR(st->st_mode) : S_ISREG(st->st_mode);
    if (need_exec && !(st->st_mode & 0111)) ok = 0;
    if (need_read && !(st->st_mode & 0444)) ok = 0;
    return ok;
}

/* Validate a required path by opening it without following links. */
static int checked_path(const char *path, int want_dir, int need_exec,
                        int need_read) {
    struct stat st;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    int ok;
    if (fd < 0) return -1;
    if (fstat(fd, &st)) { close(fd); return -1; }
    ok = path_mode_ok(&st, want_dir, need_exec, need_read);
    close(fd);
    return ok ? 0 : -1;
}

/* /sbin/init is conventionally a symlink to BusyBox, so it cannot be checked
 * with O_NOFOLLOW.  Resolve it, but only accept a regular executable whose
 * target stays inside the mounted Alpine root.  Every other path keeps the
 * strict no-symlink check. */
static int checked_init_path_at(const char *root, const char *path) {
    char resolved[PATH_MAX];
    struct stat st;
    size_t root_len = strlen(root);
    if (!realpath(path, resolved)) return -1;
    if (strncmp(resolved, root, root_len) || resolved[root_len] != '/') return -1;
    if (stat(resolved, &st)) return -1;
    return path_mode_ok(&st, 0, 1, 0) ? 0 : -1;
}

/* Validate the complete Alpine root layout.  Shared by the real boot and by
 * the image-build check so a symlinked /sbin/init can never regress again. */
static int check_root_layout(const char *root) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/sbin/init", root);
    if (checked_init_path_at(root, path)) return -1;
    snprintf(path, sizeof(path), "%s/etc/alpine-release", root);
    if (checked_path(path, 0, 0, 1)) return -1;
    snprintf(path, sizeof(path), "%s/old-root", root);
    if (checked_path(path, 1, 0, 0)) return -1;
    snprintf(path, sizeof(path), "%s/boot/gleworks-control", root);
    if (checked_path(path, 1, 0, 0)) return -1;
    return 0;
}

#endif
