/* SPDX-License-Identifier: GPL-3.0-or-later
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include "gw-signature.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Opaque OpenSSL types; we only pass pointers through. */
typedef struct bio_st BIO;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct evp_md_st EVP_MD;
typedef struct engine_st ENGINE;

typedef struct {
    void *lib;
    int (*EVP_Digest)(const void *, size_t, unsigned char *, unsigned int *,
                      const EVP_MD *, ENGINE *);
    BIO *(*BIO_new_mem_buf)(const void *, int);
    void (*BIO_free)(BIO *);
    EVP_PKEY *(*PEM_read_bio_PUBKEY)(BIO *, EVP_PKEY **, void **, void *);
    EVP_MD_CTX *(*EVP_MD_CTX_new)(void);
    void (*EVP_MD_CTX_free)(EVP_MD_CTX *);
    const EVP_MD *(*EVP_sha256)(void);
    int (*EVP_DigestVerifyInit)(EVP_MD_CTX *, void **, const EVP_MD *,
                                ENGINE *, EVP_PKEY *);
    int (*EVP_DigestVerifyUpdate)(EVP_MD_CTX *, const void *, size_t);
    int (*EVP_DigestVerifyFinal)(EVP_MD_CTX *, const unsigned char *, size_t);
    void (*EVP_PKEY_free)(EVP_PKEY *);
} gw_crypto_t;

static void set_error(char *err, size_t size, const char *message) {
    if (err && size) snprintf(err, size, "%s", message);
}

static int crypto_load(gw_crypto_t *c) {
    static const char *const names[] = {
        GW_CRYPTO_LIB,
        "/usr/lib/libcrypto.so.3",
        "/lib/libcrypto.so.3",
        "/usr/lib64/libcrypto.so.3",
#ifdef GW_CRYPTO_ALLOW_SEARCH
        "libcrypto.so.3",
        "libcrypto.so",
#endif
        NULL
    };
    int i;
    memset(c, 0, sizeof(*c));
    for (i = 0; names[i]; i++) {
        c->lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (c->lib) break;
    }
    if (!c->lib) return -1;
#define LOAD(name, sym) do { *(void **)(&c->name) = dlsym(c->lib, sym); \
    if (!c->name) { dlclose(c->lib); c->lib = NULL; return -1; } } while (0)
    LOAD(EVP_Digest, "EVP_Digest");
    LOAD(BIO_new_mem_buf, "BIO_new_mem_buf");
    LOAD(BIO_free, "BIO_free");
    LOAD(PEM_read_bio_PUBKEY, "PEM_read_bio_PUBKEY");
    LOAD(EVP_MD_CTX_new, "EVP_MD_CTX_new");
    LOAD(EVP_MD_CTX_free, "EVP_MD_CTX_free");
    LOAD(EVP_sha256, "EVP_sha256");
    LOAD(EVP_DigestVerifyInit, "EVP_DigestVerifyInit");
    LOAD(EVP_DigestVerifyUpdate, "EVP_DigestVerifyUpdate");
    LOAD(EVP_DigestVerifyFinal, "EVP_DigestVerifyFinal");
    LOAD(EVP_PKEY_free, "EVP_PKEY_free");
#undef LOAD
    return 0;
}

/* Read a bounded regular file, refusing symlinks.  Returns a heap buffer. */
static char *read_regular_file(const char *path, size_t limit, size_t *length,
                               int require_trusted_owner,
                               char *err, size_t err_size) {
    int fd;
    struct stat st;
    char *buffer;
    size_t used = 0;
    if (length) *length = 0;
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        set_error(err, err_size, "file-unreadable");
        return NULL;
    }
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        (size_t)st.st_size > limit) {
        set_error(err, err_size, "file-invalid");
        close(fd);
        return NULL;
    }
    if (require_trusted_owner) {
        if ((geteuid() == 0 && st.st_uid != 0) || (st.st_mode & 022)) {
            set_error(err, err_size, "trust-key-permissions");
            close(fd);
            return NULL;
        }
    }
    buffer = malloc((size_t)st.st_size + 1);
    if (!buffer) {
        set_error(err, err_size, "out-of-memory");
        close(fd);
        return NULL;
    }
    while (used < (size_t)st.st_size) {
        ssize_t n = read(fd, buffer + used, (size_t)st.st_size - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        used += (size_t)n;
    }
    close(fd);
    if (used != (size_t)st.st_size) {
        set_error(err, err_size, "file-read-error");
        free(buffer);
        return NULL;
    }
    buffer[used] = '\0';
    if (length) *length = used;
    return buffer;
}

