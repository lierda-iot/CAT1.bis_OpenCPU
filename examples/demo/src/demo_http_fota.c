/**
 * @File Name: liot_fota_http_demo.c
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 * @note This file implements a firmware upgrade (FOTA) functionality using the Lierda FOTA server for a module.
 *  When the FOTA package download or verification fails, the system will repeatedly attempt to download and verify the package.
 *  If the verification fails, the system will retry the download and verification process.
 *  Once the verification succeeds, the module will be restarted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_dev.h"
#include "liot_fota.h"
#include "liot_fs_api.h"
#include "liot_http.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_power.h"
#include "liot_type.h"

#define LIOT_FOTA_HTTP_LOG_LEVEL
#define LIOT_FOTA_HTTP_LOG                liot_trace
#define LIOT_FOTA_HTTP_LOG_PUSH           liot_trace
#define LIOT_FOTA_PACK_NAME_MAX_LEN       (64)
#define LIOT_FOTA_APPLICATION_DEMO_ENABLE (0)

#define LIOT_TRY_DOWN_TIMES             100
#define LIOT_WRITE_TO_FILESIZE          (1024 * 5)
#define LIOT_VERSION_MAX                256
#define LIOT_HTTP_HEAD_RANGE_LENGTH_MAX 50
#define LIOT_HTTP_DLOAD_URL             "http://xiot.oss.senthink.com/upload-http-ftp/5D6A3E7E/112233.par"
#define LIOT_FOTA_AP_FILE_NAME          "lierda_fota_package.bin"

#define LIOT_FOTA_HTTP_RANGE_SIZE  (1024 * 5)    //http range length

static liot_task_t fota_http_task_nvm = NULL;
static liot_sem_t fota_http_semp_nvm  = NULL;
static liot_sem_t fota_http_semp_close = NULL;

static int gFotaPkgSise = 0;  //firmware package size
static int gFotaDownCycle  = 0; // fota times   
static bool cycleFlg = true;   //Calculate the number of cyclic downloads


extern UINT32 start_up_buffer;
extern UINT32 end_ap_data;


typedef enum
{
    LIOT_FOTA_HTTP_DOWN_INIT,    /*!< download init */
    LIOT_FOTA_HTTP_DOWN_DOWNING, /*!< download downing */
    LIOT_FOTA_HTTP_DOWN_INTR,    /*!< download intr */
    LIOT_FOTA_HTTP_DOWN_DOWNED,  /*!< download downed */
    LIOT_FOTA_HTTP_DOWN_NOSPACE, /*!< download nospace */
} e_fota_down_stage_e;

typedef struct
{
    bool is_show;    /*!< is show progress */
    uint total_size; /*!< total size */
    uint dload_size; /*!< dload size */
    uint file_size;  /*!< file size */
} fota_http_progress_s;

typedef struct
{
    liot_http_client_t http_cli;                        /*!< http client */
    liot_httpc_url_s http_url;                          /*!< http url */
    bool b_is_http_range;                               /*!< is http range */
    int profile_idx;                                    /*!< profile idx */
    uint8_t sim_id;                                     /*!< sim id */
    char fota_packname[LIOT_FOTA_PACK_NAME_MAX_LEN];    /*!< fota pack name */
    fota_http_progress_s http_progress;                 /*!< http progress */
    e_fota_down_stage_e e_stage;                        /*!< http download stage */
    LFILE fd;                                           /*!< file descriptor */
    int i_save_size;                                    /*!< save size */
    uint last_precent;                                  /*!< last percent */
    bool b_is_have_space;                               /*!< is have space */
    int chunk_encode;                                   /*!< chunk encode */
} fota_http_client_s;


/**
 * @brief  fota_dload_file_clran_nvm
 * @param[in]  fota_http_cli_p
 * @return  0: success, -1: fail
 * @note  This function is used to clean nvm.
 */
static int fota_dload_file_clran_nvm(fota_http_client_s *fota_http_cli_p)
{
    fota_http_cli_p->http_progress.file_size  = 0;
    fota_http_cli_p->http_progress.dload_size = 0;
    fota_http_cli_p->http_progress.total_size = 0;
    fota_http_cli_p->e_stage                  = LIOT_FOTA_HTTP_DOWN_DOWNING;
    fota_http_cli_p->i_save_size              = 0;
    LIOT_FOTA_HTTP_LOG("clran write file [%s] open fd %d", fota_http_cli_p->fota_packname, fota_http_cli_p->fd);
    return 0;
}

