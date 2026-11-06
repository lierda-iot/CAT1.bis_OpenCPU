#include "tirtc_lfs_prefix.h"
#include "littlefs/lfs.h"
extern void tirtc_storage_legacy_geometry(const struct lfs_config *, uint32_t *, uint32_t *);
#define TIRTC_LFS_LEGACY_GEOMETRY tirtc_storage_legacy_geometry
#include "littlefs/lfs.c"
#include "littlefs/lfs_util.c"
