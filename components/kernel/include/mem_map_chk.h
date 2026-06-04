#ifndef __MEM_MAP_CHK_H__
#define __MEM_MAP_CHK_H__


#if PKGFLXFS_IMG_SIZE < 8192
#error "PKGFLXFS_IMG_SIZE must be larger than 8K, please check mem_map.h"
#endif

#if PKGFLXFS_IMG_END != (CONFIG_APP_DEFAULT_FS_END_ADDR)
#error "PKGFLXFS_IMG_END does not match CONFIG_APP_DEFAULT_FS_END_ADDR, please check mem_map.h"
#endif

#endif