/**
 * @brief  fota_http_info_cfg
 * @param[in]  fota_http_cli_p
 * @note  This function is get config http info.    
 * @return  none
 */
static void fota_http_info_cfg(fota_http_client_s *fota_http_cli_p)
{
    if (fota_http_cli_p == NULL)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_cli_p is null");
        return;
    }
    LIOT_FOTA_HTTP_LOG("init file stage:[%d]", fota_http_cli_p->e_stage);
    LIOT_FOTA_HTTP_LOG("init file download:[%d]", fota_http_cli_p->http_progress.dload_size);
    LIOT_FOTA_HTTP_LOG("init file file_size:[%d]", fota_http_cli_p->http_progress.file_size);
    LIOT_FOTA_HTTP_LOG("init file real file_size:[%ld]", liot_fota_nvm_free_size_get());
    LIOT_FOTA_HTTP_LOG("init file is_show:[%d]", fota_http_cli_p->http_progress.is_show);
    LIOT_FOTA_HTTP_LOG("init file last_percent:[%d]", fota_http_cli_p->last_precent);
    LIOT_FOTA_HTTP_LOG("init file space:[%d]", fota_http_cli_p->b_is_have_space);
}

/**
 * @brief  fota_http_event_cb
 * @param[in]  http client handle
 * @param[in]  event  event id
 * @param[in]  event event code
 * @param[in]  argv  argv
 * @return  none
 * @note  This function is http event callback.
 */
