/**
 * @File Name: liot_mqtt_ali_demo.c
 * @brief
 * @Author : L email:ciot_iot_support@lierda.com
 * @Version : 1.1
 * @Creat Date : 2025-12-29
 *
 * @copyright Copyright (c) 2023 Lierda.com Inc. All rights reserved.
 *
 * @note : The function implemented by this file is to realize MQTT publishing, 
 * subscribing and other functions using MQTT API and Alibaba Cloud IOT. 
 * It connects to devices via two MQTT channels, and the device connection method adopts the "one product one secret" approach.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lierda_app_main.h"
#include "liot_datacall.h"

#include "liot_os.h"
#include "liot_type.h"
#include "liot_fs_api.h"
#include "liot_http.h"
#include "liot_mqtt_client.h"
#ifdef FEATURE_MQTT_TLS_ENABLE
#include "sha256.h"
#endif

#include "cJSON.h"
#include "liot_nv.h"
#include "liot_fs_api.h"
#include "liot_nw.h"
#include "liot_ssl.h"
 
#define ALI_SHA256_KEY_IOPAD_SIZE   (64)
#define ALI_SHA256_DIGEST_SIZE      (32)

#define PRODUCTKEY_MAXLEN           (20)
#define DEVICENAME_MAXLEN           (32)
#define DEVICESECRET_MAXLEN         (65)

#define SIGN_SOURCE_MAXLEN          (200)
#define CLIENTID_MAXLEN             (150)
#define USERNAME_MAXLEN             (64)
#define PASSWORD_MAXLEN             (65)

#define ALI_PRODUCT_SECRET  "A6ohHn93T0rlgpJO"
#define ALI_PRODUCT_KEY     "htma2li9B85"
#define ALI_DEVICE_NAME1     "IOT-40"
#define ALI_DEVICE_NAME2     "IOT-41"

// #define MQTT_CLIENT_IDENTITY "866179061276528|securemode=2,signmethod=hmacsha1,timestamp=4070880000000|"
// #define MQTT_CLIENT_USER     "866179061276528&hf05listj7l"
// #define MQTT_CLIENT_PASS     "da4a91865e995fc663fb9286c05d956026c52a0b"

//HTTP request url
#define HTTP_UARL "https://iot-auth.cn-shanghai.aliyuncs.com/auth/register/device"
char* authType = "register";
#define LIOT_NV_MQTT1_CFG "liot_nv_mqtt1_cfg.nvm"
#define LIOT_NV_MQTT2_CFG "liot_nv_mqtt2_cfg.nvm"

#define PRODUCTKEY_MAXLEN           (20)
#define DEVICENAME_MAXLEN           (32)
#define DEVICESECRET_MAXLEN         (65)

#define SIGN_SOURCE_MAXLEN          (200)
#define CLIENTID_MAXLEN             (150)
#define USERNAME_MAXLEN             (64)
#define PASSWORD_MAXLEN             (65)

// #define MQTT_CLIENT_IDENTITY "866179061276528|securemode=2,signmethod=hmacsha1,timestamp=4070880000000|"
// #define MQTT_CLIENT_USER     "866179061276528&hf05listj7l"
// #define MQTT_CLIENT_PASS     "da4a91865e995fc663fb9286c05d956026c52a0b"

 // subscribe topic
#define MQTT_ALI_SUB_TOPIC1   "/" ALI_PRODUCT_KEY "/" ALI_DEVICE_NAME1 "/user/1234"
//publish topic
#define MQTT_ALI_PUB_TOPIC1   "/" ALI_PRODUCT_KEY "/" ALI_DEVICE_NAME1 "/user/1234"

//subscribe topic
#define MQTT_ALI_SUB_TOPIC2   "/" ALI_PRODUCT_KEY "/" ALI_DEVICE_NAME2 "/user/1234"
//publish topic
#define MQTT_ALI_PUB_TOPIC2   "/" ALI_PRODUCT_KEY "/" ALI_DEVICE_NAME2 "/user/1234"

// publish topic
#define MQTT_ALI_PUB_PAYLOAD "update"
// mqtt client url
#define MQTT_CLIENT_URL      "mqtt://iot-06z00iqw9ampzrw.mqtt.iothub.aliyuncs.com:1883"

INT32 cmsHexStrToHex(UINT8 *pOutput, UINT32 outBufSize, const CHAR *pHexStr, UINT32 strMaxLen);
INT32 cmsHexToHexStr(CHAR *pOutStr, UINT32 outBufSize, const UINT8 *pHexData, UINT32 hexLen);

//mqtt connect status
static int mqtt1_connected = 0, mqtt2_connected = 0;
//semaphore
static liot_sem_t mqtt_semp1, mqtt_semp2;

liot_mqtt_client_t mqtt_cli1 = 0, mqtt_cli2 = 0;

#define LIOT_HTTPC_ENABLE  1

#if LIOT_HTTPC_ENABLE

#define LIOT_HTTP_DEMO_SSL_TEST  0

unsigned char *recv_buf = NULL;
static liot_sem_t http_semp;

const char *HTTPS_CA_CERT = \
{
    \
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\r\n" \
    "A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\r\n" \
    "b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\r\n" \
    "MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\r\n" \
    "YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\r\n" \
    "aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\r\n" \
    "jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\r\n" \
    "xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\r\n" \
    "1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\r\n" \
    "snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\r\n" \
    "U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\r\n" \
    "9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\r\n" \
    "BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\r\n" \
    "AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\r\n" \
    "yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\r\n" \
    "38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\r\n" \
    "AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\r\n" \
    "DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\r\n" \
    "HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\r\n" \
    "-----END CERTIFICATE-----"
};


/**
 * @brief HmacSha256 algorithm
 * @param input Input data to be encrypted
 * @param ilen Input data length
 * @param output Encrypted string
 * @param key Key
 * @param keylen Key length
 * @return void
 */

