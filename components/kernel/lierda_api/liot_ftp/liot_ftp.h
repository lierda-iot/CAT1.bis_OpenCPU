/**
 * @file liot_ftp.h
 * @brief ftp相关API
 * @ingroup LIOT_COMPONENTS_API
 * @defgroup LIOT_COMPONENTS_API
 * @copyright Lierda
 * @author chw
 * @date 2023-08-30
 * @version 01.01
 * @details 
 * @note 参考 \PLAT\project\ec618_0h00\ap\apps\lierdaEC_lib\demo\49_ftp
 */

#ifndef  __LIOT_FTP_H__
#define  __LIOT_FTP_H__

#include "liot_api_common.h"
#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
	LIOT_FTP_CLIENT_ERR_SUCCESS = 0,
	LIOT_FTP_CLIENT_ERR_INVALID_CLIENT = 1,
	LIOT_FTP_CLIENT_ERR_INVALID_HOST,
	LIOT_FTP_CLIENT_ERR_DNS_FAIL,
	LIOT_FTP_CLIENT_ERR_SOCK_CREATE_FAIL,
	LIOT_FTP_CLIENT_ERR_SOCK_BIND_FAIL = 5,
	LIOT_FTP_CLIENT_ERR_SOCK_CONN_FAIL,
	LIOT_FTP_CLIENT_ERR_SOCK_SEND_FAIL,
	LIOT_FTP_CLIENT_ERR_SOCK_RECV_FAIL,
	LIOT_FTP_CLIENT_ERR_SOCK_CLOSE_FAIL,
	LIOT_FTP_CLIENT_ERR_SOCK_DISCONNECTED = 10,
	LIOT_FTP_CLIENT_ERR_SSL_CONN_FAIL,
	LIOT_FTP_CLIENT_ERR_RESP_TIMEOUT,
	LIOT_FTP_CLIENT_ERR_CREATE_FILE_FAIL,
	LIOT_FTP_CLIENT_ERR_NO_FILE,
	LIOT_FTP_CLIENT_ERR_NO_DIR = 15,
	LIOT_FTP_CLIENT_ERR_PARAM,
	
	LIOT_FTP_CLIENT_ERR_UNKNOWN
} LIOT_FTP_CLIENT_ERR_E;


typedef enum {
	LIOT_FTP_CLIENT_SSL_ENABLE,	
	LIOT_FTP_CLIENT_OPT_START_POS,
	LIOT_FTP_CLIENT_FILE_TYPE,
	LIOT_FTP_CLIENT_TRANS_MODE,
	LIOT_FTP_CLIENT_RSP_TIMEOUT,
}LIOT_FTP_CLIENT_OPT_E;


/**
 * @brief receive callback
 * @param[in] void *ptr:data pointer
 * @param[in] uint32_t size:data length
 * @return None.
 * @see None.
 * @note None.
 */
typedef void (*LIOT_FTP_CLIENT_WRITE_CB_EX)(void *ptr, uint32_t size);

/**
 * @brief ftp client create
 * @param[in] None.
 * @return void*:client handle
 * @retval not NULL:succuss
 * @retval NULL:failed
 * @see None.
 * @note None.
 */
void *liot_ftp_client_new(void);

/**
 * @brief release ftp client
 * @param[in] void*:client handle
 * @return None.
 * @see None.
 * @note None.
 */
void liot_ftp_client_release(void *client);

/**
 * @brief set ftp client option
 * @param[in] void *client:client handle
 * @param[in] LIOT_FTP_CLIENT_OPT_E tag:option type
 * @param[in] void *param:option value
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_setopt(void *client, LIOT_FTP_CLIENT_OPT_E tag, void *param);

/**
 * @brief connect ftp server
 * @param[in] void *client:client handle
 * @param[in] char *hostname:server addr
 * @param[in] char *port:server port
 * @param[in] char *username:login username
 * @param[in] char *password:login password
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_open(void *client, char *hostname, char *port, char *username, char *password);

/**
 * @brief close ftp connect
 * @param[in] void *client:client handle
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_close(void *client);

/**
 * @brief get data from FTP server
 * @param[in] void *client:client handle
 * @param[in] char *remotefile:file URI
 * @param[in] char *localfile:local file name
 * @param[in] LIOT_FTP_CLIENT_WRITE_CB_EX write_cb:receive callback
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_get_ex(void *client, char *remotefile, char *localfile, LIOT_FTP_CLIENT_WRITE_CB_EX write_cb);

/**
 * @brief put data to FTP server
 * @param[in] void *client:client handle
 * @param[in] char *localfile:local file name
 * @param[in] char *remotefile:server file URI
 * @param[in] void *userData:upload data
 * @param[in] uint32_t uploadlen:upload data len
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_put_ex(void *client, char *localfile, char *remotefile, void *userData, uint32_t uploadlen);

/**
 * @brief delete FTP server file
 * @param[in] void *client:client handle
 * @param[in] char *remotefile:the file to be deleted
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_delete(void *client, char *remotefile);
/**
 * @brief get FTP server dir
 * @param[in] void *client:client handle
 * @param[in] char *path:Store the obtained information
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_pwd(void *client, char *path);

/**
 * @brief change FTP server dir
 * @param[in] void *client:client handle
 * @param[in] char *path:new dir
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_cwd(void *client, char *path);

/**
 * @brief create new FTP server dir
 * @param[in] void *client:client handle
 * @param[in] char *path:new dir
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_mkdir(void *client, char *path);

/**
 * @brief delete FTP server dir
 * @param[in] void *client:client handle
 * @param[in] char *path:the dir to be deleted
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_rmdir(void *client, char *path);

/**
 * @brief get FTP server dir list
 * @param[in] void *client:client handle
 * @param[in] char *path:server dir
 * @param[in] char *list:server dir list
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_list(void *client, char *path, char *list);

/**
 * @brief get file size
 * @param[in] void *client:client handle
 * @param[in] char *filename:file name 
 * @param[in] uint32 size:file size
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_size(void *client, char *filename, uint32_t *size);

/**
 * @brief rename the file
 * @param[in] void *client:client handle
 * @param[in] char *old_name:old file name
 * @param[in] char *new_name:new file name 
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_rename(void *client, char *old_name, char *new_name);

/**
 * @brief rename the file
 * @param[in] void *client:client handle
 * @param[in] uint32_t Type: FTP_ASCII,FTP_BINARY
 * @return int:error code, LIOT_FTP_CLIENT_ERR_E
 * @see None.
 * @note None.
 */
int liot_ftp_client_FileTpye(void *client, uint32_t Type);

#ifdef __cplusplus
}
#endif

#endif