static void fota_http_event_cb(liot_http_client_t *client, int event, int event_code, void *argv)
{
    if (argv == NULL)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_event_cb argv is null");
        return;
    }
    fota_http_client_s *fota_http_cli_p = (fota_http_client_s *)argv;
    LIOT_FOTA_HTTP_LOG("fota_http_event:%d,event_code:%d", event, event_code);

    switch (event)
    {
        case LIOT_HTTPC_SESSION_OPEN:
        {
            if (event_code != LIOT_HTTPC_SUCCESS)
            {
                LIOT_FOTA_HTTP_LOG("HTTP session create failed:%d!!!!!", event_code);
                //Save the download information; if the download interruption is not due to lack of storage space, set the status to "download interrupted".
                if (fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_NOSPACE ||
                    fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_DOWNED)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_INTR;
                }
                liot_rtos_semaphore_release(fota_http_semp_nvm);
            }
        }
        break;
        case LIOT_HTTPC_RESPONSE_STATUS:
        {
            if (event_code == LIOT_HTTPC_SUCCESS)
            {
                int resp_code      = 0;
                int content_length = 0;
                int chunk_encode   = 0;
                int content_range    = 0;
                // int accept_ranges = 0;
                char *location = NULL;
                liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &resp_code);
                liot_httpc_getinfo(client, LIOT_HTTPC_CHUNK_ENCODE, &chunk_encode);
                liot_httpc_getinfo(client, LIOT_HTTPC_CONTENT_RANGE, &content_range);
                LIOT_FOTA_HTTP_LOG("response code:%d chunk_encode %d,content_range %d", resp_code, chunk_encode, content_range);
                fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNING;
                if (resp_code == 200 || resp_code == 206)
                {
                    if (chunk_encode == 0)
                    {
                        if(content_range > 0)
                        {
                            gFotaPkgSise = content_range;
                        }
                        // liot_httpc_getinfo(client, HTTP_INFO_ACCEPT_RANGES, &accept_ranges);
                        liot_httpc_getinfo(client, LIOT_HTTPC_CONTENT_LEN, &content_length);
                        LIOT_FOTA_HTTP_LOG("content_length:%d", content_length);
                        // if(accept_ranges == 1 &&  fota_http_cli_p->b_is_http_range == TRUE)
                        if (fota_http_cli_p->b_is_http_range == TRUE)
                        {
                            fota_http_cli_p->http_progress.total_size += content_length;
                            if (cycleFlg)
                            {
                                cycleFlg = false;
                                   //Calculate the remaining download cycles
                                gFotaDownCycle = gFotaPkgSise/LIOT_FOTA_HTTP_RANGE_SIZE;
                                LIOT_FOTA_HTTP_LOG("gFotaDownCycle:%d", gFotaDownCycle);
                            }
                            
                        }
                        else
                        {
                            if (fota_dload_file_clran_nvm(fota_http_cli_p) == 0)
                            {
                                fota_http_cli_p->http_progress.total_size = content_length;
                            }
                            else
                            {
                                fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                            }
                        }
                        LIOT_FOTA_HTTP_LOG("content_length:[%d] totalsize=[%d]",
                                           content_length,
                                           fota_http_cli_p->http_progress.total_size);
                    }
                    else if (1 == chunk_encode)
                    {
                        LIOT_FOTA_HTTP_LOG("http chunk encode!");
                        fota_http_cli_p->chunk_encode = 1;
                    }
                }
                else
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                }
                // Requested Range Not Satisfiable
                if (resp_code == 416)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                }
                if (resp_code >= 300 && resp_code < 400)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                    liot_httpc_getinfo(client, LIOT_HTTPC_LOCATION, &location);
                    LIOT_FOTA_HTTP_LOG("redirect location:%s", location);
                    if (location  != NULL)
                    {
                        free(location);
                    }   
                }
            }
        }
        break;
        case LIOT_HTTPC_RESPONSE_TIMEOUT:
        case LIOT_HTTPC_RESPONSE_COMPLETE:
        {
            if (event_code == LIOT_HTTPC_SUCCESS)
            {
                //Download complete, set the configuration file to its initial state; only in this case can it be restored to the initial state.
                fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                LIOT_FOTA_HTTP_LOG("===http transfer end!!!!");
            }
            else
            {
                //Save the download information; if the download interruption is not due to lack of storage space, set the status to "download interrupted".
                if (fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_NOSPACE &&
                    fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_DOWNED)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_INTR;
                }
                LIOT_FOTA_HTTP_LOG("===http transfer occur exception!!!!!");
            }
            liot_rtos_semaphore_release(fota_http_semp_nvm);
        }
        break;
        case LIOT_HTTPC_SESSION_CLOSE:
        {
            liot_trace("http LIOT_HTTPC_SESSION_CLOSE success");
            liot_rtos_semaphore_release(fota_http_semp_close);
        }
    break;
    }
}

 
 /**
  * @brief Write data to the file
  * @param[in] fota_cli_p  http client handle
  * @param[in] data  data to be written
  * @param[in] size  size of data to be written
  * @param[in] fd  file descriptor 
  * @return int  0:success, -1:fail
  */
