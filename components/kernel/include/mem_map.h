#ifndef  __MEM_MAP_H__
#define  __MEM_MAP_H__

/*                       memory map
 *————    ————    |———————————————————————| start = CONFIG_APP_BURN_ADDR
 *  |      |      |                       |
 *  |      |      | secboot check Head    | len = 0x00 or CONFIG_SECBOOT_HEAD_LENGTH
 *  ↓      |      |                       |
 * ota     |      |———————————————————————| start = PKGFLXAPP_APP0_LNA
 *  ↑      |      |                       |
 *  |      |      | APP0 code region      | len = PKGFLXAPP_APP0_SIZE (弹性区，吃掉剩余空间)
 *  |      |      |                       |
 *————     |      |———————————————————————| start = PKGFLXAPP_APPX_LNA
 *         |      | APPX code region      | len = CONFIG_MULTIAPP_SLOTX_FLASH_SIZE (多 APP 时定长，默认 0)
 *         ↓      |———————————————————————|
 *       burn     |                       |
 *         ↑      | tts lib date          | len =0x00 or CONFIG_APP_TTS_SIZE (when tts configed, value can be set)
 *         |      |                       |
 *        ————    |———————————————————————| end = CONFIG_APP_BURN_ADDR
 *                |                       |
 *                | file system           | len = FLASH_FS_REGION_SIZE
 *                |                       |
 *                |———————————————————————| end = CONFIG_APP_DEFAULT_FS_END_ADDR (value from basePkg, no adjustable)
 * 
 */

#ifndef CONFIG_APP_SLOT
#define CONFIG_APP_SLOT                 (0)
#endif

#ifndef CONFIG_MULTIAPP_SLOT_NUM
#define CONFIG_MULTIAPP_SLOT_NUM        (1)
#endif

#ifndef CONFIG_MULTIAPP_SLOTX_FLASH_START
#define CONFIG_MULTIAPP_SLOTX_FLASH_START    (0x00000000)
#endif

#ifndef CONFIG_MULTIAPP_SLOTX_FLASH_SIZE
#define CONFIG_MULTIAPP_SLOTX_FLASH_SIZE     (0x00)
#endif

#ifndef CONFIG_MULTIAPP_SLOTX_RAM_START
#define CONFIG_MULTIAPP_SLOTX_RAM_START      (0x00000000)
#endif

#ifndef CONFIG_MULTIAPP_SLOTX_RAM_SIZE
#define CONFIG_MULTIAPP_SLOTX_RAM_SIZE       (0x00)
#endif

/* Store secure boot authentication public key */
#ifdef CONFIG_SECBOOT_ENABLE
#define CONFIG_SECBOOT_HEAD_LENGTH  (0xA0)
#else
#define CONFIG_SECBOOT_HEAD_LENGTH  (0x00)
#endif

#ifdef CONFIG_TTS_ENABLE
#define FLASH_TTS_REGION_SIZE    (CONFIG_TTS_RESOURCE_SIZE)
#else
#define FLASH_TTS_REGION_SIZE    (0x00)
#endif

#ifdef CONFIG_CODE_REGION_ADJUST
#define FLASH_REGION_ADJUST    (CONFIG_CODE_REGION_ADJUST)
#else
#define FLASH_REGION_ADJUST    (0x00)
#endif

/* RAM region */
#define PKGFLXAPP_RAM_ADDR      (CONFIG_APP_RAM_ORIGIN_ADDR)
#define PKGFLXAPP_RAM_SIZE      (CONFIG_APP_RAM_LENGTH_ADDR)

/* FLASH code region */
#define CODE_FALSH_ADDR         (CONFIG_APP_BURN_ADDR + CONFIG_SECBOOT_HEAD_LENGTH)
#define CODE_FALSH_SIZE         (CONFIG_APP_BURN_SIZE - CONFIG_SECBOOT_HEAD_LENGTH - FLASH_TTS_REGION_SIZE + FLASH_REGION_ADJUST - CONFIG_MULTIAPP_SLOTX_FLASH_SIZE)
#define PKGFLXAPP_APP0_LNA      (CODE_FALSH_ADDR)
#define PKGFLXAPP_APP0_SIZE     (CODE_FALSH_SIZE)
#define PKGFLXAPP_APP0_RAM_ADDR (PKGFLXAPP_RAM_ADDR)
#define PKGFLXAPP_APP0_RAM_SIZE (PKGFLXAPP_RAM_SIZE - CONFIG_MULTIAPP_SLOTX_RAM_SIZE)

/* APPX 定长区，起始地址由 CONFIG_MULTIAPP_SLOTX_FLASH_START/RAM_START 指定 */
#define PKGFLXAPP_APPX_LNA      (CONFIG_MULTIAPP_SLOTX_FLASH_START)
#define PKGFLXAPP_APPX_SIZE     (CONFIG_MULTIAPP_SLOTX_FLASH_SIZE)
#define PKGFLXAPP_APPX_RAM_ADDR (CONFIG_MULTIAPP_SLOTX_RAM_START)
#define PKGFLXAPP_APPX_RAM_SIZE (CONFIG_MULTIAPP_SLOTX_RAM_SIZE)

/* 当前编译 slot 的区域（app.ld 使用） */
#if (CONFIG_APP_SLOT != 0)
#define PKGFLXAPP_CUR_LNA       (PKGFLXAPP_APPX_LNA)
#define PKGFLXAPP_CUR_SIZE      (PKGFLXAPP_APPX_SIZE)
#define PKGFLXAPP_CUR_RAM_ADDR  (PKGFLXAPP_APPX_RAM_ADDR)
#define PKGFLXAPP_CUR_RAM_SIZE  (PKGFLXAPP_APPX_RAM_SIZE)
#define PKGFLXAPP_FLASH_END     (PKGFLXAPP_APPX_LNA + PKGFLXAPP_APPX_SIZE)
#else
#define PKGFLXAPP_CUR_LNA       (PKGFLXAPP_APP0_LNA)
#define PKGFLXAPP_CUR_SIZE      (PKGFLXAPP_APP0_SIZE)
#define PKGFLXAPP_CUR_RAM_ADDR  (PKGFLXAPP_APP0_RAM_ADDR)
#define PKGFLXAPP_CUR_RAM_SIZE  (PKGFLXAPP_APP0_RAM_SIZE)
#define PKGFLXAPP_FLASH_END     (PKGFLXAPP_APP0_LNA + PKGFLXAPP_APP0_SIZE)
#endif

/* FLASH TTS region */
#define PKGFLXTTS_RES_LNA    (PKGFLXAPP_FLASH_END)
#define PKGFLXTTS_RES_ADDR   (PKGFLXTTS_RES_LNA - 0x800000)
#define PKGFLXTTS_RES_SIZE   (FLASH_TTS_REGION_SIZE)
#define PKGFLXTTS_RES_END    (PKGFLXTTS_RES_LNA + PKGFLXTTS_RES_SIZE)

/* FLASH FS region */
#define PKGFLXFS_IMG_LNA            (PKGFLXTTS_RES_END)
#define PKGFLXFS_IMG_ADDR           (PKGFLXFS_IMG_LNA - 0x800000)
#define PKGFLXFS_IMG_END            (CONFIG_APP_DEFAULT_FS_END_ADDR)
#define PKGFLXFS_IMG_SIZE           (PKGFLXFS_IMG_END - PKGFLXFS_IMG_ADDR)

#include "mem_map_chk.h"

#endif