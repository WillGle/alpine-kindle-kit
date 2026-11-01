/* SPDX-License-Identifier: GPL-3.0-or-later
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include "gw-signature.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Opaque OpenSSL types; we only pass pointers through. */
typedef struct bio_st BIO;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct evp_md_st EVP_MD;
typedef struct engine_st ENGINE;

typedef struct {
    void *lib;
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
    static const char *const names[] = { "libcrypto.so.3", "libcrypto.so", NULL };
    int i;
    memset(c, 0, sizeof(*c));
    for (i = 0; names[i]; i++) {
        c->lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (c->lib) break;
    }
    if (!c->lib) return -1;
#define LOAD(name, sym) do { *(void **)(&c->name) = dlsym(c->lib, sym); \
    if (!c->name) { dlclose(c->lib); c->lib = NULL; return -1; } } while (0)
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

/* Read a small file into a NUL-terminated heap buffer. */
static char *read_file(const char *path, size_t limit, size_t *length) {
    FILE *file;
    char *buffer;
    size_t used = 0, n;
    if (length) *length = 0;
    file = fopen(path, "rb");
    if (!file) return NULL;
    buffer = malloc(limit + 1);
    if (!buffer) { fclose(file); return NULL; }
    while (used < limit && (n = fread(buffer + used, 1, limit - used, file)) > 0)
        used += n;
    if (ferror(file) || used == 0 || used >= limit) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[used] = '\0';
    fclose(file);
    if (length) *length = used;
    return buffer;
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
    FILE *file = NULL;
    unsigned char buffer[32768];
    size_t n;
    int result = -1;

    if (!path || !signature_path || !public_key_path) {
        set_error(err, err_size, "invalid-argument");
        return -1;
    }
    if (crypto_load(&c) < 0) {
        set_error(err, err_size, "libcrypto-unavailable");
        return -2;
    }
    pem = read_file(public_key_path, 16384, &pem_len);
    if (!pem) { set_error(err, err_size, "trust-key-unreadable"); goto done; }
    signature = (unsigned char *)read_file(signature_path, 4096, &signature_len);
    if (!signature) { set_error(err, err_size, "signature-unreadable"); goto done; }

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
    file = fopen(path, "rb");
    if (!file) { set_error(err, err_size, "package-unreadable"); goto done; }
    while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (c.EVP_DigestVerifyUpdate(ctx, buffer, n) != 1) {
            set_error(err, err_size, "verify-update");
            goto done;
        }
    }
    if (ferror(file)) { set_error(err, err_size, "package-read-error"); goto done; }
    {
        int ok = c.EVP_DigestVerifyFinal(ctx, signature, signature_len);
        if (ok == 1) result = 0;
        else { set_error(err, err_size, ok == 0 ? "signature-invalid" : "verify-failed"); }
    }

done:
    if (file) fclose(file);
    if (ctx) c.EVP_MD_CTX_free(ctx);
    if (key) c.EVP_PKEY_free(key);
    if (key_bio) c.BIO_free(key_bio);
    free(signature);
    free(pem);
    if (c.lib) dlclose(c.lib);
    return result;
}