static int fota_http_write_file(fota_http_client_s *fota_cli_p, char *data, int size, LFILE fd)
{
    int ret   = -1;
    uint temp = 0;
    // if (fota_cli_p->fd != fd)
    // {
    //     LIOT_FOTA_HTTP_LOG("file fd error");
    //     fota_http_close_fd(fota_cli_p);
    //     return 0;
    // }
    // ret = liot_fwrite(data, size, 1, fd);
    ret = liot_fota_nvm_write(fota_cli_p->http_progress.dload_size,(uint8_t *)data, size);
    LIOT_FOTA_HTTP_LOG("write size:[%d] dload_size=[%d]", size, fota_cli_p->http_progress.dload_size);
    if (ret >= 0)
    {
        fota_cli_p->http_progress.dload_size += (uint)size;
        fota_cli_p->http_progress.file_size = fota_cli_p->http_progress.dload_size;
        if (fota_cli_p->http_progress.is_show == TRUE)
        {
            if (1 != fota_cli_p->chunk_encode)
            {
                //Calculate the progress. If progress display is enabled, compare the current progress with the previous progress. 
                //Only display the progress information if they are different.
                temp = 100UL * fota_cli_p->http_progress.dload_size / fota_cli_p->http_progress.total_size;
                if (fota_cli_p->last_precent != temp || temp == 100)
                {
                    fota_cli_p->last_precent = temp;
                    LIOT_FOTA_HTTP_LOG("dload progress:===[%u%%]===total size[%d] file_size[%d] dload size[%d]",
                                       temp,
                                       fota_cli_p->http_progress.total_size,
                                       liot_fsize(fd),
                                       fota_cli_p->http_progress.dload_size);
                }
            }
            else
            {
                LIOT_FOTA_HTTP_LOG("dload progress:=== file_size[%d] dload size[%d] ===",
                                   liot_fsize(fd),
                                   fota_cli_p->http_progress.dload_size);
            }
        }

        if ((fota_cli_p->i_save_size <= fota_cli_p->http_progress.dload_size) ||
            ((1 != fota_cli_p->chunk_encode) && (fota_cli_p->i_save_size >= fota_cli_p->http_progress.total_size)))
        {
            //Refresh the file system
            /*if(liot_fsync(fd) < LIOT_FS_OK)
            {
                LIOT_FOTA_HTTP_LOG("sync file failed");
                fota_http_close_fd(fota_cli_p);
                return 0;
            }*/

            if ((1 != fota_cli_p->chunk_encode) && (fota_cli_p->i_save_size >= fota_cli_p->http_progress.total_size))
            {
                fota_cli_p->i_save_size = fota_cli_p->http_progress.total_size;
            }
            else
            {
                fota_cli_p->i_save_size = fota_cli_p->http_progress.dload_size + LIOT_WRITE_TO_FILESIZE;
            }
        }
        if ((1 != fota_cli_p->chunk_encode) &&
            (fota_cli_p->http_progress.dload_size >= fota_cli_p->http_progress.total_size))
        {
            fota_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
        }
        // if (ret != size)
        // {
        //     fota_http_close_fd(fota_cli_p);
        // }
    }
    else
    {
        LIOT_FOTA_HTTP_LOG("error: ret:%d", ret);
        //Close the file descriptor of the firmware upgrade package
        liot_httpc_stop(&(fota_cli_p->http_cli));
    }
    return ret;
}

/**
 * @brief Write data to the file
 * @param[in] client  http client handle
 * @param[in] argv  liot_httpc_setopt用HTTP_CLIENT_OPT_WRITE_DATA传递的参数
 * @param[in] data  data to be written
 * @param[in] size  size of data to be written
 * @param[in] end  1 last packet of data 0 not last packet of data
 * @return int  0:success, -1:fail
 *
 */
static int fota_http_write_response_data(
    liot_http_client_t *client, void *argv, char *data, int size, unsigned char end)
{
    int ret            = -1;
    int write_size     = size;
    char *p_write_data = data;
    // int i_deal_size    = LIOT_WRITE_TO_FILESIZE;
    int file_free_size = 0;
    if (argv == NULL)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_write_response_data argv is invalied NULL ");
        return -2;
    }
    fota_http_client_s *fota_cli_p = (fota_http_client_s *)argv;
    if ((fota_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_DOWNED) || (fota_cli_p->chunk_encode == 1 && end == 1))
    {
        fota_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
        
        LIOT_FOTA_HTTP_LOG("go on dload file finished", fota_cli_p->fota_packname);
        // Close the file descriptor of the firmware upgrade package
        liot_httpc_stop(&(fota_cli_p->http_cli));
        return 0;
    }

    //Judgments on file size and storage in chunked versus non-chunked transfer
    file_free_size = liot_fota_nvm_free_size_get();
    if ((1 != fota_cli_p->chunk_encode &&
         file_free_size < (fota_cli_p->http_progress.total_size - fota_cli_p->http_progress.dload_size)) ||
        (1 == fota_cli_p->chunk_encode && file_free_size < size))
    {
        if (1 != fota_cli_p->chunk_encode)
        {
            LIOT_FOTA_HTTP_LOG("free_space[%d] total_size [%d] dload_size[%d]",
                               file_free_size,
                               fota_cli_p->http_progress.total_size,
                               fota_cli_p->http_progress.dload_size);
        }
        else
        {
            LIOT_FOTA_HTTP_LOG("free_space[%d] dload_size[%d]", file_free_size, fota_cli_p->http_progress.dload_size);
        }
        fota_cli_p->e_stage         = LIOT_FOTA_HTTP_DOWN_NOSPACE;
        fota_cli_p->b_is_have_space = FALSE;
        LIOT_FOTA_HTTP_LOG("file free_size not enough");
        liot_httpc_stop(&(fota_cli_p->http_cli));
        return 0;
    }

    if (size <= 0)
    {
        LIOT_FOTA_HTTP_LOG("write 0 size to file [%s]", fota_cli_p->fota_packname);
        liot_httpc_stop(&(fota_cli_p->http_cli));
        return -1;
    }
    /*
    A cache pool can also be added here. 
    Data can first be written into the cache pool (with a check to determine if the data volume exceeds the cache pool capacity). 
    When the cache pool is full, its contents are then written to the file system. 
    This prevents frequent small write operations to the file system, which could otherwise lead to high file system utilization.
    */

    // do
    // {
    //     if (write_size < i_deal_size)
    //     {
    //         i_deal_size = write_size;
    //     }
        ret = fota_http_write_file(fota_cli_p, p_write_data, size, fota_cli_p->fd);
        if (ret < 0)
        {
            LIOT_FOTA_HTTP_LOG("write file error");
            return size - write_size;
        }
    //     write_size -= ret;
    //     p_write_data += ret;
    // } while (write_size > 0);
    return size;
}

