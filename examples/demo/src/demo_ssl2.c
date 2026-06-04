/**
 * @File Name: liot_ssl2_demo.c
 * @brief
 * @Author : lxh email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2026-01-12
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_fs_api.h"
#include "liot_nw.h"
#include "liot_sockets.h"
#include "liot_ssl2.h"

#define LIOT_SSL_LOG liot_trace
#if 1
#define LIOT_TEST_HOST_ADDR "cmp-tcp-monitor.xiot.senthink.com"
#define LIOT_TEST_HOST_PORT 41003

#define LIOT_TEST_SEND_DATA \
    "Hello Lierda\0"
#else
#define LIOT_TEST_HOST_ADDR "220.180.239.212"
#define LIOT_TEST_HOST_PORT 8586
#define LIOT_TEST_SEND_DATA "1234567890123456789012345678901234567890"
#endif

#define LIOT_ROOT_CRT_PATH   "UFS:root.crt"
#define LIOT_CLIENT_KEY_PATH "UFS:client.key"
#define LIOT_CLIENT_CRT_PATH "UFS:client.crt"

#define liot_root_crt \
    "-----BEGIN CERTIFICATE-----\n\
MIIEQjCCAyqgAwIBAgIIUqzcXCgwtxUwDQYJKoZIhvcNAQELBQAwfDELMAkGA1UE\n\
BhMCQ04xDzANBgNVBBETBjMxMTEyMTEZMBcGA1UEChMQTGllcmRhIEdyb3VwIElu\n\
YzEfMB0GA1UEAxMWTGllcmRhIFJTQSBDQSBUTFMgMjAyMzEgMB4GCSqGSIb3DQEJ\n\
AQwRZGV2b3BzQGxpZXJkYS5jb20wIBcNMjMwMzIzMDM1NzEwWhgPMjEyMzAzMjMw\n\
MzU3MTBaMHwxCzAJBgNVBAYTAkNOMQ8wDQYDVQQREwYzMTExMjExGTAXBgNVBAoT\n\
EExpZXJkYSBHcm91cCBJbmMxHzAdBgNVBAMTFkxpZXJkYSBSU0EgQ0EgVExTIDIw\n\
MjMxIDAeBgkqhkiG9w0BCQEMEWRldm9wc0BsaWVyZGEuY29tMIIBIjANBgkqhkiG\n\
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA7Km6RoA8W+ABfVi4Su5CBXXXX03W4tRft2CU\n\
PL2WxSXmt+NLa6qVpXwdXhnOFka7W3Yky7sJsEa+4L3ZAdvLAaV+T/YWjWUs4kBS\n\
g4m1qvNbVBf+QRMnN7T4KDykAmVt9slk5hrl9wmaza2uC0mVybCus1hWFl4O6bEK\n\
NVfVj67RMYJ0/V8rGo1RaM9gY9ocfsvCWnfmoBCBWVJqgKHt3zw3eVdgiK1c3/ZD\n\
Uksl+YEnHc0xZpUp++K/xn5BdgWJPY/5Xe/YGOMGXIJqINoW9br2ZKXIGsWmByus\n\
LltumLIThO6fnP2cyJjhJM+UnTUGJ/38tk7zjp2NnFQFFEjhlQIDAQABo4HFMIHC\n\
MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEw\n\
DwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQU3amZSimWqD7Ws6eB19/JaiRbJMQw\n\
KwYIKwYBBQUHAQEEHzAdMBsGCCsGAQUFBzAChg94aW90LmxpZXJkYS5jb20wNAYD\n\
VR0RBC0wK4IWTGllcmRhIFJTQSBDQSBUTFMgMjAyM4ERZGV2b3BzQGxpZXJkYS5j\n\
b20wDQYJKoZIhvcNAQELBQADggEBADGONA83hjydoGGSfbAF9KLDYUXGqLuAk3w+\n\
hZSt22x/nrq24TTGp/nVdhPs1CWNKzm/E0J+PpR/igWE/wYoLPCBZp1Yw5Lr+Gsu\n\
1dREldvcrnwCD6lGBk+liL3e/JTa7XTpoN+gYbpr9LVzPPlSwbFv8HXUQpoKCCXl\n\
A3L2TgoXCWKmW/TlPveT4eeSBPKif03hA6LsIFC5ebKztIe8cLRQiC+nuVg+vgOn\n\
L2o7gudAW9oeLHHwaZaJ862g0QlzhP+xKbK5vF2hRVpuE3VOWXFmUiWOhPw9BBgb\n\
6qrLAy26wFxdlnE8Q5yJcNGdJHk2d7TnnCohghLOjdhfE5lUWGc=\n\
-----END CERTIFICATE-----\n\
-----BEGIN CERTIFICATE-----\n\
MIIElTCCA32gAwIBAgIIEv+6yZm7eTMwDQYJKoZIhvcNAQELBQAwfDELMAkGA1UE\n\
BhMCQ04xDzANBgNVBBETBjMxMTEyMTEZMBcGA1UEChMQTGllcmRhIEdyb3VwIElu\n\
YzEfMB0GA1UEAxMWTGllcmRhIFJTQSBDQSBUTFMgMjAyMzEgMB4GCSqGSIb3DQEJ\n\
AQwRZGV2b3BzQGxpZXJkYS5jb20wIBcNMjMwMzIzMDQ0MDU4WhgPMjEyMzAyMjMw\n\
NDQwNThaMIGCMQswCQYDVQQGEwJDTjEPMA0GA1UEERMGMzExMTIxMQ8wDQYDVQQK\n\
EwZMaWVyZGExETAPBgNVBAsTCFNlbnRoaW5rMRwwGgYDVQQDDBMqLnhpb3Quc2Vu\n\
dGhpbmsuY29tMSAwHgYJKoZIhvcNAQkBDBFkZXZvcHNAbGllcmRhLmNvbTCCASIw\n\
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAOZp21VtvMnztvuhLiBUGD8rZXcP\n\
5Co3WXMM1All0MvRxrWPMxMUUlvhfXCbq8KEizjpirnjZEMroPdNj7yDS7Ua4wQ9\n\
NfCMKy/xpZFRRfCAFEZmMQnbA3RDGZbIMtc2sWv9DbzEKNmxFbSd52aEFT08qpjC\n\
tToQpFzDsmdNGGW/kT/3hjBMPsB/MusREh93V8QAc+91bsMPhadQbhpZ+KfDXkO2\n\
yqosbjttywmKh53nCTrjWz+ti8rXEGcLOhUEIinMeEGFFSql9X5AUGEE3hAVw4T4\n\
N9JD5xGSW5xoiVI81zwjOF1I0ytfcnuxyoODQ7pMfdMnGh10wGwHvMsDLNsCAwEA\n\
AaOCARAwggEMMA4GA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYI\n\
KwYBBQUHAwIwHQYDVR0OBBYEFIdFQk2DWZPhKuxN75FM9ru8TsvQMB8GA1UdIwQY\n\
MBaAFN2pmUoplqg+1rOngdffyWokWyTEMCsGCCsGAQUFBwEBBB8wHTAbBggrBgEF\n\
BQcwAoYPeGlvdC5saWVyZGEuY29tMG4GA1UdEQRnMGWCEyoueGlvdC5zZW50aGlu\n\
ay5jb22CESoueGlvdC5saWVyZGEuY29tghMqLnRlc3Quc2VudGhpbmsuY29tghMq\n\
LnN0YWcuc2VudGhpbmsuY29tgRFkZXZvcHNAbGllcmRhLmNvbTANBgkqhkiG9w0B\n\
AQsFAAOCAQEABF/qYc9/+9YhabnGLpUGSYtbht1fXHM9LOc1yqaoJbkP6tTxdrex\n\
kewzR5N4bLV+sl8QirsKZjxBmmnyjjBFJE9wnmeGiEr3pRO3MjCxjbQC/hLNjGNj\n\
fvPrxmAEyOcrp6drYGPum49p4/ecXRlM5vNKYXZSEsm2hxU2Mp1xcPVZSU4ttzoQ\n\
73BbdO1SYgPqYeQjw4sawqTW60aurDZNzW5yMnkh56320kjcy1DwI6xYW4q7HPnI\n\
8lmgl1ARntMlAQPCi77aLid91sxPFttfnH+NAe4UTjL6B7xCD2RqpiJRb3DyFaUl\n\
L3ZMQQC9ylYlcmYE6RUNKgVZ7WaJLPzl4g==\n\
-----END CERTIFICATE-----"

#define liot_client_crt \
"-----BEGIN CERTIFICATE-----\n\
MIIEizCCA3OgAwIBAgIIEaRUDbqjWgEwDQYJKoZIhvcNAQELBQAwfDELMAkGA1UE\n\
BhMCQ04xDzANBgNVBBETBjMxMTEyMTEZMBcGA1UEChMQTGllcmRhIEdyb3VwIElu\n\
YzEfMB0GA1UEAxMWTGllcmRhIFJTQSBDQSBUTFMgMjAyMzEgMB4GCSqGSIb3DQEJ\n\
AQwRZGV2b3BzQGxpZXJkYS5jb20wIBcNMjMwMzIzMDQ0MjU1WhgPMjEyMzAyMjMw\n\
NDQyNTVaMIGCMQswCQYDVQQGEwJDTjEPMA0GA1UEERMGMzExMTIxMQ8wDQYDVQQK\n\
EwZMaWVyZGExETAPBgNVBAsTCFNlbnRoaW5rMRwwGgYDVQQDDBMqLnhpb3Quc2Vu\n\
dGhpbmsuY29tMSAwHgYJKoZIhvcNAQkBDBFkZXZvcHNAbGllcmRhLmNvbTCCASIw\n\
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAPIGB6UVDXHwiVEfeUTo+zKMzz4c\n\
MDPfKTpRciDLmmg27blh5+jVBC7xMNaM5ran6GjF7JuYO9EipYPCMv5WnfyPzAU3\n\
HFDbynaGhPLE/ZTs71PU8fPgK7M8WxesPCDJ5Q9NOkIGq+PtI2pHMRMaDUOjMEGE\n\
ftm7VGSMwfE+dQTeOBwDNoQ6ENTVg0a5yfy+SXgKXW3QI9flW//Wagw6WLjIVeYQ\n\
ffXc8IuJHmlYtaYrFeZaL1tRXoscnlFRUMdB60XIq5JOyxqFtPEYgv8LwYDgozpW\n\
2jJTpH+Tp/+Sxc2rQHO56XCx1Z51W3Ia7Q30Mu1WJHYsLphdWeeMlkXT+eECAwEA\n\
AaOCAQYwggECMA4GA1UdDwEB/wQEAwIHgDATBgNVHSUEDDAKBggrBgEFBQcDAjAd\n\
BgNVHQ4EFgQUBqMN+wB9BUK01w1HHL+ZonufeOowHwYDVR0jBBgwFoAU3amZSimW\n\
qD7Ws6eB19/JaiRbJMQwKwYIKwYBBQUHAQEEHzAdMBsGCCsGAQUFBzAChg94aW90\n\
LmxpZXJkYS5jb20wbgYDVR0RBGcwZYITKi54aW90LnNlbnRoaW5rLmNvbYIRKi54\n\
aW90LmxpZXJkYS5jb22CEyoudGVzdC5zZW50aGluay5jb22CEyouc3RhZy5zZW50\n\
aGluay5jb22BEWRldm9wc0BsaWVyZGEuY29tMA0GCSqGSIb3DQEBCwUAA4IBAQDP\n\
tC24Uw1ud4wO2C3qWHZluLrNtsIVcD5tLcYMlX2oa3uuNaI9fsvFEPikf8LI7LYU\n\
NKQVBbeKpnIfBCtrmarj6jOQumSZXKC02f3w52Z1BFGvMj8GNj8ts7EHMqe8yohJ\n\
3lsoReAb0VDPerExFX/uhkmxd4OEQatpvjLR8nKrjlvq2sNQ+SrQALvM3/iVQ0Vo\n\
ld3qaIy/rpg+SRJeSmoh3tPl18H5CEqQYImWs5G9bKS7eFCj4Rr/a+4NeRwaV/0y\n\
hwt7V7AaWKdpwEpRoV3ecxIdyIod313T18ndIDLIWlcVTllLA3Y60f6G8J4Lp9Gs\n\
Utn0AVYamn42u8y+Itjh\n\
-----END CERTIFICATE-----"

#define liot_client_key \
"-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpAIBAAKCAQEA8gYHpRUNcfCJUR95ROj7MozPPhwwM98pOlFyIMuaaDbtuWHn\n\
6NUELvEw1ozmtqfoaMXsm5g70SKlg8Iy/lad/I/MBTccUNvKdoaE8sT9lOzvU9Tx\n\
8+ArszxbF6w8IMnlD006Qgar4+0jakcxExoNQ6MwQYR+2btUZIzB8T51BN44HAM2\n\
hDoQ1NWDRrnJ/L5JeApdbdAj1+Vb/9ZqDDpYuMhV5hB99dzwi4keaVi1pisV5lov\n\
W1FeixyeUVFQx0HrRcirkk7LGoW08RiC/wvBgOCjOlbaMlOkf5On/5LFzatAc7np\n\
cLHVnnVbchrtDfQy7VYkdiwumF1Z54yWRdP54QIDAQABAoIBAQCHibTyUG3tJ0Xf\n\
dn7VhpxW3OCa6fJculc+X6pIj31za3+KWDgl/lWPbGdrYBE8rZ/MPTuaj5DAOGT0\n\
+ubP5PYpIOai7RAggFZMNl1yD7vrkR0el5zAdQREJkLlaQFrY2WyQg+kNlRBGubA\n\
eAoVpntTK6q0plzb1rsWbHWYgt/JzwpU078m2BnF0PG6sqZWRPjSqpMBHkkKPn52\n\
nThKlTL2/XfpahxWnN+j3Gq7eSI3t7XXopHBOxSPbPa0Hx+jLLPfxnRihzhIb9IZ\n\
r7CCBhGFrab+/VbOJc+FUstTGEd/2tO2+AbqZnonUTPgkPMMPrWGqCKlsIoUxwqV\n\
lS1B+/FZAoGBAPmZAp7cFMp3DWqd8NiR4EutrH2tfOoK3zAt1S7ErVmiS+UKBaZ+\n\
WcWnUzn27DZ3uslnB2aIQ8ZFaunM6NkdG7jkpNQpw2myFTK7Vd5VsqUHjPNJwPhT\n\
xjAdxvBTED3c12U3xlxmfgob9Mv3EwmseH+AZPGcojw7Vg8VbeZDsjw7AoGBAPg7\n\
SKjFA4ZpiWEiELWl/i5IrQsuLINM/hM7fAPNCzFrN2/ZVfqoe6kbkII5c8DIgU6i\n\
5LxTkgaEYC3ne8pGjnB0WN1xI/nbqtg3ZNrN4S9ArlEkzHiI9r0xsnyRUt99wAlB\n\
3jjOPBPXzFRgDy/VXoE1k4OUrCPnwV1JVwM2j+yTAoGAGu0UyUp4u7bILb5IIzTp\n\
e4CK4Sv8E8uEdFAddWQmR0so4NaJ8YIj8Ryx78jhhqa38c45kCgXF6aZeAtw+ysW\n\
uIWB+pleyWI6wzROeSPJl+AOpphIRwHSmZI8yeRMFkBixDqWHZW49yI48GABH8QJ\n\
Y8fYG/CH1/OWvx3Ss0qOSc0CgYB5UohVrEznwecaKUsfbOzJdE3XHA83cUbw4W7g\n\
cLPayHtE2GV0aCpgYY+Ehk3z2EuSwnynKvVA6CwV44CWQVvWzJauTGD3+owKtzA8\n\
3MkFjkhkcmbZNp/ZolOJHIHzp4NXQvE4tFuamytLxdNGXrOc1TGu6tgjiF7ymCjF\n\
8kHvrwKBgQCpbYvfsn9T2hE1jyK5Qh7P8XqSprQ7cZ05VYhDbMZ8iOn2LWXJLf1j\n\
DQEUjO44VzU4IRb2f0hiw7SYMjPbNh+rKkjUxCdLa5RvJ9CWT9xddjQZHjfv1Px2\n\
I2H+OQwOo8QOUXH6z8gUAw8MsNYiDlvtRw/6GFAkc4D5A25UzVBmcA==\n\
-----END RSA PRIVATE KEY-----"

#define DEMO_SSL_CLIENT_ID 1
#define DEMO_SEND_INTERVAL 60000 // 发送间隔，单位：毫秒
#define LIOT_SSL_LOG liot_trace

Liot_sslClientInfo_t ssl_client_info;

/**
 * ssl recv callback
 * @param[in] clientId ssl client id
 * @param[in] data received data
 * @param[in] arg user argument
 * @return none
 */