static void HmacSha256(const unsigned char *input, int ilen, unsigned char *output,const unsigned char *key, int keylen)
{
    int i;
    mbedtls_sha256_context ctx;
    unsigned char k_ipad[ALI_SHA256_KEY_IOPAD_SIZE] = {0};
    unsigned char k_opad[ALI_SHA256_KEY_IOPAD_SIZE] = {0};

    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5C, 64);

    if ((NULL == input) || (NULL == key) || (NULL == output)) {
        return;
    }

    if (keylen > ALI_SHA256_KEY_IOPAD_SIZE) {
        return;
    }

    for(i=0; i<keylen; i++)
    {
        if(i>=ALI_SHA256_KEY_IOPAD_SIZE)
        {
            break;
        }
        k_ipad[i] ^=key[i];
        k_opad[i] ^=key[i];
    }
    mbedtls_sha256_init(&ctx);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_ipad, ALI_SHA256_KEY_IOPAD_SIZE);
    mbedtls_sha256_update(&ctx, input, ilen);
    mbedtls_sha256_finish(&ctx, output);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_opad, ALI_SHA256_KEY_IOPAD_SIZE);
    mbedtls_sha256_update(&ctx, output, ALI_SHA256_DIGEST_SIZE);
    mbedtls_sha256_finish(&ctx, output);

    mbedtls_sha256_free(&ctx);
}

/**
 * @brief HTTP request to get request body
 * @param productkey Product key
 * @param deviceName Device name
 * @param productSecret Product secret
 * @param requestdata Request data body
 * @return INT32 Request result -1表示失败 0表示成功
 */

INT32 liot_getHttpRequestData(char* productkey, char* deviceName, char* productSecret, char* requestdata)
{
int dataLen = 0;
    uint8_t* sign = NULL;
    int radomData = 0;
    uint8_t sign_input_fmt[128] = {0};
    char hexstr[65] = {0};
    liot_trace("productkey: %s, deviceName: %s, productSecret: %s", productkey, deviceName, productSecret);
    if((NULL == productkey) || (NULL == deviceName) || (NULL == productSecret) || (NULL == requestdata))
    {
        return -1;
    }
    radomData = rand();
    sign = malloc(SIGN_SOURCE_MAXLEN);
    if(NULL == sign)
    {
        return -1;
    }

    sprintf((char*)sign_input_fmt, "deviceName%sproductKey%srandom%d", deviceName, productkey, radomData);
    liot_trace("sign_input_fmt: %s", sign_input_fmt);

    HmacSha256(sign_input_fmt, strlen((char*)sign_input_fmt), sign, (uint8_t*)productSecret, strlen(productSecret));

    cmsHexToHexStr(hexstr, 64, sign, 32);
    liot_trace("hexstr: %s", hexstr);
    sprintf(requestdata, "productKey=%s&deviceName=%s&random=%d&sign=%s&signMethod=HmacSha256", productkey, deviceName, radomData, hexstr);
    liot_trace("requestdata: %s", requestdata);

    free(sign);
    dataLen = strlen(requestdata);
    return dataLen;
}

/**
 * @brief Get device secret
 * @param inStr Input json string
 * @param deviceSecret Device secret
 * @return INT32 Request result -1表示失败 0表示成功
 */
INT32 cjsonGetDeviceSercite(char* inStr,  char* deviceSecret)
{
    if((NULL == inStr) || (NULL == deviceSecret) )
    {
        liot_trace("Failed to parse JSON data!.\n");
        return -1;
    }
    cJSON *json = cJSON_Parse(inStr);
    if (json == NULL)
    {
        liot_trace("Failed to parse JSON data.\n");
        return -1;
    }

    cJSON *code = cJSON_GetObjectItem(json, "code");
    if(NULL != code)
    {
        liot_trace("Code: %d\n", code->valueint);
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if(NULL != data)
    {
        cJSON *device_name = cJSON_GetObjectItem(data, "deviceName");
        if(device_name != NULL)
        {
            liot_trace("Device Name: %s\n", device_name->valuestring);
        }
        
        cJSON *device_secret = cJSON_GetObjectItem(data, "deviceSecret");
        if(NULL != device_secret)
        {
            liot_trace("Device Secret: %s\n", device_secret->valuestring);
            strcpy(deviceSecret, device_secret->valuestring);
        }

        cJSON *product_key = cJSON_GetObjectItem(data, "productKey");
        if(NULL != product_key)
        {
            liot_trace("Product Key: %s\n", product_key->valuestring);
        }
        cJSON *message = cJSON_GetObjectItem(json, "message");
        liot_trace("Message: %s\n", message->valuestring);
    }
    else
    {
        liot_trace("Failed to parse JSON data!!.\n");
        return -1;
    }
    cJSON_Delete(json);
    return 0;
}

/**
 * @brief http event cb
 * @param client http client
 * @param evt http event
 * @param evt_code http event code
 * @param arg http event arg
 * @retval none
 * @note none
 */

static void http_event_cb(liot_http_client_t *client, int evt, int evt_code, void *arg)
{
    liot_trace("===http_event_cb===  evt:%d,evt_code:%d,%p", evt, evt_code, client);
    switch (evt)
    {
        /*http session open*/
        case LIOT_HTTPC_SESSION_OPEN:
        {
            if (evt_code != LIOT_HTTPC_SUCCESS)
            {
                liot_trace("http session open ERROR");
                liot_rtos_semaphore_release(http_semp);
            }
        }
        break;
        case LIOT_HTTPC_UPLOAD_START:
        {
            liot_httpc_user_notify(client, LIOT_HTTPC_READ);
        }
        break;
        case LIOT_HTTPC_UPLOAD_END:
        {
        }
        break;
        case LIOT_HTTPC_RESPONSE_STATUS:
        {
            if (evt_code == LIOT_HTTPC_SUCCESS)
            {
                int resp_code      = 0;
                int content_length = 0;
                int chunk_encode   = 0;
                char *location     = NULL;
                char *date         = NULL;
                liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &resp_code);
                liot_httpc_getinfo(client, LIOT_HTTPC_CHUNK_ENCODE, &chunk_encode);
                liot_httpc_getinfo(client, LIOT_HTTPC_DATE, &date);
                if (date != NULL)
                {
                    liot_trace("===http_event_cb=== Date:%s", date);
                    liot_rtos_free(date);
                }
                liot_trace("===http_event_cb=== resp_code:%d,chunk_encode:%d", resp_code, chunk_encode);
                if (chunk_encode == 0)
                {
                    liot_httpc_getinfo(client, LIOT_HTTPC_CONTENT_LEN, &content_length);
                    liot_trace("===http_event_cb=== content_length:%d", content_length);
                }
                else
                {
                    liot_trace("http chunk encode");
                }

                if (resp_code >= 300 && resp_code < 400)
                {
                    liot_httpc_getinfo(client, LIOT_HTTPC_LOCATION, &location);
                    liot_trace("===http_event_cb=== location:%s", location);
                    liot_rtos_free(location);
                }
            }
            else if(evt_code == LIOT_HTTPC_ERR_SOCKET_FAILURE)
            {
                liot_rtos_semaphore_release(http_semp);
            }
        }
        break;
        case LIOT_HTTPC_RESPONSE_COMPLETE:
        {
            if (evt_code == LIOT_HTTPC_SUCCESS)
            {
                liot_trace("http transfer success");
            }
            else
            {
                liot_trace("http transfer fail");
            }
            liot_rtos_semaphore_release(http_semp);
        }
        break;
        case LIOT_HTTPC_RESPONSE_TIMEOUT:
        {
            liot_trace("http response timeout");
            liot_rtos_semaphore_release(http_semp);
        }
        break;
        case LIOT_HTTPC_SESSION_CLOSE:
        {
            liot_trace("http LIOT_HTTPC_SESSION_CLOSE success");
            liot_rtos_semaphore_release(http_semp);
        }
        break;
    }
}