/**
 * @brief fota_http_init_nvm
 * @param[in] fota_http_cli_p   http client handle
 * 
 */
static void fota_http_init_nvm(fota_http_client_s *fota_http_cli_p)
{
    // LFILE fd = -1;
    liot_rtos_semaphore_create(&fota_http_semp_nvm, 0);
    liot_rtos_semaphore_create(&fota_http_semp_close, 0);
    memset(fota_http_cli_p, 0x00, sizeof(fota_http_client_s));

    fota_http_cli_p->http_cli    = 0; // http client handle
    fota_http_cli_p->profile_idx = 1; // cid
    fota_http_cli_p->sim_id      = 0; // simid
    fota_http_cli_p->e_stage     = LIOT_FOTA_HTTP_DOWN_INIT;
    fota_http_cli_p->i_save_size = 0;
    fota_http_cli_p->b_is_http_range       = FALSE;    //Range requests, not supported by default
    fota_http_cli_p->b_is_have_space       = TRUE;     //Storage space is assumed to be sufficient by default.
    fota_http_cli_p->http_progress.is_show = FALSE;     //Enable progress bar display
    fota_http_cli_p->last_precent          = 0;
    fota_http_cli_p->chunk_encode          = 0;

}

/**
 * @brief  fota_http_release
 * @param  None
 * @retval None
 * @note  This function is used to release the fota http client resources.Set to null after release
 */
static void fota_http_release()
{
    liot_rtos_semaphore_release(fota_http_semp_nvm);
    liot_rtos_semaphore_delete(fota_http_semp_nvm);
    liot_rtos_semaphore_release(fota_http_semp_close);
    liot_rtos_semaphore_delete(fota_http_semp_close);
    fota_http_semp_nvm = NULL;
    fota_http_semp_close = NULL;
}

 /**
  * @brief fota_http_active
  * @param fota_http_cli_p  http client handle
  * @param sim  sim id
  * @param cid  cid id
  * @return liot_datacall_errcode_e 
  */