static int key_fingerprint_ok(const char *pem, size_t length, gw_crypto_t *c) {
    unsigned char digest[32];
    unsigned int digest_length = 0;
    char hex[65];
    size_t i;
    if (!GW_APP_KEY_SHA256[0]) return 1;
    if (c->EVP_Digest(pem, length, digest, &digest_length, c->EVP_sha256(),
                      NULL) != 1 || digest_length != sizeof(digest))
        return 0;
    for (i = 0; i < sizeof(digest); i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    hex[64] = '\0';
    return strcmp(hex, GW_APP_KEY_SHA256) == 0;
}

int gw_signature_verify_file(const char *path, const char *signature_path,
                             const char *public_key_path,
                             char *err, size_t err_size) {
    gw_crypto_t c;
    BIO *key_bio = NULL;
    EVP_PKEY *key = NULL;
    EVP_MD_CTX *ctx = NULL;
    char *pem = NULL;
    unsigned char *signature = NULL;
    size_t pem_len = 0, signature_len = 0;
    int package_fd = -1;
    unsigned char buffer[32768];
    ssize_t n;
    int result = -1;

    if (!path || !signature_path || !public_key_path) {
        set_error(err, err_size, "invalid-argument");
        return -1;
    }
    if (crypto_load(&c) < 0) {
        set_error(err, err_size, "libcrypto-unavailable");
        return -2;
    }
    pem = read_regular_file(public_key_path, 16384, &pem_len, 1, err, err_size);
    if (!pem) goto done;
    if (!key_fingerprint_ok(pem, pem_len, &c)) {
        set_error(err, err_size, "trust-key-fingerprint-mismatch");
        goto done;
    }
    signature = (unsigned char *)read_regular_file(signature_path, 4096,
                                                  &signature_len, 0, err,
                                                  err_size);
    if (!signature) goto done;

    key_bio = c.BIO_new_mem_buf(pem, (int)pem_len);
    if (!key_bio) { set_error(err, err_size, "trust-key-invalid"); goto done; }
    key = c.PEM_read_bio_PUBKEY(key_bio, NULL, NULL, NULL);
    if (!key) { set_error(err, err_size, "trust-key-invalid"); goto done; }
    ctx = c.EVP_MD_CTX_new();
    if (!ctx) { set_error(err, err_size, "crypto-context"); goto done; }
    if (c.EVP_DigestVerifyInit(ctx, NULL, c.EVP_sha256(), NULL, key) != 1) {
        set_error(err, err_size, "verify-init");
        goto done;
    }
    package_fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (package_fd < 0) { set_error(err, err_size, "package-unreadable"); goto done; }
    {
        struct stat st;
        if (fstat(package_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
            set_error(err, err_size, "package-invalid");
            goto done;
        }
    }
    while ((n = read(package_fd, buffer, sizeof(buffer))) > 0) {
        if (c.EVP_DigestVerifyUpdate(ctx, buffer, (size_t)n) != 1) {
            set_error(err, err_size, "verify-update");
            goto done;
        }
    }
    if (n < 0) { set_error(err, err_size, "package-read-error"); goto done; }
    {
        int ok = c.EVP_DigestVerifyFinal(ctx, signature, signature_len);
        if (ok == 1) result = 0;
        else { set_error(err, err_size, ok == 0 ? "signature-invalid" : "verify-failed"); }
    }

done:
    if (package_fd >= 0) close(package_fd);
    if (ctx) c.EVP_MD_CTX_free(ctx);
    if (key) c.EVP_PKEY_free(key);
    if (key_bio) c.BIO_free(key_bio);
    free(signature);
    free(pem);
    if (c.lib) dlclose(c.lib);
    return result;
}
