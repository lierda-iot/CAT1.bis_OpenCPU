#ifndef __MEM_MAP_CHK_H__
#define __MEM_MAP_CHK_H__


#if PKGFLXFS_IMG_SIZE < 8192
#error "PKGFLXFS_IMG_SIZE must be larger than 8K, please check mem_map.h"
#endif

#if PKGFLXFS_IMG_END != (CONFIG_APP_DEFAULT_FS_END_ADDR)
#error "PKGFLXFS_IMG_END does not match CONFIG_APP_DEFAULT_FS_END_ADDR, please check mem_map.h"
#endif

#if CONFIG_MULTIAPP_SLOTX_FLASH_SIZE + PKGFLXAPP_APP0_SIZE > CONFIG_APP_BURN_SIZE
#error "SLOTX and APP0's FALSH have overlapping areas, please check mem_map.h"
#endif

#if CONFIG_MULTIAPP_SLOTX_RAM_SIZE + PKGFLXAPP_APP0_RAM_SIZE > CONFIG_APP_RAM_LENGTH_ADDR
#error "SLOTX and APP0's RAM have overlapping areas, please check mem_map.h"
#endif

#endif