void ssl_recv_callback(uint8_t clientId, void *data, uint32_t dataLen, void *arg)
{
    LIOT_SSL_LOG("Received data: %d.*%s", dataLen, (char *)data);
}

/**
 * ssl client init
 * @return none
 */
void liot_ssl_init(void)
{
    memset(&ssl_client_info, 0, sizeof(ssl_client_info));

 
    ssl_client_info.sslClientId = DEMO_SSL_CLIENT_ID;
    ssl_client_info.socket_type = 1; // TCP
    ssl_client_info.host = (uint8_t *)LIOT_TEST_HOST_ADDR;
    ssl_client_info.port = LIOT_TEST_HOST_PORT;
    if(ssl_client_info.ssl_context == NULL)
    {
        ssl_client_info.ssl_context = liot_rtos_malloc(sizeof(Liot_sslContext_t));
        if (ssl_client_info.ssl_context == NULL)
        {
            LIOT_SSL_LOG("Failed to allocate memory for SSL context");
            return;
        }
    }
    memset(ssl_client_info.ssl_context, 0, sizeof(Liot_sslContext_t));

    ssl_client_info.ssl_context->auth_type = LIOT_SSL_VERIFY_CLIENT_SERVER;  // 
    ssl_client_info.ssl_context->ca_cert = (uint8_t *)liot_root_crt;
    ssl_client_info.ssl_context->ca_cert_length = strlen(liot_root_crt) + 1;
    ssl_client_info.ssl_context->user_cert = (uint8_t *)liot_client_crt;
    ssl_client_info.ssl_context->user_cert_length = strlen(liot_client_crt) + 1;
    ssl_client_info.ssl_context->user_key = (uint8_t *)liot_client_key;
    ssl_client_info.ssl_context->user_key_length = strlen(liot_client_key) + 1;
    ssl_client_info.ssl_context->ssl_version = LIOT_SSL_VERSION_3; // TLS 1.2
    ssl_client_info.ssl_context->r_timeout = 10; 
    ssl_client_info.ssl_context->sni_support = 1;
    ssl_client_info.ssl_context->ciphersuite[0] =0XFFFF ;


    ssl_client_info.KeepaliveEn = 1; // enable Keepalive
    ssl_client_info.keepidle = 30;   
    ssl_client_info.keepinterval = 10; 
    ssl_client_info.keepcount = 5;   

    ssl_client_info.sslClient_cb = ssl_recv_callback;

    int ret = Liot_SSLSetCfg(&ssl_client_info);
    if (ret != LIOT_SSL_SUCCESS)
    {
        LIOT_SSL_LOG("Liot_SSLSetCfg failed, ret: %d", ret);
        liot_rtos_free(ssl_client_info.ssl_context);
        return;
    }
}

