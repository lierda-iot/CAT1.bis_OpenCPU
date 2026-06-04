/**
 * @File Name: liot_fs_demo.c
 * @brief
 * @Author : Lxh email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#include <stdio.h>
#include <string.h>

#include "liot_fs_api.h"
#include "liot_nv.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_log.h"

/*
   This project demo trates the use of the file system.
   In this example, we will create a file, write data to the file, save the file.
   Then open the file, and read the data after offset 2 bytes from the file header.
 */

#define DATA_LENGTH      11
#define FSEEK_LENGTH     2
#define TRUNCATE_LENGTH  6
#define DEMO_FILE        "demo.txt"
#define DEMO_FILE_RENAME "rename.txt"

#define DIR_NAME     "/testDir"
#define DIR_NAME_111 "/testDir111"
#define DIR_FILE     "/testDir/dir.txt"

#define LIOT_NV_DEMO_CFG "liot_nv_demo_cfg.nvm"

typedef struct _nv_demo
{
    int parameter_1;
    char parameter_2[10];
} nv_demo;

void liot_fs_demo_task(void *argv)
{
    liot_stat_s fstat_st;
    liot_stat_s stat_st;
    LDIR *dir_test      = NULL;
    ldirent *dir_info   = NULL;
    int ret             = -1;
    LFILE fd            = -1;

    char *read_buff     = liot_rtos_malloc(DATA_LENGTH - FSEEK_LENGTH + 1);
    char *frewind_buff  = liot_rtos_malloc(DATA_LENGTH + 1);
    char *truncate_buff = liot_rtos_malloc((DATA_LENGTH - TRUNCATE_LENGTH) + 1);
    char *dir_buff      = liot_rtos_malloc(DATA_LENGTH + 1);

    memset(&fstat_st, 0, sizeof(liot_stat_s));
    memset(&stat_st, 0, sizeof(liot_stat_s));

    liot_rtos_task_sleep_ms(2000);
    liot_trace("========== fs demo start ==========");

    liot_trace("Internal FS free size: %d", liot_internal_fs_free_size_get());
    ret = liot_file_exist(DEMO_FILE);
    liot_trace("=== before create, check file exist. ret(%d) ===", ret);
    if (ret == 0)
    {
        liot_trace("=== remove file. ret(%d) ===", liot_remove(DEMO_FILE));
        liot_trace("=== remove file, check file exist. ret(%d) ===", liot_file_exist(DEMO_FILE));
    }

    liot_trace("=== creat file ===\r\n");
    fd = liot_fopen(DEMO_FILE, "wb+");
    if (fd < LIOT_FS_OK)
    {
        liot_trace("=== create file fail. fd(%d) ===", fd);
    }

    liot_trace("=== write data to file ====");
    ret = liot_fwrite("Hello World", 1, DATA_LENGTH, fd);
    if (ret != DATA_LENGTH)
    {
        liot_trace("=== write data fail. ret(%d) ===", ret);
    }

    liot_trace("=== sync file data ====");
    ret = liot_fsync(fd);
    if (ret != LIOT_FS_OK)
    {
        liot_trace("=== sync file data fail. ret(%d) ===", ret);
    }

    liot_trace("=== write data to file again ====");
    ret = liot_fwrite("Hello Lierda", 1, DATA_LENGTH + 1, fd);
    if (ret != DATA_LENGTH + 1)
    {
        liot_trace("=== write data fail. ret(%d) ===", ret);
    }

    liot_trace("=== close file ===");
    ret = liot_fclose(fd);
    if (ret != LIOT_FS_OK)
    {
        liot_trace("=== close file fail. ret(%d) ===", ret);
    }

    liot_rtos_task_sleep_ms(1000);

    liot_trace("=== open file ===");
    fd = liot_fopen(DEMO_FILE, "r");
    if (fd < LIOT_FS_OK)
    {
        liot_trace("=== open file fail. fd(%d) ===", fd);
    }

    liot_trace("=== file size, ret(%d) ===", liot_fsize(fd));

    liot_fseek(fd, FSEEK_LENGTH, LIOT_SEEK_SET);

    liot_trace("=== read data from file ====");
    ret = liot_fread(read_buff, DATA_LENGTH - FSEEK_LENGTH, 1, fd);
    if (ret != (DATA_LENGTH - FSEEK_LENGTH))
    {
        liot_trace("=== read data fail. ret(%d) ===", ret);
    }
    else
    {
        liot_trace("=== read data success(%s) ===", read_buff);
    }

    ret = liot_ftell(fd);
    if (ret > 0)
    {
        liot_trace("=== liot_ftell, ret(%d) ===", ret);
        liot_trace("=== liot_frewind, ret(%d) ===", liot_frewind(fd));
        ret = liot_fread(frewind_buff, DATA_LENGTH, 1, fd);
        if (ret != DATA_LENGTH)
        {
            liot_trace("=== read data fail. ret(%d) ===", ret);
        }
        else
        {
            liot_trace("=== after rewind file. data(%s) ===", frewind_buff);
        }
    }

    liot_trace("=== close file. ret(%d) ===", liot_fclose(fd));

    // test
    liot_trace("=== open test:%d ", liot_fopen("test1", "wb+"));
    liot_trace("=== open:%d ", liot_fopen("test:test2", "wb+"));

    liot_trace("=== after create, check file exist. ret(%d) ===", liot_file_exist(DEMO_FILE));

    liot_trace("=== rename file. ret(%d) ===", liot_rename(DEMO_FILE, DEMO_FILE_RENAME));

    liot_trace("=== after rename, check file exist. ret(%d) ===", liot_file_exist(DEMO_FILE_RENAME));

    fd = liot_fopen(DEMO_FILE_RENAME, "r+");
    if (fd < LIOT_FS_OK)
    {
        liot_trace("=== open file fail. fd(%d) ===", fd);
    }

    liot_trace("=== truncate file. ret(%d) ===", liot_ftruncate(fd, TRUNCATE_LENGTH));

    liot_trace(
        "=== liot_fstat. ret(%d) name(%s) size(%d) ===", liot_fstat(fd, &fstat_st), fstat_st.name, fstat_st.size);

    liot_trace("=== liot_stat. ret(%d) name(%s) size(%d) ===",
            liot_stat(DEMO_FILE_RENAME, &stat_st),
            stat_st.name,
            stat_st.size);

    liot_trace("=== close rename file. ret(%d) ===", liot_fclose(fd));

    liot_trace("=== remove rename file. ret(%d) ===", liot_remove(DEMO_FILE_RENAME));

    liot_trace("====== Dir demo ======");

    liot_trace("=== makdir. ret(%d) ===", liot_mkdir(DIR_NAME, 1));

    fd = liot_fopen(DIR_FILE, "wb+");
    liot_trace("=== create file in dir. fd(%d) ===", fd);

    dir_test = liot_opendir(DIR_NAME);
    liot_trace("=== opendir. fs_index(%d) ===", dir_test->fs_index);

    ret = liot_file_exist(DIR_FILE);
    liot_trace("=== after makdir, check dir file exist. ret(%d) ===", ret);

    if (ret == 0)
    {
        if (fd < LIOT_FS_OK)
        {
            liot_trace("=== open dir file fail. fd(%d) ===", fd);
        }

        liot_trace("=== write data to dir file ====");
        ret = liot_fwrite("dirFileTest", 1, strlen("dirFileTest"), fd);

        ret = liot_fclose(fd);
        if (ret != LIOT_FS_OK)
        {
            liot_trace("=== close dir file fail. ret(%d) ===", ret);
        }

        liot_rtos_task_sleep_ms(500);

        fd = liot_fopen(DIR_FILE, "r");
        if (fd < LIOT_FS_OK)
        {
            liot_trace("=== open dir file fail. fd(%d) ===", fd);
        }

        liot_trace("=== read data from file ====");
        ret = liot_fread(dir_buff, DATA_LENGTH, 1, fd);

        if (ret != DATA_LENGTH)
        {
            liot_trace("=== read dir file data fail. ret(%d) ===", ret);
        }
        else
        {
            liot_trace("=== read dir file data success(%s) ===", dir_buff);
        }

        liot_trace("=== close dir file. ret(%d) ===", liot_fclose(fd));

        liot_rtos_task_sleep_ms(500);
    }

    liot_trace("=== liot_closedir. ret(%d) ===", liot_closedir(dir_test));

    dir_test = liot_opendir(DIR_NAME);
    liot_trace("=== opendir. fs_index(%d) ===", dir_test->fs_index);
    while((dir_info = liot_readdir(dir_test)) != NULL)
    {
        if(dir_info->d_type != 1)
        {
            continue;
        }   
        liot_trace("=== read dir success. name(%s) ===", dir_info->d_name);
    }


    liot_trace("=== remove dir file. ret(%d) ===", liot_remove(DIR_FILE));

    liot_trace("=== liot_closedir. ret(%d) ===", liot_closedir(dir_test));

    liot_trace("=== remove dir. ret(%d) ===", liot_remove(DIR_NAME));
    liot_trace("========== fs demo end ==========");

    liot_rtos_free(read_buff);
    liot_rtos_free(frewind_buff);
    liot_rtos_free(truncate_buff);
    liot_rtos_free(dir_buff);

    nv_demo demo   = {10, "nv_demo"};
    nv_demo demo_r = {0};

    liot_trace("liot_nv_demo");

    ret = liot_nvm_fwrite(LIOT_NV_DEMO_CFG, (void *)&demo, sizeof(demo), 1);
    if (ret != sizeof(demo))
    {
        liot_trace("========== nv write fail ==========");
    }
    ret = liot_nvm_fread(LIOT_NV_DEMO_CFG, (void *)&demo_r, sizeof(demo_r), 1);
    if (ret != sizeof(demo_r))
    {
        liot_trace("========== nv read fail ==========");
    }
    if (demo_r.parameter_1 == 10)
    {
        liot_trace("parameter_1=%d,parameter_2=%s", demo_r.parameter_1, demo_r.parameter_2);
    }
    ret = liot_cust_nvm_fwrite((void *)&demo, sizeof(demo), 1);
    if (ret != sizeof(demo))
    {
        liot_trace("========== nv cust write fail ==========");
    }
    memset((void *)&demo_r, 0x00, sizeof(demo_r));
    ret = liot_cust_nvm_fread((void *)&demo_r, sizeof(demo_r), 1);
    if (ret != sizeof(demo_r))
    {
        liot_trace("========== nv cust read fail ==========");
    }

    if (demo_r.parameter_1 == 10)
    {
        liot_trace("parameter_1=%d,parameter_2=%s", demo_r.parameter_1, demo_r.parameter_2);
    }

    liot_rtos_task_sleep_ms(1000);

    liot_rtos_task_delete(NULL); // kill itself
}