static liot_datacall_errcode_e fota_http_active(uint8_t sim, int cid)
{
    liot_datacall_errcode_e ret        = LIOT_DATACALL_SUCCESS;
    liot_nw_reg_status_info_s reg_info = {0};
    liot_nw_get_reg_status(sim, &reg_info);
    while (LIOT_NW_REG_STATE_HOME_NETWORK != reg_info.data_reg.state)
    {
        LIOT_FOTA_HTTP_LOG("wait for network reg:%d", reg_info.data_reg.state);
        liot_rtos_task_sleep_ms(500);
        liot_nw_get_reg_status(sim, &reg_info);
    }
    liot_trace("===start data call====");
    ret = liot_start_data_call(sim, cid, LIOT_DATA_TYPE_IP, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("===data call result:%d", ret);

    liot_rtos_task_sleep_s(10);
    return LIOT_DATACALL_SUCCESS;
}

 /**
  * @brief fota_http_cli_init   
  * @param fota_http_cli_p  http client handle  
  * @return int 0  -- sucess  other 	-- failed
  * @note  This function is used to initialize the fota http client.
  */
static int fota_http_cli_init(fota_http_client_s *fota_http_cli_p)
{
    if (fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_INIT ||
        fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_DOWNED ||
        fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_NOSPACE)
    {
        fota_http_cli_p->http_progress.file_size  = 0;
        fota_http_cli_p->http_progress.dload_size = 0;
        fota_http_cli_p->http_progress.total_size = 0;
        fota_http_cli_p->e_stage                  = LIOT_FOTA_HTTP_DOWN_INIT;
        fota_http_cli_p->i_save_size              = 0;
        LIOT_FOTA_HTTP_LOG("over write file");
    }
    else
    {
        LIOT_FOTA_HTTP_LOG("add write file");
    }
    return 0;
}
/**
 * @brief fota_http_destroy
 * @param fota_http_cli_p  http client handle
 * @return None
 * @note  This function is used to destroy the fota http client.
 */
static void fota_http_destroy(fota_http_client_s *fota_http_cli_p)
{
    liot_httpc_stop(&(fota_http_cli_p->http_cli));
    liot_rtos_semaphore_wait(fota_http_semp_close, LIOT_WAIT_FOREVER);
    liot_httpc_release(&(fota_http_cli_p->http_cli));
}

/**
 * @brief  liot_http_url_free
 * @param  url  http url
 * @retval None
 * @note  This function is used to free the http url.
 */
static void liot_http_url_free(liot_httpc_url_s *url)
{
    if(url->host)
    {
        liot_rtos_free(url->host);
    }

    if(url->uri)
    {
        liot_rtos_free(url->uri);
    }

}
extern void liot_http_info_log(liot_http_client_t *client);
/**
 * @brief fota_http_request_init
 * @param fota_http_cli_p  http client handle
 * @return int 0  -- sucess  other 	-- failed
 * @note  This function is used to initialize the http request environment, build http request packet, and initiate http request.
 */
static int fota_http_request_init(fota_http_client_s *fota_http_cli_p)
{
    int cid = fota_http_cli_p->profile_idx;
    liot_httpc_url_s local_http_url;
     //Create a file descriptor for the upgrade package file before sending the HTTP request.
    if (fota_http_cli_init(fota_http_cli_p) < 0)
    {
        LIOT_FOTA_HTTP_LOG("range_request http data done ,file_size[%d]", fota_http_cli_p->http_progress.file_size);
        return -1;
    }
    memset(&local_http_url, 0x00, sizeof(local_http_url));
    /*
    After the address resolution is successful, 
    this interface internally allocates memory for the `uri` and `host` fields in `liot_httpc_url_s`. 
    If called separately, the user must actively release this memory once after allocation. However,
    if used in conjunction with the `liot_httpc_setopt` interface to set the URL, 
    manual release is not required, as the internal implementation will handle memory deallocation.
    */
    if (TRUE != liot_httpc_url_parse(LIOT_HTTP_DLOAD_URL, &(local_http_url)))
    {
        LIOT_FOTA_HTTP_LOG("fota-url-parse fail!!!");
        return -1;
    }


    if (liot_httpc_new(&(fota_http_cli_p->http_cli), fota_http_event_cb, fota_http_cli_p) != LIOT_HTTPC_SUCCESS)
    {
        LIOT_FOTA_HTTP_LOG("http create failed");
        liot_httpc_release(&(fota_http_cli_p->http_cli));
        liot_http_url_free(&(local_http_url));
        return -2;
    }

     //Set the HTTP request method to HTTP_METHOD_GET.
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_METHOD, LIOT_HTTPC_METHOD_GET);
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_PDPCID, cid);
      //Set the URL download address.   
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_URL, &(local_http_url));
    //set the sim id.
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_SIM_ID, fota_http_cli_p->sim_id);
    //set the http pdp cid
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_PDPCID, fota_http_cli_p->profile_idx);
    // Receive the file content from the response body
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, fota_http_write_response_data);
    // Set the second parameter of fota_http_write_response_data to fota_http_cli
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_WRITE_DATA, fota_http_cli_p);
    return 0;
}