/**
 *  ssl test
 * @param[in] profile_idx profile index
 * @return none
 */
static int liot_ssl_test(int profile_idx)
{
    liot_data_call_info_t info;
    int ret = LIOT_SSL_SUCCESS;
    int i = 0;
    int ssl_status = 0; 
    // Get data call information for the specified profile
    liot_get_data_call_info(0, profile_idx, &info);

start:

    liot_ssl_init();
    
    ret = Liot_SSLSocketOpen(DEMO_SSL_CLIENT_ID);
    if (ret != LIOT_SSL_SUCCESS)
    {
        LIOT_SSL_LOG("Liot_SSLSocketOpen failed, ret: %d", ret);
        liot_rtos_free(ssl_client_info.ssl_context);
        return LIOT_SSL_ERROR_UNKOWN;
    }

    while (1)
    {
        ssl_status = Liot_SSLSocketGetStatus(DEMO_SSL_CLIENT_ID);
        LIOT_SSL_LOG("Current SSL status: %d", ssl_status);
        const char *send_data = LIOT_TEST_SEND_DATA;
        ret = Liot_SSLSocketSend(DEMO_SSL_CLIENT_ID, (uint8_t *)send_data, strlen(send_data));
        if (ret < 0)
        {
            LIOT_SSL_LOG("Liot_SSLSocketSend failed, ret: %d", ret);
            ret = LIOT_SSL_ERROR_SEND_FAILURE;
            break;
        }
        LIOT_SSL_LOG("Data sent: %s", send_data);
        i++;
        if (i >= 3)
        {
            break;
        }
        ssl_status = Liot_SSLSocketGetStatus(DEMO_SSL_CLIENT_ID);
        LIOT_SSL_LOG("Current SSL status: %d", ssl_status);
        liot_rtos_task_sleep_ms(DEMO_SEND_INTERVAL);
    }
    if(ssl_status == LIOT_SSL_CLIENT_STATUS_CLOSED)
    {
        LIOT_SSL_LOG("SSL is closed, start reconnecting!! : %d", ssl_status);
        goto start;
    }
    Liot_SSLSocketClose(DEMO_SSL_CLIENT_ID);
    liot_rtos_free(ssl_client_info.ssl_context);
    LIOT_SSL_LOG("SSL demo task finished");
    return ret;
}

