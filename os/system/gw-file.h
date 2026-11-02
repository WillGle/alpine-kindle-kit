/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared atomic file replacement for settings and shell state.
 *
 * The temporary file is created with O_CREAT|O_EXCL|O_NOFOLLOW, fsynced, then
 * renamed over the target and the parent directory is fsynced.  The only
 * writes in the repository that need this weaker, multi-writer form should
 * use this helper. */
#ifndef GW_FILE_H
#define GW_FILE_H

#include <stddef.h>
#include <sys/types.h>

/* Replace path with len bytes from data.  Parent directories are created.
 * Returns 0 on success, -1 on failure (errno is preserved). */
int gw_write_atomic(const char *path, const void *data, size_t len, mode_t mode);

/* fsync a directory so a preceding rename is durable. */
int gw_fsync_dir(const char *path);

#endif /* GW_FILE_H */
