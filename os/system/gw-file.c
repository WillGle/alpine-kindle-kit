/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "gw-file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int gw_fsync_dir(const char *path) {
    char directory[512];
    char *slash;
    int fd, result;
    if (!path || snprintf(directory, sizeof(directory), "%s", path) >=
                 (int)sizeof(directory))
        return -1;
    slash = strrchr(directory, '/');
    if (!slash) {
        strcpy(directory, ".");
    } else if (slash == directory) {
        directory[1] = '\0';
    } else {
        *slash = '\0';
    }
    fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    result = fsync(fd);
    close(fd);
    return result;
}

int gw_write_atomic(const char *path, const void *data, size_t len, mode_t mode) {
    char tmp[256], parent[256];
    char *slash;
    static unsigned long sequence;
    const unsigned char *cursor = data;
    size_t remaining = len;
    int fd = -1, attempt;

    if (!path || (!data && len)) { errno = EINVAL; return -1; }
    if (snprintf(parent, sizeof(parent), "%s", path) >= (int)sizeof(parent)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        if (mkdir(parent, 0700) < 0 && errno != EEXIST) return -1;
    }
    for (attempt = 0; attempt < 8; attempt++) {
        if (snprintf(tmp, sizeof(tmp), "%s.tmp.%ld.%lu", path,
                     (long)getpid(), sequence++) >= (int)sizeof(tmp)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                  mode);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) return -1;
    while (remaining) {
        ssize_t n = write(fd, cursor, remaining);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) goto fail;
        cursor += n;
        remaining -= (size_t)n;
    }
    if (fsync(fd) < 0) goto fail;
    if (close(fd) < 0) { fd = -1; goto fail; }
    fd = -1;
    if (rename(tmp, path) != 0) goto fail;
    return gw_fsync_dir(path);
fail:
    {
        int saved = errno;
        if (fd >= 0) close(fd);
        unlink(tmp);
        errno = saved;
    }
    return -1;
}
