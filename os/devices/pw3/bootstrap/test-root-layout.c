/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host regression test for the Alpine root-layout check used by the PW3
 * rescue bootstrap.  Reproduces the symlinked /sbin/init layout that a build
 * produces, plus the failure modes the check must still reject.
 */
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "root-layout.h"

static int checks, failures;

static void expect(const char *name, int got, int want) {
    checks++;
    if (got != want) {
        failures++;
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
    }
}

static void join_path(char *out, size_t size, const char *root, const char *rel) {
    int n = snprintf(out, size, "%s/%s", root, rel);
    if (n < 0 || (size_t)n >= size) { fprintf(stderr, "path too long\n"); exit(2); }
}

static void mkdirs(const char *path) {
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *q = buf + 1; *q; q++) {
        if (*q != '/') continue;
        *q = 0;
        if (mkdir(buf, 0755) && errno != EEXIST) { perror(buf); exit(2); }
        *q = '/';
    }
    if (mkdir(buf, 0755) && errno != EEXIST) { perror(buf); exit(2); }
}

static void write_file(const char *path, mode_t mode) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0 || write(fd, "x", 1) != 1) { perror(path); exit(2); }
    close(fd);
    if (chmod(path, mode)) { perror(path); exit(2); }
}

static void make_tree(const char *root) {
    char path[PATH_MAX];
    join_path(path, sizeof(path), root, "sbin"); mkdirs(path);
    join_path(path, sizeof(path), root, "bin"); mkdirs(path);
    join_path(path, sizeof(path), root, "etc"); mkdirs(path);
    join_path(path, sizeof(path), root, "old-root"); mkdirs(path);
    join_path(path, sizeof(path), root, "boot/gleworks-control"); mkdirs(path);
    join_path(path, sizeof(path), root, "bin/busybox"); write_file(path, 0755);
    join_path(path, sizeof(path), root, "etc/alpine-release"); write_file(path, 0644);
    join_path(path, sizeof(path), root, "sbin/init");
    if (symlink("../bin/busybox", path)) { perror(path); exit(2); }
}

int main(void) {
    char base[] = "/tmp/gw-root-layout-XXXXXX";
    if (!mkdtemp(base)) { perror("mkdtemp"); return 2; }

    char root[PATH_MAX];
    join_path(root, sizeof(root), base, "good");
    make_tree(root);
    expect("normal symlinked init", check_root_layout(root), 0);

    join_path(root, sizeof(root), base, "noexec");
    make_tree(root);
    {
        char path[PATH_MAX];
        join_path(path, sizeof(path), root, "bin/busybox");
        if (chmod(path, 0644)) { perror(path); return 2; }
    }
    expect("init target not executable", check_root_layout(root), -1);

    join_path(root, sizeof(root), base, "escape");
    make_tree(root);
    {
        char path[PATH_MAX];
        join_path(path, sizeof(path), root, "sbin/init");
        unlink(path);
        if (symlink("/bin/sh", path)) { perror(path); return 2; }
    }
    expect("init symlink escapes root", check_root_layout(root), -1);

    join_path(root, sizeof(root), base, "noold");
    make_tree(root);
    {
        char path[PATH_MAX];
        join_path(path, sizeof(path), root, "old-root");
        if (rmdir(path)) { perror(path); return 2; }
    }
    expect("missing /old-root", check_root_layout(root), -1);

    join_path(root, sizeof(root), base, "plaininit");
    make_tree(root);
    {
        char path[PATH_MAX];
        join_path(path, sizeof(path), root, "sbin/init");
        unlink(path);
        write_file(path, 0755);
    }
    expect("regular init file", check_root_layout(root), 0);

    printf("root layout checks: %d total, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