/**
 * @brief fota_http_evn_request  
 * @param fota_http_cli_p http client handle    
 * @return int 0  -- sucess  other 	-- failed
 * @note   fota_http_evn_request  is used to request the event data from the server.
 */
static int fota_http_evn_request(fota_http_client_s *fota_http_cli_p)
{
    char dload_range[LIOT_HTTP_HEAD_RANGE_LENGTH_MAX] = {0};
    int ret = 0;
    
    //Chunked Download
    if (fota_http_cli_p->b_is_http_range == TRUE)
    {
        LIOT_FOTA_HTTP_LOG("dload_range");
        //Download resume
        fota_http_cli_p->http_progress.dload_size = fota_http_cli_p->http_progress.file_size;
        //set range length
        int posEnd = fota_http_cli_p->http_progress.file_size + LIOT_FOTA_HTTP_RANGE_SIZE;
        if((posEnd > gFotaPkgSise) && (gFotaPkgSise != 0))
        {
            posEnd = gFotaPkgSise - 1;
        }
        sprintf(dload_range, "Range: bytes=%d-%d", fota_http_cli_p->http_progress.file_size, posEnd);
        
       
        liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, dload_range);
        LIOT_FOTA_HTTP_LOG("Get http %s", dload_range);
       
    }
    //send request
    if ((ret = liot_httpc_perform(&fota_http_cli_p->http_cli)) == LIOT_HTTPC_SUCCESS)
    {
        if (liot_rtos_semaphore_wait(fota_http_semp_nvm, LIOT_WAIT_FOREVER) != LIOT_OSI_SUCCESS)
        {
            LIOT_FOTA_HTTP_LOG("liot_rtos_semaphore_wait failed");
            return -1;
        }
        LIOT_FOTA_HTTP_LOG("fota http dload size %d=====End,\n", fota_http_cli_p->http_progress.dload_size);
       
        return 0;
    }
    else
    {
        LIOT_FOTA_HTTP_LOG("http perform failed!!! ret = %d", ret);
        return -3;
    }
}

 /**
  * @brief fota_http_download_pacfile
  * @param fota_http_cli_p http client handle   
  * @return int 0  -- sucess  other 	-- failed
  */
static int fota_http_download_pacfile(fota_http_client_s *fota_http_cli_p)
{
    int ret  = 0;

    fota_http_info_cfg(fota_http_cli_p);
    
    ret = fota_http_request_init(fota_http_cli_p);
    if (ret != 0)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_request_init failed");
        return ret;
    }
    do
    {
        if (fota_http_evn_request(fota_http_cli_p) != 0)
        {
            int file_size = liot_fota_nvm_free_size_get();
            LIOT_FOTA_HTTP_LOG("liot_fota_nvm_free_size_get size[%d]", file_size);
            ret = -1;
            break;
        }
        fota_http_info_cfg(fota_http_cli_p);
        LIOT_FOTA_HTTP_LOG("fota_http_evn_request cycle = %d", gFotaDownCycle);
    }while (gFotaDownCycle--);
    
    fota_http_destroy(fota_http_cli_p);
    return ret;
}

/**
 * @brief fota_http_result_process_nvm  
 * @param void
 * @return liot_fota_result_e 
 * @note   fota_http_result_process_nvm  is used to process the fota result.
 */
static liot_fota_result_e fota_http_result_process_nvm(void)
{
    liot_fota_result_e p_fota_result = 0;

    //获取升级结果
    if (liot_fota_get_result(&p_fota_result) != LIOT_FOTA_UPGRADE_SUCCESS)
    {
        LIOT_FOTA_HTTP_LOG("liot_fota_get_result failed ");
        return LIOT_FOTA_STATUS_INVALID;
    }

    if (p_fota_result == LIOT_FOTA_FINISHED)
    {
        LIOT_FOTA_HTTP_LOG("update finished");
        // liot_fota_file_reset(TRUE);
        return LIOT_FOTA_FINISHED;
    }
    else if (p_fota_result == LIOT_FOTA_UPGRADE_READY)
    {
        LIOT_FOTA_HTTP_LOG("fota ready bigen power reset ");
        liot_rtos_task_sleep_s(5);
        liot_power_reset(LIOT_RESET_NORMAL);
    }
    else if (p_fota_result == LIOT_FOTA_NOT_EXIST)
    {
        LIOT_FOTA_HTTP_LOG("fota file not exist");
        // liot_fota_file_reset(TRUE);
        return LIOT_FOTA_NOT_EXIST;
    }
    LIOT_FOTA_HTTP_LOG("fota  result status invalid");
    return LIOT_FOTA_STATUS_INVALID;
}

