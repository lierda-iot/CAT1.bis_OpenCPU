/**
 * @file Name: liot_ftp_demo.c
 * @brief FTP client function demonstration
 * @author chw email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-08-18
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

/**
 * 1. Initialize FTP client
 * 2. Register network callback function
 * 3. Connect to FTP server
 * 4. Create directory on server
 * 5. Upload file to server
 * 6. Download file from server
 * 7. Delete file on server
 * 8. Release FTP client instance
 * 9. Print test results
 */

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_os.h"
#include "liot_ftp.h"
#include "liot_fs_api.h"
#include <string.h>

/* FTP data receive callback function */
void receiveCB(void *dataPtr, uint32_t len)
{
    liot_trace("--receive len = %d--\r\n%s", len, (char *)dataPtr);
}

/* FTP demo thread main function */
void liot_ftp_demo_thread(void *argv)
{
    int ret = -1;

    liot_rtos_task_sleep_s(2);  // Delay 2 seconds for system initialization
    liot_trace("========== FTP Demo Start ==========");

    /* Network registration process */
    int times = 0;
    uint8_t nSim = 0; //0:sim1 or 1:sim2
    while (LIOT_DATACALL_SUCCESS != (ret = liot_network_register_wait(nSim, 120)) && times < 10)
    {
        times++;
        liot_rtos_task_sleep_s(1);  // Retry after 1 second if registration fails
    }
    
    if (LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("==== Network Register Failure ====");
        goto exit;
    }
    liot_trace("==== Network Registered ====");

    /* Create FTP client instance */
    void *ftpClient = liot_ftp_client_new();
    if(ftpClient == NULL)
    {
        liot_trace("FTP Client Create Failed");
        goto exit;
    }
    liot_trace("FTP Client Create Success");
    
    /* ASCII = 0, BINARY = 1*/
    liot_ftp_client_FileTpye(ftpClient, 0);
    liot_trace("FTP Client set FileTpye ASCII");

    uint8_t timeout = 90;
    ret = liot_ftp_client_setopt(ftpClient, LIOT_FTP_CLIENT_RSP_TIMEOUT, &timeout);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("FTP Client set timeout error");
    }
    liot_trace("FTP Client set timeout %d", timeout);
    
    /* Connect to FTP server */
    ret = liot_ftp_client_open(ftpClient, "121.89.205.240", "21", "ftpuser", "123456");
    if(ret != LIOT_FTP_CLIENT_ERR_SUCCESS)
    {
        liot_trace("FTP Connect Failed! Error: %d", ret);
        goto release_client;
    }
    liot_trace("FTP Connect Success");

    /* Get current server directory */
    char path[64] = {0};
    ret = liot_ftp_client_pwd(ftpClient, path);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("Current Directory: %s", path);
    }

    /* Create directory on server */
    ret = liot_ftp_client_mkdir(ftpClient, "DIR_TEST");
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("Create Directory Success");
    }

    /* Change server directory */
    ret = liot_ftp_client_cwd(ftpClient, "DIR_TEST");
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("Change Directory Success");
    }

    /* Upload local file to server */
    char databuf[] = "HELLO WORLD!!!";
    LFILE fd = liot_fopen("localfile", "wb+");
    liot_fwrite(databuf, 1, strlen(databuf), fd);
    liot_fclose(fd);
    liot_trace("Write Data Success: %s", databuf);
    
    ret = liot_ftp_client_put_ex(ftpClient, "localfile", "oldName.txt", NULL, 0);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("File Upload Success");
    }

    /* Rename server file */
    ret = liot_ftp_client_rename(ftpClient, "oldName.txt", "newName.txt");
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("File Rename Success");
    }

    /* Get server file size */
    uint32_t filelen = 0;
    ret = liot_ftp_client_size(ftpClient, "newName.txt", &filelen);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("File Size: %d bytes", filelen);
    }

    /* Download file from server */
    ret = liot_ftp_client_get_ex(ftpClient, "newName.txt", NULL, receiveCB);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("File Download Success");
    }

    /* Change back to root directory */
    ret = liot_ftp_client_cwd(ftpClient, "/");
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("Change to Root Directory Success");
    }

    /* Get server directory list */
    char ftplist[256] = {0};                
    ret = liot_ftp_client_list(ftpClient, "DIR_TEST", ftplist);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("Directory List:\r\n%s", ftplist);
    }

    /* Delete server file */
    ret = liot_ftp_client_delete(ftpClient, "DIR_TEST/newName.txt");
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("File Delete Success");
    }

    /* Delete server directory */
    ret = liot_ftp_client_rmdir(ftpClient, "DIR_TEST");
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("Directory Delete Success");
    }

    /* Close FTP connection */
    ret = liot_ftp_client_close(ftpClient);
    if(ret == LIOT_FTP_CLIENT_ERR_SUCCESS) {
        liot_trace("FTP Disconnect Success");                    
    }

release_client:
    /* Release FTP client resources */
    liot_ftp_client_release(ftpClient);
    liot_trace("FTP Client Released");

    while(1) {
        liot_rtos_task_sleep_s(1);  // Keep thread running
    }
    
exit:    
    /* Thread exit processing */
    if (LIOT_OSI_SUCCESS != liot_rtos_task_delete(NULL)) {
        liot_trace("Task Delete Failed");
    }
    liot_trace("FTP Demo Finished");
    
    return;
}

