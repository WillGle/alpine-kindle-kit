/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host fixture tests for the signed app installer.
 *
 * Build with -DGW_APPS_TESTING so the installer uses the sandbox roots below
 * and exposes the post-verify seam used by the swap test. */
#define _GNU_SOURCE
#include "gw-apps.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char apps_root[PATH_MAX];
static char data_root[PATH_MAX];
static int failures;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

const char *gw_apps_test_root(void) { return apps_root; }
const char *gw_apps_test_data_root(void) { return data_root; }

/* Destroy the caller's package path after verification.  A hardened install
 * must already be working from its private staged copy. */
/* Grow the source after the installer's lstat() but before its open(). */
void gw_apps_test_before_copy(const char *source) {
    const char *trigger = getenv("GWAP_TEST_GROW_ORIGINAL");
    int fd;
    if (!trigger || strcmp(trigger, "1")) return;
    if (!strstr(source, ".gwapp") || strstr(source, ".sig")) return;
    fd = open(source, O_WRONLY | O_NOFOLLOW);
    if (fd >= 0) {
        (void)ftruncate(fd, 128L * 1024 * 1024 + 1);
        close(fd);
    }
}

void gw_apps_test_after_verify(const char *original, const char *staged) {
    const char *trigger = getenv("GWAP_TEST_CORRUPT_ORIGINAL");
    (void)staged;
    if (trigger && !strcmp(trigger, "1")) {
        int fd = open(original, O_WRONLY | O_TRUNC | O_NOFOLLOW);
        if (fd >= 0) {
            static const char junk[] = "corrupted after verify";
            (void)write(fd, junk, sizeof(junk) - 1);
            close(fd);
        }
    }
}

static int mkpath(const char *path) {
    char copy[PATH_MAX];
    char *p;
    if (snprintf(copy, sizeof(copy), "%s", path) >= (int)sizeof(copy)) return -1;
    for (p = copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(copy, 0700) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(copy, 0700) < 0 && errno != EEXIST) return -1;
    return 0;
}

static int reset_case(const char *base, const char *name) {
    char case_dir[PATH_MAX];
    if (snprintf(case_dir, sizeof(case_dir), "%s/%s", base, name) >= (int)sizeof(case_dir))
        return -1;
    if (mkpath(case_dir) < 0) return -1;
    snprintf(apps_root, sizeof(apps_root), "%s/apps", case_dir);
    snprintf(data_root, sizeof(data_root), "%s/appdata", case_dir);
    return 0;
}

static int file_exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

static int copy_file(const char *source, const char *destination) {
    char buffer[32768];
    int in = open(source, O_RDONLY), out;
    ssize_t n;
    if (in < 0) return -1;
    out = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0) { close(in); return -1; }
    while ((n = read(in, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(out, buffer + written, (size_t)(n - written));
            if (w <= 0) { close(in); close(out); return -1; }
            written += w;
        }
    }
    close(in);
    return close(out);
}

static void expect_failure(const char *base, const char *fixture_dir,
                           const char *name, const char *expected) {
    char package[PATH_MAX], err[192] = "";
    if (reset_case(base, name) < 0) { CHECK(0); return; }
    if (snprintf(package, sizeof(package), "%s/%s.gwapp", fixture_dir, name) >=
        (int)sizeof(package)) { CHECK(0); return; }
    if (gw_apps_install(package, NULL, err, sizeof(err)) == 0) {
        printf("FAIL: %s installed but should have been rejected\n", name);
        failures++;
    } else if (!strstr(err, expected)) {
        printf("FAIL: %s error '%s' does not contain '%s'\n", name, err, expected);
        failures++;
    }
}

