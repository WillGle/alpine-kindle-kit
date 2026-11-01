/* SPDX-License-Identifier: GPL-3.0-or-later
 * Detached-signature verification for app bundles.
 *
 * The PW3 image already carries OpenSSL's libcrypto (used by apk), but no
 * OpenSSL headers or CLI.  We therefore declare the small EVP surface we need
 * and load libcrypto at runtime, which keeps the build dependency-free and
 * makes a missing library a clean verification failure instead of a crash.
 *
 * Signature format: raw RSA PKCS#1 v1.5 over SHA-256 of the whole package
 * file, exactly as produced by
 *   openssl dgst -sha256 -sign release.key -out package.gwapp.sig package.gwapp
 * The trust root is a PEM public key installed at GW_APP_KEY_PATH.
 *
 * STATUS: CORE (in use).
 */
#ifndef GW_SIGNATURE_H
#define GW_SIGNATURE_H

#include <stddef.h>

#define GW_APP_KEY_PATH "/etc/gleworks/app-keys/release.pem"

/*
 * Returns 0 when the signature is valid.
 *        -1 when the signature is invalid, the key is unreadable, or I/O
 *            fails.
 *        -2 when libcrypto is not available on this image.
 * On failure, err (if non-NULL) receives a short diagnostic.
 */
int gw_signature_verify_file(const char *path, const char *signature_path,
                             const char *public_key_path,
                             char *err, size_t err_size);

#endif /* GW_SIGNATURE_H */
