#ifndef LIOT_EXTERNAL_FLASH_FS_H
#define LIOT_EXTERNAL_FLASH_FS_H

#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIOT_EXTFLASH_NAME_MAX 255

#define LIOT_EXTFLASH_SEEK_SET 0
#define LIOT_EXTFLASH_SEEK_CUR 1
#define LIOT_EXTFLASH_SEEK_END 2

#define LIOT_EXTFLASH_TYPE_FILE 0x001
#define LIOT_EXTFLASH_TYPE_DIR  0x002

typedef struct {
    uint32_t base_addr;
    uint32_t total_size;
    uint32_t block_size;
    uint32_t read_size;
    uint32_t prog_size;
} liot_ext_fs_cfg_t;

typedef INT32 LFILE_EXT;

typedef struct {
    INT32 fs_index;
    INT32 _reserved;
} LDIR_EXT;

typedef struct {
    UINT8 d_type;
    INT32 d_info;
    CHAR d_name[256];
} ldirent_ext;

typedef struct {
    UINT8 type;
    UINT32 size;
    CHAR name[LIOT_EXTFLASH_NAME_MAX + 1];
} liot_stat_ext_s;

typedef enum {
    LIOT_EXTFLASH_OK                   = 0,
    LIOT_EXTFLASH_INVALID_PARAMETER    = -1,
    LIOT_EXTFLASH_NO_SPACE             = -2,
    LIOT_EXTFLASH_OPEN_FAIL            = -3,
    LIOT_EXTFLASH_CLOSE_FAIL           = -4,
    LIOT_EXTFLASH_READ_FAIL            = -5,
    LIOT_EXTFLASH_WRITE_FAIL           = -6,
    LIOT_EXTFLASH_SEEK_FAIL            = -7,
    LIOT_EXTFLASH_TELL_FAIL            = -8,
    LIOT_EXTFLASH_TRUNCATE_FAIL        = -9,
    LIOT_EXTFLASH_REMOVE_FAIL          = -10,
    LIOT_EXTFLASH_RENAME_FAIL          = -11,
    LIOT_EXTFLASH_STAT_FAIL            = -12,
    LIOT_EXTFLASH_MKDIR_FAIL           = -13,
    LIOT_EXTFLASH_DIR_OPEN_FAIL        = -14,
    LIOT_EXTFLASH_DIR_CLOSE_FAIL       = -15,
    LIOT_EXTFLASH_DIR_READ_FAIL        = -16,
    LIOT_EXTFLASH_MOUNT_FAIL           = -17,
    LIOT_EXTFLASH_FORMAT_FAIL          = -18,
    LIOT_EXTFLASH_NOT_EXIST            = -19,
    LIOT_EXTFLASH_SIZE_FAIL            = -20,
    LIOT_EXTFLASH_SYNC_FAIL            = -21,
    LIOT_EXTFLASH_ERROR_GENERAL        = -22,
} liot_exflash_errcode_e;

int liot_finit_ext(const liot_ext_fs_cfg_t *cfg);
int liot_fdeinit_ext(void);
int liot_fformat_ext(void);

LFILE_EXT liot_fopen_ext(const char *file_name, const char *mode);
int liot_fclose_ext(LFILE_EXT fd);
int liot_remove_ext(const char *file_name);
int liot_fread_ext(void *buffer, size_t size, size_t num, LFILE_EXT fd);
int liot_fwrite_ext(void *buffer, size_t size, size_t num, LFILE_EXT fd);
int liot_fseek_ext(LFILE_EXT fd, long offset, int origin);
int liot_frewind_ext(LFILE_EXT fd);
int liot_ftell_ext(LFILE_EXT fd);
int liot_fstat_ext(LFILE_EXT fd, liot_stat_ext_s *st);
int liot_stat_ext(const char *file_name, liot_stat_ext_s *st);
int liot_ftruncate_ext(LFILE_EXT fd, UINT32 length);
int liot_fsize_ext(LFILE_EXT fd);
int liot_file_exist_ext(const char *file_path);
int liot_rename_ext(const char *oldpath, const char *newpath);
int liot_fsync_ext(LFILE_EXT fd);

int liot_mkdir_ext(const char *path, UINT32 mode);
LDIR_EXT *liot_opendir_ext(const char *path);
int liot_closedir_ext(LDIR_EXT *pdir);
ldirent_ext *liot_readdir_ext(LDIR_EXT *pdir);

int liot_exflash_free_size_get(void);

#ifdef __cplusplus
}
#endif

#endif