void liot_fota_http_nvm_thread(void *arvg)
{
    liot_rtos_task_sleep_ms(10000);
    LIOT_FOTA_HTTP_LOG("init file real file_size:[%ld]", liot_fota_nvm_free_size_get());
    
    fota_http_client_s fota_http_cli;
    uint8 ui_down_times                = LIOT_TRY_DOWN_TIMES;
    char version_buf[LIOT_VERSION_MAX] = {0};
    //get fota result
    if (fota_http_result_process_nvm() == LIOT_FOTA_FINISHED)
    {
        LIOT_FOTA_HTTP_LOG("Fota file and flag clear!!!");
    }

    INT32 file_free_size = liot_fota_nvm_free_size_get();
    LIOT_FOTA_HTTP_LOG("fs free size get:0x%x", file_free_size);
    liot_rtos_task_sleep_s(2);
    liot_dev_get_firmware_version(version_buf, sizeof(version_buf));
    LIOT_FOTA_HTTP_LOG("current version:  %s", version_buf);
    fota_http_init_nvm(&fota_http_cli);
     //data call active
    if (LIOT_DATACALL_SUCCESS != fota_http_active(fota_http_cli.sim_id, fota_http_cli.profile_idx))
    {
        LIOT_FOTA_HTTP_LOG("http net is failed ");
        goto exit;
    }
    while (ui_down_times--)
    {
        cycleFlg = true;

        if(liot_fota_nvm_init() !=  LIOT_FOTA_UPGRADE_SUCCESS)
        {
            LIOT_FOTA_HTTP_LOG("liot_fota_nvm_init failed");
            break;
        }
        if (fota_http_download_pacfile(&fota_http_cli) == 0)
        {
            //download success
            if (fota_http_cli.e_stage == LIOT_FOTA_HTTP_DOWN_DOWNED)
            {
                LIOT_FOTA_HTTP_LOG("fota_http_download_pacfile success");
                if (fota_http_cli.b_is_http_range == TRUE)
                {
                    //check fota package 
                    if (gFotaPkgSise == fota_http_cli.http_progress.dload_size)
                    {
                        LIOT_FOTA_HTTP_LOG("fota_http_download_pacfile is complete package");
                    }
                }

                 //Verify whether the downloaded file is valid
                int ret = liot_fota_nvm_image_verify();
                if (ret != LIOT_FOTA_UPGRADE_SUCCESS)
                {
                    liot_fota_clear(NULL,TRUE);
                    LIOT_FOTA_HTTP_LOG("[%s]package is invalid", fota_http_cli.fota_packname);
                    //break;
                }
                else
                {
                    //verify  successful, reset power
                    LIOT_FOTA_HTTP_LOG("download is sucuess ,system will reset power!");
                    liot_rtos_task_sleep_s(5);
                    liot_fota_power_reset(LIOT_RESET_NORMAL);
                }
                   
                LIOT_FOTA_HTTP_LOG("download success");
               // break;
             }
        }
        else
        {
            LIOT_FOTA_HTTP_LOG("fota_http_download_pacfile failed");
        }
        if (fota_http_cli.b_is_have_space != TRUE)
        {
            liot_fota_clear(NULL,TRUE);
            LIOT_FOTA_HTTP_LOG("have no space");
            //break;
        }
        liot_rtos_task_sleep_ms(2000);
    }
exit:
    fota_http_release();
    // liot_rtos_task_sleep_s(500);
    LIOT_FOTA_HTTP_LOG("exit liot_http_fota_demo,%d", fota_http_task_nvm);
    liot_rtos_task_delete(fota_http_task_nvm);
}