#if LIOT_HTTP_DEMO_SSL_TEST
/**
 * @brief  http ssl param cfg   
 * @param client http client
 * @param verify_level https verify level
 * @return true if success, false if failed
 */
bool liot_https_param_cfg(liot_http_client_t *client, liot_https_verify_level_e verify_level)
{
    if((*client == 0) || (verify_level > 2))
    {
        liot_trace("http liot_https_param_cfg failed!!!!");
        return false;
    }

    unsigned short hs_timeout    = 300;

    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSLCTXID, 1);      // config ssl id
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERSION, LIOT_SSL_VERSION_3);     // ssl version
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_HS_TIMEOUT, hs_timeout);
    // liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_SNI, 0);

    liot_trace("%s:%d  verify_level:%d ", __FUNCTION__, __LINE__, verify_level);
    if (verify_level == LIOT_HTTPS_VERIFY_SERVER)
    {
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL, LIOT_HTTPS_VERIFY_SERVER);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_CACERT_DATA, HTTPS_CA_CERT);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_INVALID_CERT_SIGN, 1);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_CERT_ITEM, 0xffff);
    }
    else if (verify_level == LIOT_HTTPS_VERIFY_SERVER_CLIENT) 
    {
    liot_trace(">>>>>verify_level error:%d ", verify_level);
    return false;
    }
    liot_trace(">>>>>verify_level:%d ", verify_level);
    return true;
}
#endif

static int http_recv_data_cb(liot_http_client_t *client, void *arg, char *data, int size, unsigned char end)
{
    liot_trace(">>>>>client:%p", client);
    liot_trace("===http_response_write_data_cb=== size:%d", size);
    
    recv_buf                = malloc(size + 1);
    if (recv_buf != NULL)
    {
        memset(recv_buf, 0, size + 1);
        memcpy(recv_buf, data, size);
        liot_trace("===http_response_write_data_cb=== recv_buf:%s", recv_buf);
    }
    return size;
}

/**
 * @brief http request send data cb
 * @param client http client
 * @param arg http request send data cb arg
 * @param data http request send data
 * @param size http request send data size
 * @return int http request send data size
 */
static int http_request_send_data_cb(liot_http_client_t *client, void *arg, char *data, int size)
{
    liot_trace(">>>>>client:%p,%d", client, arg);
    if ((client == NULL) || (arg == NULL) || (data == NULL) || (size <= 0))
    {
        return 0;
    }
    memset(data, 0, size);
    memcpy(data, arg, size);
    return size;
}

 /**
  * @brief    http init
  * @param nSim  sim id 
  * @param cid  data call id
  * @param client  http client
  * @param httpHost  http host
  * @param http_method  http method
  * @return int 
  */
 INT32 liot_http_init(uint8_t nSim, uint8_t cid, liot_http_client_t *client, char *httpHost, int http_method)
 {
     liot_httpc_url_s *url = NULL;
     if (client == NULL || httpHost == NULL )
     {
         liot_trace("http liot_http_requeset_data failed!!!!");
         return -1;
     }
     url = liot_rtos_malloc(sizeof(liot_httpc_url_s));
     if(NULL == url)
     {
         liot_trace("http liot_http_requeset_data memary malloc failed!!!!");
         return -1;
     }
 
     if(liot_httpc_url_parse(httpHost, url) == false)
     {
         liot_trace("http liot_http_requeset_data liot_httpc_url_parse failed!!!!");
         liot_rtos_free(url);
         return -1;
     }
     liot_trace(">>>>>>url:%s,port:%d,path:%s,scheme:%d", url->host, url->port, url->uri, url->scheme);
     liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SIM_ID, nSim);
     liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_PDPCID, cid);
     liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_METHOD, http_method);
     liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_URL, url);
     liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, http_recv_data_cb);
     if (http_method == LIOT_HTTPC_METHOD_POST)
     {  
         liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_READ_FUNC, http_request_send_data_cb);
         liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, "Content-type: application/x-www-form-urlencoded");
     }
    
 
 #if LIOT_HTTP_DEMO_SSL_TEST
     if(url->scheme != LIOT_HTTPC_SCHEME_HTTPS)
     {
         liot_trace("url->scheme =%s", url->scheme);
         return -1;
     }
     if(liot_https_param_cfg(client, LIOT_HTTPS_VERIFY_SERVER) == false)
     {
         liot_trace("liot_https_param_cfg failed!!!!");
         return -1;
     }
 #endif
     return 0;
 }
 
 