int main(int argc, char **argv) {
    const char *fixtures = argc > 1 ? argv[1] : "fixtures";
    const char *base = argc > 2 ? argv[2] : ".";
    char package[PATH_MAX], installed[PATH_MAX], active[PATH_MAX], data[PATH_MAX];
    char err[192] = "";

    /* Oversized numeric components must not wrap around. */
    CHECK(gw_apps_version_compare("99999999999999999999.1",
                                  "99999999999999999999.2") < 0);
    CHECK(gw_apps_version_compare("1.0.0", "1.0.0") == 0);

    if (reset_case(base, "valid") < 0) { printf("FAIL: sandbox\n"); return 1; }
    snprintf(package, sizeof(package), "%s/valid.gwapp", fixtures);
    CHECK(gw_apps_install(package, NULL, err, sizeof(err)) == 0);
    snprintf(installed, sizeof(installed), "%s/valid/1.0.0/payload/hello.sh",
             apps_root);
    CHECK(file_exists(installed));
    snprintf(active, sizeof(active), "%s/valid/active", apps_root);
    {
        char target[PATH_MAX];
        ssize_t n = readlink(active, target, sizeof(target) - 1);
        CHECK(n > 0);
        if (n > 0) {
            target[n] = '\0';
            CHECK(!strcmp(target, "1.0.0"));
        }
    }
    snprintf(data, sizeof(data), "%s/valid", data_root);
    CHECK(file_exists(data));

    /* Swap the original path after verification: the install must still use
     * the verified private copy and succeed. */
    if (reset_case(base, "swapped") < 0) { CHECK(0); }
    CHECK(mkdir(apps_root, 0700) == 0 || errno == EEXIST);
    {
        char source[PATH_MAX], target[PATH_MAX], sig_source[PATH_MAX], sig_target[PATH_MAX];
        snprintf(source, sizeof(source), "%s/swapped.gwapp", fixtures);
        snprintf(target, sizeof(target), "%s/package.gwapp", apps_root);
        snprintf(sig_source, sizeof(sig_source), "%s/swapped.gwapp.sig", fixtures);
        snprintf(sig_target, sizeof(sig_target), "%s/package.gwapp.sig", apps_root);
        CHECK(copy_file(source, target) == 0);
        CHECK(copy_file(sig_source, sig_target) == 0);
        package[0] = '\0';
        snprintf(package, sizeof(package), "%s", target);
    }
    setenv("GWAP_TEST_CORRUPT_ORIGINAL", "1", 1);
    err[0] = '\0';
    CHECK(gw_apps_install(package, NULL, err, sizeof(err)) == 0);
    unsetenv("GWAP_TEST_CORRUPT_ORIGINAL");
    snprintf(installed, sizeof(installed), "%s/swapped/1.0.0/payload/hello.sh",
             apps_root);
    CHECK(file_exists(installed));

    /* A file that grows between lstat() and open() must be rejected by the
     * post-open size check. */
    if (reset_case(base, "grown") < 0) { CHECK(0); }
    snprintf(package, sizeof(package), "%s/grown/grown.gwapp", base);
    {
        char source[PATH_MAX], sig_source[PATH_MAX], sig_target[PATH_MAX];
        snprintf(source, sizeof(source), "%s/valid.gwapp", fixtures);
        snprintf(sig_source, sizeof(sig_source), "%s/valid.gwapp.sig", fixtures);
        snprintf(sig_target, sizeof(sig_target), "%s/grown/grown.gwapp.sig", base);
        CHECK(copy_file(source, package) == 0);
        CHECK(copy_file(sig_source, sig_target) == 0);
    }
    setenv("GWAP_TEST_GROW_ORIGINAL", "1", 1);
    err[0] = '\0';
    CHECK(gw_apps_install(package, NULL, err, sizeof(err)) != 0);
    CHECK(strstr(err, "package-invalid") != NULL);
    unsetenv("GWAP_TEST_GROW_ORIGINAL");

    /* Metadata selection verifies and reads the same staged bytes. */
    {
        gw_app_info_t info;
        char verify_error[96] = "";
        snprintf(package, sizeof(package), "%s/valid.gwapp", fixtures);
        CHECK(gw_apps_package_info_verified(package, NULL, &info,
                                            verify_error,
                                            sizeof(verify_error)) == 0);
        CHECK(!strcmp(info.id, "valid"));
    }

    expect_failure(base, fixtures, "setuid", "rejected");
    expect_failure(base, fixtures, "hardlink", "rejected");
    expect_failure(base, fixtures, "symlink", "rejected");
    expect_failure(base, fixtures, "duplicate", "unsafe-or-incomplete-package");
    expect_failure(base, fixtures, "oversized", "rejected");
    expect_failure(base, fixtures, "gzip-bomb", "rejected");
    expect_failure(base, fixtures, "wrong-key", "signature-invalid");
    expect_failure(base, fixtures, "wrong-profile", "manifest-incompatible");
    expect_failure(base, fixtures, "version-overflow", "manifest-id-or-version-invalid");

    if (failures) {
        printf("App installer fixture tests: %d failures\n", failures);
        return 1;
    }
    printf("App installer fixture tests: PASS\n");
    return 0;
}
