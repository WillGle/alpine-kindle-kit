/* GPL-2.0-only: fixed nonce-bound RAM-trial guardian interface. */
#define GW_GUARD_STATUS 0x4810
#define GW_GUARD_TEST 0x4811
#define GW_GUARD_ARM 0x4812
#define GW_GUARD_CANCEL 0x4813
struct gw_guard { unsigned int token[4], state, remaining_seconds; };