int liot_http_request(uint8_t nSim, uint8_t cid, char *httpHost, int http_method, char *postData)
{
    liot_http_client_t http_client = 0;
    int ret = -1;
    uint32 postDataLen = 0;
    liot_rtos_semaphore_create(&http_semp, 0);
    if ((httpHost == NULL) || (postData == NULL))
    {
        liot_trace("http liot_http_requeset_data failed!!!!");
        return -1;
    }
    if (liot_httpc_new(&http_client, http_event_cb, NULL) != LIOT_HTTPC_SUCCESS)
    {
        liot_trace("http client create failed!!!!");
        return -1;
    }
    ret = liot_http_init(nSim, cid, &http_client, httpHost, http_method);
    if(ret != 0)
    {
        liot_trace("http liot_http_requeset_data failed!!!!");
        return -1;
    }
    if (http_method == LIOT_HTTPC_METHOD_POST)
    {
        liot_trace("http liot_http_requeset_data postData:%d", postData);
        postDataLen = strlen(postData);
        liot_httpc_setopt(&http_client, LIOT_HTTP_CLIENT_OPT_READ_DATA, postData);
        liot_httpc_setopt(&http_client, LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN, postDataLen);
    }
    if(liot_httpc_perform(&http_client) == LIOT_HTTPC_SUCCESS)
    {
        liot_trace("liot_rtos_semaphore_wait");
        liot_rtos_semaphore_wait(http_semp, LIOT_WAIT_FOREVER);
        liot_trace("http perform client success");
        ret = 0;
    }
    else
    {
        liot_trace("http perform client fail");
        ret = -1;
    }
    liot_httpc_stop(&http_client);
    liot_rtos_semaphore_wait(http_semp, LIOT_WAIT_FOREVER);
    liot_httpc_release(&http_client);
    liot_rtos_semaphore_release(http_semp);
    liot_rtos_semaphore_delete(http_semp);
    http_semp = NULL;
    return ret;
}

int liot_get_mqtt_devSercite(int nSim, int cid, char* productkey, char* deviceName, char* productSecret, char *devSercite)
{ 
    int ret = -1;
    char *reqData = NULL;
    if (devSercite == NULL)
    {
        liot_trace("http liot_http_requeset_data failed!!!!");
        return -1;
    }
    reqData = liot_rtos_malloc(256);
    if (reqData == NULL)
    {
        liot_trace("http liot_http_requeset_data failed!!!!");
        return -1;
    }
    ret = liot_getHttpRequestData(productkey, deviceName, productSecret, reqData);
    if(ret <= 0)
    {
        liot_trace("http liot_http_requeset_data failed!!!!");
        return -1;
    }
    ret = liot_http_request(nSim, cid, HTTP_UARL, LIOT_HTTPC_METHOD_POST, reqData);
    if(ret != 0)
    {
        liot_trace("http liot_http_requeset_data failed!!!!");
        ret = -1;
    }
    else
    {
       ret = cjsonGetDeviceSercite((char*)recv_buf, devSercite);
    }
    if (recv_buf != NULL)
    {
        liot_rtos_free(recv_buf);
    }
    if(reqData != NULL)
    {
        liot_rtos_free(reqData);
    }
    return ret;
}

/**
 * @brief Get MQTT client information
 * @param productkey Product key
 * @param deviceName Device name
 * @param deviceSecret Device secret
 * @param clientId MQTT client ID
 * @param username MQTT login username
 * @param password MQTT login password
 * @return INT32 Request result -1表示失败 0表示成功
 */

 int32_t getMqttClientInfo(char* productkey, char* deviceName, char* deviceSecret,
    char* clientId, char* username, char* password)
{
    uint8_t* sign = NULL;
    // int radomData = 0;
    char clientId_in_fmt[256] = {0};
    char userName_in_fmt[65] = {0};
    uint8_t sign_input_fmt[128] = {0};

    if((NULL == productkey) || (NULL == deviceName) || (NULL == deviceSecret) ||
    (NULL == clientId) || (NULL == username) || (NULL == password) )
    {
        return -1;
    }
    // radomData = rand();
    sign = malloc(SIGN_SOURCE_MAXLEN);
    if(NULL == sign)
    {
        return -1;
    }
    sprintf(clientId_in_fmt, "%s&%s|securemode=3,signmethod=hmacsha256|",deviceName, productkey);
    liot_trace("clientId_in_fmt: %s", clientId_in_fmt);
    strcpy(clientId, clientId_in_fmt);
    sprintf(userName_in_fmt, "%s&%s", deviceName, productkey);
    liot_trace("userName_in_fmt: %s", userName_in_fmt);
    strcpy(username, userName_in_fmt);
    sprintf((char*)sign_input_fmt, "clientId%sdeviceName%sproductKey%s",userName_in_fmt, deviceName, productkey);
    liot_trace("sign_input_fmt: %s", sign_input_fmt);
    HmacSha256(sign_input_fmt, strlen((char*)sign_input_fmt), sign, (uint8_t*) deviceSecret, strlen(deviceSecret));
    cmsHexToHexStr(password, 64, sign, 32);
    liot_trace("password: %s", password);
    free(sign);
    return 0;

}


/**
 * @brief Get mqtt client information   
 * @param fileName File name
 * @param url Post request URL
 * @param productKey Product key
 * @param deviceName Device name
 * @param productSecret Product secret
 * @param clientId Client ID
 * @param username Username
 * @param password Password
 * @return int32_t 
 */
