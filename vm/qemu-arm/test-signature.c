/* SPDX-License-Identifier: GPL-3.0-or-later
 * ARM/QEMU signature gate: validates the trust-key fingerprint and detached
 * signature path on the target ABI without a device. */
#include "gw-signature.h"
#include <stdio.h>

int main(int argc, char **argv) {
    char err[128] = "";
    char valid_sig[512], wrong_sig[512];
    int valid, wrong;
    if (argc != 4) {
        fprintf(stderr, "usage: %s VALID.gwapp WRONG.gwapp KEY.pem\n", argv[0]);
        return 2;
    }
    snprintf(valid_sig, sizeof(valid_sig), "%s.sig", argv[1]);
    snprintf(wrong_sig, sizeof(wrong_sig), "%s.sig", argv[2]);
    valid = gw_signature_verify_file(argv[1], valid_sig, argv[3], err, sizeof(err));
    printf("valid: rc=%d err=%s\n", valid, err);
    err[0] = '\0';
    wrong = gw_signature_verify_file(argv[2], wrong_sig, argv[3], err, sizeof(err));
    printf("wrong: rc=%d err=%s\n", wrong, err);
    return (valid == 0 && wrong < 0) ? 0 : 1;
}
