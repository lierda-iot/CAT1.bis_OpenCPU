#ifndef  __MEM_MAP_H__
#define  __MEM_MAP_H__

/*                       memory map
 *————    ————    |———————————————————————| start = CONFIG_APP_BURN_ADDR
 *  |      |      |                       |
 *  |      |      | secboot check Head    | len = 0x00 or CONFIG_SECBOOT_HEAD_LENGTH
 *  ↓      |      |                       |
 * ota     |      |———————————————————————| start = PKGFLXAPP_FLASH_ADDR
 *  ↑      ↓      |                       |  
 *  |    burn     | flash code region     | len = PKGFLXAPP_FLASH_SIZE
 *  |      ↑      |                       |
 *————     |      |———————————————————————|
 *         |      |                       |
 *         |      | tts lib date          | len =0x00 or CONFIG_APP_TTS_SIZE (when tts configed, value can be set)
 *         |      |                       |
 *        ————    |———————————————————————| end = CONFIG_APP_BURN_ADDR
 *                |                       |
 *                | file system           | len = FLASH_FS_REGION_SIZE
 *                |                       |
 *                |———————————————————————| end = CONFIG_APP_DEFAULT_FS_END_ADDR (value from basePkg, no adjustable)
 * 
 */

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
#define CODE_FALSH_SIZE         (CONFIG_APP_BURN_SIZE - CONFIG_SECBOOT_HEAD_LENGTH - FLASH_TTS_REGION_SIZE + FLASH_REGION_ADJUST)
#define PKGFLXAPP_APP0_LNA      (CODE_FALSH_ADDR)
#define PKGFLXAPP_APP0_SIZE     (CODE_FALSH_SIZE)

/* FLASH TTS region */
#define PKGFLXTTS_RES_LNA    (PKGFLXAPP_APP0_LNA + PKGFLXAPP_APP0_SIZE)
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