int32_t liot_get_mqtt_parma(int nSim, int cid, char* fileName, char* url, char* productKey, char* deviceName, char* productSecret,
    char* clientId, char* username, char* password)
{ 
    LFILE fd = 0;
    int32_t result = 0;
    char deviceSecret[DEVICESECRET_MAXLEN] = {0};
    // int32_t deviceSecretLen = 0;
    if(clientId == NULL || username == NULL || password == NULL || url == NULL ||
    productKey == NULL || deviceName == NULL || productSecret == NULL || fileName == NULL)
    {
        liot_trace("clientId, username or password is NULL");
        return -1;
    }

    liot_trace("mqtt test start");
    // get device secret
    if(0 == liot_file_exist(fileName))
    {
        fd = liot_fopen(fileName, "r");
    }
    else
    {
        fd = liot_fopen(fileName, "wb+");
    }

    result = liot_fread(deviceSecret, sizeof(deviceSecret), 1, fd);
    if (result <= 0)
    {
        result = liot_get_mqtt_devSercite( nSim, cid, productKey, deviceName, productSecret, deviceSecret);
        if(result != 0)
        {
            liot_trace("httpPostData  failed!!");
        }
        else 
        {
            liot_trace(" deviceSecre: %s",deviceSecret);
        }
        result = liot_fwrite(deviceSecret, strlen(deviceSecret), 1, fd);
        if(result != strlen(deviceSecret))
        {
            liot_trace(" LIOT_NV_MQTT_CFG write fail"); 
        }
        result = liot_fclose(fd);
        if (result != LIOT_FS_OK)
        {
            liot_trace("=== close file fail. ret(%d) ===", result);
        }
    }
    
    result = getMqttClientInfo(productKey, deviceName, deviceSecret, clientId, username, password);
    return result;
}
#endif

static void liot_mqtt_event_cb(liot_mqtt_client_t *client, int event_type, void *arg, void *data)
{
    liot_trace("===liot_mqtt_event_cb!!!! = %d",event_type);
    switch(event_type)
    {
        case LIOT_MQTT_OPEN_EVENT:
        case LIOT_MQTT_CONNECT_EVENT:
            {
                int status = *(int *)data;
                if((*client) == mqtt_cli1)
                {
                    if ( status == 0)
                    {
                        mqtt1_connected = 1;
                    }
                    liot_trace("mqtt_cli1 connect result = %d",  status);
                    liot_rtos_semaphore_release(mqtt_semp1);
                }
                else if((*client) == mqtt_cli2)
                {
                    
                    if ( status == 0)
                    {
                        mqtt2_connected = 1;
                    }
                    liot_trace("mqtt_cli2 connect result = %d",  status);
                    liot_rtos_semaphore_release(mqtt_semp2);
                }
                liot_trace("MQTT_OPEN_EVENT data=%d", *(int *)data);
            }
            break;
        case LIOT_MQTT_CLOSE_EVENT:
            {
                if((*client) == mqtt_cli1)
                {
                     mqtt1_connected = 0;
                    liot_trace("mqtt_cli1 disconnected");
                    //liot_rtos_semaphore_release(mqtt_semp1);
                }
                else if((*client) == mqtt_cli2)
                {
                     mqtt2_connected = 0;
                    liot_trace("mqtt_cli2 disconnected");
                    //liot_rtos_semaphore_release(mqtt_semp2);
                }
                liot_trace("MQTT_CLOSE_EVENT data=%d", *(int *)data);
            }
            break;
        case LIOT_MQTT_RECONNECT_EVENT:
            {
                if((*client) == mqtt_cli1)
                {
                    liot_trace("mqtt_cli1 reconnect");
                }
                else if((*client) == mqtt_cli2)
                {
                    liot_trace("mqtt_cli2 reconnect");
                }
                liot_trace("MQTT_RECONNECT_EVENT");
            }
            break;
        case LIOT_MQTT_PUB_EVENT:
        case LIOT_MQTT_SUB_EVENT:
        case LIOT_MQTT_UNSUB_EVENT:
            {
                if((*client) == mqtt_cli1)
                {
                    liot_rtos_semaphore_release(mqtt_semp1);
                    liot_trace("mqtt_requst_result_cb  *client = %p", *client);
                }
                else if((*client) == mqtt_cli2)
                {
                    liot_rtos_semaphore_release(mqtt_semp2);
                    liot_trace("mqtt_requst_result_cb  *client = %p", *client);
                }
                liot_trace("MQTT_PUB_EVENT pkt_id=%d", *(int *)data);
            }
            break;
        case LIOT_MQTT_DISCONNECT_EVENT:
            {
                if((*client) == mqtt_cli1)
                {
                    liot_rtos_semaphore_release(mqtt_semp1);
                    liot_trace("mqtt_disconnect_result_cb  *client = %p", *client);
                }
                else if((*client) == mqtt_cli2)
                {
                    liot_rtos_semaphore_release(mqtt_semp2);
                    liot_trace("mqtt_disconnect_result_cb  *client = %p", *client);
                }
                liot_trace("MQTT_DISCONNECT_EVENT data=%d", *(int *)data);
            }
            break;
        case LIOT_MQTT_PUBLISH_EVENT: 
            {
                liot_mqtt_recvMessage *msg = (liot_mqtt_recvMessage *)data;
                liot_trace("===mqtt_recv_data=====  *client = %p",  *client);
                liot_trace("topic: %s, len: %d", msg->topicName, msg->topicLen);
                liot_trace("payload: %s, len: %d", msg->payload, msg->payloadlen);
                liot_trace("qos: %d", msg->qos);
                liot_trace("retain: %d", msg->retained);
                liot_trace("dup: %d", msg->dup);
                liot_trace("msgid: %d", msg->id);
            }
            break;
        default:
            break;
    }
}
/**
 * @brief Callback function for network indication events.
 * This function is a test function for multi - task network callback.
 * 
 * @param nSim SIM card number.
 * @param ind_type Indication event type.
 * @param ctx Pointer to the context data.
 */
