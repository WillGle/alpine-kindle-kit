/* GPL-2.0-only: ARM32 private experiment ABI; no execute command. */
#define GW_LOAD 0x4701
#define GW_UNLOAD 0x4702
#define GW_VERIFY 0x4703
#define GW_BUDGET 0x4704
#define GW_LIMIT (3U * 1024U * 1024U)
#define GW_DEST 0x80008000U
struct gw_request {
    unsigned int entry, count, segments, flags;
};
struct gw_segment {
    unsigned int buf, bufsz, mem, memsz;
};
struct gw_status {
    unsigned int bytes, checksum, owned_pages, peak_pages;
};

#define GW_PREPARE 0x4705 /* Fixed image only, returning RAM preparation. */
