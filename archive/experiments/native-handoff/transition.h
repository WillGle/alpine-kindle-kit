/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kexec.h>

struct gw_handoff_callbacks {
    void (*restart_prepare)(char *);
    void (*machine_shutdown)(void);
    void (*setup_mm)(char);
    void (*flush_inner)(void);
    void (*finish_cpu)(void);
};

/* Only prepares the owned control page; no shutdown or hardware transition. */
int gw_prepare_handoff(struct kimage *image, unsigned long dtb,
                       unsigned long dtb_size);

/* Host compiled/review candidate, intentionally not linked into any module. */
void gw_enter_handoff(struct kimage *image,
                      const struct gw_handoff_callbacks *callbacks)
    __attribute__((noreturn));