static void liot_nw_ind_callback(uint8_t nSim, unsigned int ind_type, void *ctx)
{
    char csq = 99; // Default signal quality value
    liot_nw_common_reg_status_info_s *liot_nw_msg = NULL;
    liot_nw_nitz_time_info_s *liot_nw_nitz_time_info = NULL;
    liot_trace("nSim=%d, ind_type=%x", nSim, ind_type);
    switch (ind_type)
    {
        case LIOT_NW_SIGNAL_QUALITY_IND:
        {
            csq = *((char *)ctx); // Get the signal quality value
            liot_trace("csq=%d", csq);
        }
        break;

        case LIOT_NW_DATA_REG_STATUS_IND:
        {
            liot_nw_msg = (liot_nw_common_reg_status_info_s *)ctx; // Cast the context pointer
            liot_trace("regState=%d, lac=0x%X, cid=0x%X, act=%d",
                    liot_nw_msg->state,
                    liot_nw_msg->lac,
                    liot_nw_msg->cid,
                    liot_nw_msg->act);
        }
        break;

        case LIOT_NW_NITZ_TIME_UPDATE_IND:
        {
            liot_nw_nitz_time_info = (liot_nw_nitz_time_info_s *)ctx; // Cast the context pointer
            liot_trace(
                "nitz_time=%s, abs_time=%ld", liot_nw_nitz_time_info->nitz_time, liot_nw_nitz_time_info->abs_time);
        }
        break;
    }
}

static int datacall_start()
{
    int ret         = 0;
    int profile_idx = 1;
    liot_data_call_info_t info;
    uint8_t nSim          = 0;

    // Register network callback
    ret = liot_nw_register_cb(liot_nw_ind_callback);
    if (LIOT_NW_SUCCESS != ret)
    {
        liot_trace("liot_nw_register_cb failed, ret is 0x%x.", ret);
    }

    // Try to wait for network registration, up to 10 attempts
    int times = 0;
    while (LIOT_DATACALL_SUCCESS != (ret = liot_network_register_wait(nSim, 120)) && times < 10)
    {
        times++;
        liot_rtos_task_sleep_s(1);
    }

    if (LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("====network register failure!!!!!====");
        return -1;
    }

    liot_trace("===start data call begin====");
    ret = liot_start_data_call(nSim, profile_idx, LIOT_DATA_TYPE_IP, "", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    if (ret != LIOT_DATACALL_SUCCESS)
    {
        liot_trace("liot_start_data_call ret: 0x%x.", ret);
        return -1;
    }
    liot_trace("===start data call end====");

    liot_rtos_task_sleep_s(5);
    ret = liot_get_data_call_info(nSim, profile_idx, &info);
    if (ret != 0)
    {
        liot_stop_data_call(nSim, profile_idx);
        liot_trace("liot_get_data_call_info ret: %d", ret);
        return -1;
    }
    liot_trace("info->cid: %d", info.cid);
    liot_trace("info->ip_version: %d", info.ip_version);

    liot_trace("info->v4.state: %d", info.v4.state);
    // inet_ntop(AF_INET, &info.v4.addr.ip, ip4_addr_str, sizeof(ip4_addr_str));
    // liot_trace("info.v4.addr.ip: %s", ip4_addr_str);

    // inet_ntop(AF_INET, &info.v4.addr.pri_dns, ip4_addr_str, sizeof(ip4_addr_str));
    // liot_trace("info.v4.addr.pri_dns: %s", ip4_addr_str);

    // inet_ntop(AF_INET, &info.v4.addr.sec_dns, ip4_addr_str, sizeof(ip4_addr_str));
    // liot_trace("info.v4.addr.sec_dns: %s", ip4_addr_str);
    return 0;
}

/**
 * @brief   mqtt recv  callback function
 * @param client mqtt client
 * @param arg user data
 * @param pkt_id packet id
 * @param topic recv topic
 * @param payload recv payload
 * @param payload_len recv payload length
 */
static void mqtt_inpub_data_cb(liot_mqtt_client_t *client,
                               void *arg,
                               int pkt_id,
                               const char *topic,
                               const unsigned char *payload,
                               unsigned short payload_len)
{
    liot_trace("topic: %s", topic);
    liot_trace("payload: %s", payload);
}

void liot_mqtt_ali_client1_thread(void *arg)
{
    int ret                         = 0;
    liot_mqtt_client_option options = {0};
    char clientid[CLIENTID_MAXLEN] = {0};
    char username[USERNAME_MAXLEN] = {0};
    char password[PASSWORD_MAXLEN] = {0};
    int mqttState = 0;
    int cid = 1;
    int nSim = 0;
    int free_size = liot_xPortGetFreeHeapSize();
    liot_trace("free size :%d",free_size);
#if LIOT_HTTPC_ENABLE
    ret = liot_get_mqtt_parma(nSim, cid, LIOT_NV_MQTT1_CFG, HTTP_UARL, ALI_PRODUCT_KEY, ALI_DEVICE_NAME1, ALI_PRODUCT_SECRET,
                        clientid, username, password);
    if (ret != 0)
    {
        liot_trace("get mqtt parma failed");
        return; 
    }
#endif
    options.version = LIOT_MQTT_VERSION_4; // 3.1.1
    options.pdp_cid = 1;
    options.ssl_enable = false,
    options.ssl_cfg       = NULL;
    options.clean_session = 1;
    options.kalive_time   = 30;  //mqtt keepalive time unit: second
    options.delivery_time = 5;   //mqtt message delivery time
    options.delivery_cnt  = 3;
    options.will_flag     = 0;
    options.will_qos      = 0;
    options.will_retain   = 0;
    options.ping_timeout  = 5;
    options.client_id     = clientid;
    options.client_user   = username;
    options.client_pass   = password;
    memset(options.will_topic, 0x00, 256);
    memset(options.will_message, 0x00, 256);
    liot_rtos_semaphore_create(&mqtt_semp1, 0);
    int reconnectCnt = 0;
    if (liot_mqtt_client_init_ex(&mqtt_cli1, cid, liot_mqtt_event_cb, NULL) != LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("mqtt client init failed!!!!");
    }

    ret = liot_mqtt_connect(&mqtt_cli1,
                            MQTT_CLIENT_URL,
                            NULL,
                            NULL,
                            (liot_mqtt_client_option *)&options,
                            NULL);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        liot_trace("====wait connect result");
        liot_rtos_semaphore_wait(mqtt_semp1, LIOT_WAIT_FOREVER);
        if (mqtt1_connected == 0)
        {
            liot_mqtt_client_deinit(&mqtt_cli1);
        }
    }
    else
    {
        liot_trace("===mqtt connect failed ,ret = %d", ret);
    }
    while(1)
    {
       // app_show_mem(NULL, __LINE__);
        mqttState = liot_mqtt_client_state(&mqtt_cli1);
        liot_trace("=====MQTT STATE-->>%d", mqttState);
        if(mqttState  == MQTT_CONN_CONNECTED)
        {
            if (liot_mqtt_sub_unsub(&mqtt_cli1, MQTT_ALI_SUB_TOPIC1, 0, NULL, NULL, 1) ==
                LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("======wait subscrible result");
                liot_rtos_semaphore_wait(mqtt_semp1, LIOT_WAIT_FOREVER);
            }

            if (liot_mqtt_publish(&mqtt_cli1,
                                MQTT_ALI_PUB_TOPIC1,
                                MQTT_ALI_PUB_PAYLOAD,
                                strlen(MQTT_ALI_PUB_PAYLOAD),
                                0,
                                0,
                                NULL,
                                NULL) == LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("======wait publish result");
                liot_rtos_semaphore_wait(mqtt_semp1, LIOT_WAIT_FOREVER);
            }


            if (liot_mqtt_sub_unsub(&mqtt_cli1, MQTT_ALI_SUB_TOPIC1, 0, NULL, NULL, 0) ==
                LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("=====wait unsubscrible result");
                liot_rtos_semaphore_wait(mqtt_semp1, LIOT_WAIT_FOREVER);
            }
        }
        //reconnect mqtt
        else if((mqttState == MQTT_CONN_DISCONNECTED) || (mqttState == MQTT_CONN_DEFAULT))
        {
            liot_trace("=====MQTT reconnect");
            ret = datacall_start();
            if (ret != 0) {
                liot_trace("datacall_start failed");
                break;
            }
            reconnectCnt++;
            if (liot_mqtt_client_init(&mqtt_cli1, cid) != LIOT_MQTTCLIENT_SUCCESS)
            {
                liot_trace("mqtt client init failed!!!!");
            }
            ret = liot_mqtt_connect(&mqtt_cli1,
                            MQTT_CLIENT_URL,
                            NULL,
                            NULL,
                            (liot_mqtt_client_option *)&options,
                            NULL);
            liot_trace("=====MQTT eeeee-->> ret: %d", ret);
            if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
            {   
                uint32_t count = 0;
                liot_rtos_semaphore_get_cnt(mqtt_semp1, &count);
                liot_trace("====wait connect result liot_rtos_semaphore_get_cnt = %d", count);
                liot_rtos_semaphore_wait(mqtt_semp1, LIOT_WAIT_FOREVER);
                if (mqtt1_connected == 0)
                {
                    liot_mqtt_client_deinit(&mqtt_cli1);
                }
                else
                {
                    reconnectCnt = 0;
                }
            }
            else
            {
                liot_trace("===mqtt connect failed ,ret = %d", ret);
            }
            if (reconnectCnt > 2)
            {
                liot_trace("===mqtt reconnect failed ,exit");
                break;
            }
        }
        else if(mqttState == MQTT_CONN_RECONNECTING)
        {
            liot_trace("=====MQTT STATE-->> RECONNECTING");
        }
        else
        {
            liot_trace("=====MQTT STATE-->> other: %d", mqttState);
        }
        liot_rtos_task_sleep_s(5);
    }
    if (mqtt1_connected == 1 && liot_mqtt_disconnect(&mqtt_cli1, NULL, NULL) == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        liot_trace("=====wait disconnect result");
        liot_rtos_semaphore_wait(mqtt_semp1, LIOT_WAIT_FOREVER);
    }
    liot_rtos_task_sleep_s(5);
    liot_mqtt_client_deinit(&mqtt_cli1);
    mqtt1_connected = 0;

    liot_rtos_semaphore_delete(mqtt_semp1);
    mqtt_semp1 = NULL;

    liot_rtos_task_sleep_s(5);

    liot_trace("mqtt test end");
    liot_rtos_task_delete(NULL);
    return;
}


