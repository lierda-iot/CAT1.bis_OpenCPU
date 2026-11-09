/**
 * @file demo_extflash_p25q64_mgr.h
 * @brief Unified app-layer lifecycle manager for the onboard P25Q64 extflash.
 *
 * The vendor flash driver (liot_flash_init_ext) is not idempotent: when several
 * demo tasks call it concurrently (raw + lfs + lvgl photo lfs), the second call
 * fails while creating the vendor SPI mutex and the demo task resets. This
 * manager serializes the first init, then keeps a reference count so every
 * consumer can init/deinit independently while the hardware is only brought up
 * once and torn down when the last owner releases it.
 */

#ifndef DEMO_EXTFLASH_P25Q64_MGR_H
#define DEMO_EXTFLASH_P25Q64_MGR_H

#include <stdbool.h>
#include <stdint.h>

#include "liot_external_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Owner tag helpers. Each consumer passes the same literal every time so the
 * refcount pair (init/deinit) is symmetric. The tag is only used for logs.
 */
#define DEMO_EXTFLASH_MGR_OWNER_RAW      "p25q64_raw"
#define DEMO_EXTFLASH_MGR_OWNER_LFS       "p25q64_lfs"
#define DEMO_EXTFLASH_MGR_OWNER_PHOTO_LFS "lvgl_photo_lfs"

/*
 * Bring up the shared extflash once and take one reference.
 *
 * The first caller performs the full power/SPI pinmux sequence plus
 * liot_flash_init_ext. Later callers only bump the refcount. The configuration
 * is copied, so callers may keep their own static cfg.
 */
int32_t demo_extflash_p25q64_mgr_init(const liot_ext_flash_cfg_t *cfg,
                                      const char *owner);

/*
 * Release one reference. The vendor deinit runs only when the count reaches
 * zero. Power rails and pinmux stay in the state established by the first
 * owner; photo-lfs stays mounted across boots and shares the same GPIO setup.
 */
int32_t demo_extflash_p25q64_mgr_deinit(const char *owner);

/* True once the shared flash has been initialized successfully. */
bool demo_extflash_p25q64_mgr_is_ready(void);

/*
 * Coarse cross-demo access lock. LFS consumers hold it around multi-call FS
 * operations (open/write/read/close) so raw erases and photo-lfs seeding cannot
 * interleave on the same SPI bus.
 */
int32_t demo_extflash_p25q64_mgr_lock(void);
void demo_extflash_p25q64_mgr_unlock(void);

/*
 * Query-only helpers for host-side unit tests. They return the current state
 * without touching the RTOS; in device builds they simply read the globals.
 */
int32_t demo_extflash_p25q64_mgr_refcount_for_test(void);
bool demo_extflash_p25q64_mgr_ready_for_test(void);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_EXTFLASH_P25Q64_MGR_H */