/**
 * @brief Main SSL application thread.
 * This thread handles the complete SSL demo workflow including:
 * 1. Establishing data connection
 * 2. Initializing certificates
 * 3. Performing SSL/TLS operations
 * 4. Cleaning up resources
 * 
 * @param arg Thread arguments (unused)
 */
void liot_ssl2_demo_thread(void *arg)
{
    int ret         = 0;
    int profile_idx = 1;
    liot_data_call_info_t info;
   // char ip4_addr_str[16] = {0};
    uint8_t nSim          = 0;

    LIOT_SSL_LOG("========== ssl demo start ==========");

    LIOT_SSL_LOG("===start data call begin====");
    liot_start_data_call(nSim, profile_idx, LIOT_DATA_TYPE_IP, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    LIOT_SSL_LOG("===start data call end====");

    ret = liot_get_data_call_info(nSim, profile_idx, &info);
    if (ret != 0)
    {
        liot_stop_data_call(nSim, profile_idx);
        liot_trace("liot_get_data_call_info ret: %d", ret);
        goto exit;
    }
    liot_trace("info->cid: %d", info.cid);
    liot_trace("info->ip_version: %d", info.ip_version);

    ret = liot_ssl_test(profile_idx);
    liot_trace("liot_ssl_test ret: %d", ret);
exit:
    liot_rtos_task_delete(NULL);
    return;
}