void liot_mqtt_ali_client2_thread(void *arg)
{
    int ret                         = 0;
    liot_mqtt_client_option options = {0};
    char clientid[CLIENTID_MAXLEN] = {0};
    char username[USERNAME_MAXLEN] = {0};
    char password[PASSWORD_MAXLEN] = {0};
    int mqttState = 0;
    int cid = 1;
    int nSim = 0;
    
    int free_size = liot_xPortGetFreeHeapSize();
    liot_trace("free size :%d",free_size);
#if LIOT_HTTPC_ENABLE
    ret = liot_get_mqtt_parma(nSim, cid, LIOT_NV_MQTT2_CFG, HTTP_UARL, ALI_PRODUCT_KEY, ALI_DEVICE_NAME2, ALI_PRODUCT_SECRET,
                        clientid, username, password);
    if (ret != 0)
    {
        liot_trace("get mqtt parma failed");
        return; 
    }
#endif
    options.version = LIOT_MQTT_VERSION_4; // 3.1.1
    options.pdp_cid = 1;
    options.ssl_enable = false,
    options.ssl_cfg       = NULL;
    options.clean_session = 1;
    options.kalive_time   = 30;   // mqtt keep alive time, unit: s
    options.delivery_time = 5;    // mqtt message delivery time, unit: s
    options.delivery_cnt  = 3;
    options.will_flag     = 0;
    options.will_qos      = 0;
    options.will_retain   = 0;
    options.ping_timeout  = 5;
    options.client_id     = clientid;
    options.client_user   = username;
    options.client_pass   = password;
    memset(options.will_topic, 0x00, 256);
    memset(options.will_message, 0x00, 256);
    liot_rtos_semaphore_create(&mqtt_semp2, 0);
    int reconnectCnt = 0;
    if (liot_mqtt_client_init_ex(&mqtt_cli2, cid, liot_mqtt_event_cb, NULL) != LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("mqtt client init failed!!!!");
    }

    ret = liot_mqtt_connect(&mqtt_cli2,
                            MQTT_CLIENT_URL,
                            NULL,
                            NULL,
                            (liot_mqtt_client_option *)&options,
                            NULL);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        liot_trace("====wait connect result");
        liot_rtos_semaphore_wait(mqtt_semp2, LIOT_WAIT_FOREVER);
        ret = liot_mqtt_client_is_connected(&mqtt_cli2);
        liot_trace("===mqtt connect, ret = %d", ret);
        if (mqtt2_connected == 0)
        {
            liot_mqtt_client_deinit(&mqtt_cli2);
        }
    }
    else
    {
        liot_trace("===mqtt connect failed ,ret = %d", ret);
    }
    liot_mqtt_set_inpub_callback(&mqtt_cli2, mqtt_inpub_data_cb, NULL);
    while(1)
    {
        //app_show_mem(NULL, __LINE__);
        mqttState = liot_mqtt_client_state(&mqtt_cli2);
        liot_trace("=====MQTT STATE-->>%d", mqttState);
        if(mqttState  == MQTT_CONN_CONNECTED)
        {
            if (liot_mqtt_sub_unsub(&mqtt_cli2, MQTT_ALI_SUB_TOPIC2, 1, NULL, NULL, 1) ==
                LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("======wait subscrible result");
                liot_rtos_semaphore_wait(mqtt_semp2, LIOT_WAIT_FOREVER);
            }

            if (liot_mqtt_publish(&mqtt_cli2,
                                MQTT_ALI_PUB_TOPIC2,
                                MQTT_ALI_PUB_PAYLOAD,
                                strlen(MQTT_ALI_PUB_PAYLOAD),
                                1,
                                0,
                                NULL,
                                NULL) == LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("======wait publish result");
                liot_rtos_semaphore_wait(mqtt_semp2, LIOT_WAIT_FOREVER);
            }


            if (liot_mqtt_sub_unsub(&mqtt_cli2, MQTT_ALI_SUB_TOPIC2, 1, NULL, NULL, 0) ==
                LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("=====wait unsubscrible result");
                liot_rtos_semaphore_wait(mqtt_semp2, LIOT_WAIT_FOREVER);
            }
        }
        //reconnect mqtt
        else if((mqttState == MQTT_CONN_DISCONNECTED) || (mqttState == MQTT_CONN_DEFAULT))
        {
            liot_trace("=====MQTT reconnect");
            ret = datacall_start();
            if (ret != 0) {
                liot_trace("datacall_start failed");
                break;
            }
            reconnectCnt++;
            if (liot_mqtt_client_init(&mqtt_cli2, cid) != LIOT_MQTTCLIENT_SUCCESS)
            {
                liot_trace("mqtt client init failed!!!!");
            }
            ret = liot_mqtt_connect(&mqtt_cli2,
                            MQTT_CLIENT_URL,
                            NULL,
                            NULL,
                            (liot_mqtt_client_option *)&options,
                            NULL);
            liot_trace("=====MQTT eeeee-->> ret: %d", ret);
            if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
            {
                liot_trace("====wait connect result");
                liot_rtos_semaphore_wait(mqtt_semp2, LIOT_WAIT_FOREVER);
                if (mqtt2_connected == 0)
                {
                    liot_mqtt_client_deinit(&mqtt_cli2);
                }
                else
                {
                    reconnectCnt = 0;
                }
            }
            else
            {
                liot_trace("===mqtt connect failed ,ret = %d", ret);
            }
            if (reconnectCnt > 2)
            {
                liot_trace("===mqtt reconnect failed ,exit");
                break;
            }
        }
        else if(mqttState == MQTT_CONN_DISCONNECTED)
        {
            liot_trace("=====MQTT STATE-->> CONNECTING");
        }
        else if(mqttState == MQTT_CONN_RECONNECTING)
        {
            liot_trace("=====MQTT STATE-->> RECONNECTING");
        }
        else
        {
            liot_trace("=====MQTT STATE-->> other: %d", mqttState);
        }
      
        liot_rtos_task_sleep_s(5);

    }


    if (mqtt2_connected == 1 && liot_mqtt_disconnect(&mqtt_cli2, NULL, NULL) == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        liot_trace("=====wait disconnect result");
        liot_rtos_semaphore_wait(mqtt_semp2, LIOT_WAIT_FOREVER);
    }
    liot_rtos_task_sleep_s(5);
    liot_mqtt_client_deinit(&mqtt_cli2);
    mqtt2_connected = 0;

    liot_rtos_semaphore_delete(mqtt_semp2);
    mqtt_semp2 = NULL;

    liot_rtos_task_sleep_s(5);

    liot_trace("mqtt test end");
    liot_rtos_task_delete(NULL);
    return;
}




liot_task_t mqttClient1Task = NULL, mqttClient2Task = NULL;
#define LIOT_MQTT_TASK_PRIORITY APP_PRIORITY_NORMAL
void liot_mqtt_ali_app_thread(void *arg)
{
    int ret  = -1;
    ret = datacall_start();
    if (ret != 0) {
        liot_trace("datacall_start failed");
        liot_rtos_task_delete(NULL);
        return;
    }
    liot_rtos_task_sleep_s(10);
    liot_rtos_task_create(&mqttClient2Task, 1024*6, LIOT_APP_TASK_PRIORITY,"mqtt_ali_client2_thread", &liot_mqtt_ali_client2_thread, NULL);
    liot_rtos_task_sleep_s(3);
    liot_rtos_task_create(&mqttClient1Task, 1024*6, LIOT_APP_TASK_PRIORITY,"mqtt_ali_client1_thread", &liot_mqtt_ali_client1_thread, NULL);
    liot_rtos_task_sleep_s(1);
    while(1)
    {
        liot_rtos_task_sleep_s(20);
        //app_show_mem(NULL, __LINE__);
    }

}