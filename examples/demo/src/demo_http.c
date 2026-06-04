/**
 * @File Name: liot_http_demo.c
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 * @note  The function of this file is to implement simple HTTP post and get functions using the HTTP API interface and Lierda's test server.
 * The test server is used to verify the correctness of the HTTP API interface.
 * The test server is only used for testing purposes.
 * Any use of the test server is at your own risk.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_fs_api.h"
#include "liot_http.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_sockets.h"

#define LIOT_HTTP_DEMO_SSL_TEST  0


#define LIOT_HTTP_POST_URL "http://cmp-net-monitor.xiot.senthink.com/pushData-1"
#define LIOT_HTTP_GET_URL  "http://cmp-net-monitor.xiot.senthink.com/getPushData-1"

static liot_sem_t http_semp;

#if LIOT_HTTP_DEMO_SSL_TEST
#include "liot_ssl.h"

char *liot_http_ca_cert_path     = "UFS:root.crt";
char *liot_http_user_cert_path   = "UFS:client.crt";
char *liot_http_user_key_path    = "UFS:client.key";


char *liot_http_ca_cert_data =
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
-----END CERTIFICATE-----\n";

char *liot_http_user_cert_data  = 
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
-----END CERTIFICATE-----\n";

char *liot_http_user_key_data =
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
-----END RSA PRIVATE KEY-----\n";


/**
 * @brief https ca cert init 
 * @return int 
 */
static int http_ca_cert_init(void)
{
    int ret  = 0;
    LFILE fp = -1;

    if (liot_file_exist(liot_http_ca_cert_path) != LIOT_FS_NOT_EXIST)
    {
        liot_remove(liot_http_ca_cert_path);
    }
    fp = liot_fopen(liot_http_ca_cert_path, "w+");
    if (fp < 0)
    {
        liot_trace("open file:%s failed!!!!", liot_http_ca_cert_path);
        return -1;
    }
    /* write ca cert */
    ret = liot_fwrite(liot_http_ca_cert_data, strlen(liot_http_ca_cert_data), 1, fp);
    liot_trace("%s:%d", __FUNCTION__, __LINE__);
    liot_trace("ret:%d", ret);
    if (ret != strlen(liot_http_ca_cert_data))
    {
        liot_trace("ret:%d", ret);
        liot_fclose(fp);
        return -1;
    }
    liot_fclose(fp);
    return 0;
}

/**
 * @brief https user cert init
 * @return int 
 */
static int http_user_cert_init(void)
{
    int ret  = 0;
    LFILE fp = -1;

    if (liot_file_exist(liot_http_user_cert_path) != LIOT_FS_NOT_EXIST)
    {
        liot_remove(liot_http_user_cert_path);
    }
    fp = liot_fopen(liot_http_user_cert_path, "w+");
    if (fp < 0)
    {
        liot_trace("open file:%s failed!!!!", liot_http_user_cert_path);
        return -1;
    }
    /* write user cert */
    ret = liot_fwrite(liot_http_user_cert_data, strlen(liot_http_user_cert_data), 1, fp);
    liot_trace("%s:%d", __FUNCTION__, __LINE__);
    liot_trace("ret:%d", ret);
    if (ret != strlen(liot_http_user_cert_data))
    {
        liot_trace("ret:%d", ret);
        liot_fclose(fp);
        return -1;
    }
    liot_fclose(fp);
    return 0;
}

/**
 * @brief https user key init
 * @return int 
 */
static int http_user_key_init(void)
{
    int ret  = 0;
    LFILE fp = -1;

    if (liot_file_exist(liot_http_user_key_path) != LIOT_FS_NOT_EXIST)
    {
        liot_remove(liot_http_user_key_path);
    }
    fp = liot_fopen(liot_http_user_key_path, "w+");
    if (fp < 0)
    {
        liot_trace("open file:%s failed!!!!", liot_http_user_key_path);
        return -1;
    }
    /* write user key */
    ret = liot_fwrite(liot_http_user_key_data, strlen(liot_http_user_key_data), 1, fp);
    liot_trace("%s:%d", __FUNCTION__, __LINE__);
    liot_trace("ret:%d", ret);
    if (ret != strlen(liot_http_user_key_data))
    {
        liot_trace("ret:%d", ret);
        liot_fclose(fp);
        return -1;
    }
    liot_fclose(fp);
    return 0;
}
#endif

char *liot_http_upload_path = "UFS:http_test.txt";
char *liot_http_upload_data = "http_upload_test";
int liot_http_upload_len    = 0;

/**
 * @brief http upload file init
 * 
 * @return int 
 */
static int http_uploadfile_init(void)
{
    int ret  = 0;
    LFILE fp = -1;

    if (liot_file_exist(liot_http_upload_path) != LIOT_FS_NOT_EXIST)
    {
        liot_remove(liot_http_upload_path);
    }
    fp = liot_fopen(liot_http_upload_path, "w+");
    if (fp < 0)
    {
        liot_trace("open file:%s failed!!!!", liot_http_upload_path);
        return -1;
    }
    ret = liot_fwrite(liot_http_upload_data, strlen(liot_http_upload_data), 1, fp);
    liot_trace("%s:%d", __FUNCTION__, __LINE__);
    liot_trace("ret:%d,%s", ret, liot_http_upload_data);
    liot_http_upload_len = liot_fsize(fp);
    liot_trace("%s:%d  liot_http_upload_len:%d", __FUNCTION__, __LINE__, liot_http_upload_len);
    if (ret != strlen(liot_http_upload_data))
    {
        liot_trace("ret:%d", ret);
        liot_fclose(fp);
        return -1;
    }
    liot_fclose(fp);
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

static int http_response_write_data_cb(liot_http_client_t *client, void *arg, char *data, int size, unsigned char end)
{
    liot_trace(">>>>>client:%p", client);
    liot_trace("===http_response_write_data_cb=== size:%d", size);
    unsigned char *recv_buf = NULL;
    recv_buf                = malloc(size + 1);
    if (recv_buf != NULL)
    {
        memset(recv_buf, 0, size + 1);
        memcpy(recv_buf, data, size);
        liot_trace("===http_response_write_data_cb=== recv_buf:%s", recv_buf);
        liot_rtos_free(recv_buf);
        recv_buf = NULL;
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
static int http_request_read_data_cb(liot_http_client_t *client, void *arg, char *data, int size)
{
    int ret  = 0;
    LFILE fp = -1;
    liot_trace(">>>>>client:%p", client);

    if (client == NULL)
    {
        return 0;
    }

    fp = liot_fopen(liot_http_upload_path, "r+");
    if (fp < 0)
    {
        liot_trace("open file:%s failed!!!!", liot_http_upload_path);
        return 0;
    }
    ret = liot_fread(data, size, 1, fp);
    liot_fclose(fp);
    liot_trace("http_request_read_data_cb  data:%s", data);
    liot_trace("http_request_read_data_cb  size:%d", ret);
    return ret;
}

/**
 * @brief network init
 * @param nSim sim card id
 * @param cid data call id
 * @param ip_version ip version
 * @param apn_name apn name
 * @param username username
 * @param password password
 * @param auth_type auth type
 * @return true if success network init success, false if network init failed
 */
bool liot_network_init(UINT8 nSim, INT32 cid, INT32 ip_version, CHAR *apn_name, CHAR *username, CHAR *password, INT32 auth_type)
{
    liot_data_call_info_t info;
    int times = 0;
    int ret                        = 0;
    while (LIOT_DATACALL_SUCCESS != (ret = liot_network_register_wait(nSim, 120)) && times < 10)
    {
        times++;
        liot_rtos_task_sleep_s(1);
    }
    if (LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("====network register failure!!!!!====");
        if (LIOT_OSI_SUCCESS != liot_rtos_task_delete(NULL))
        {
            liot_trace("task deleted failed");
        }
        return  false;
    }

    ret = liot_start_data_call(nSim, cid, ip_version, apn_name, username, password, auth_type);
    if (ret != 0)
    {
        liot_trace("====data call failure!!!!=====");

    }

    // liot_rtos_task_sleep_ms(1000);

    ret = liot_get_data_call_info(nSim, cid, &info);
    if (ret != 0)
    {
        liot_trace("liot_get_data_call_info ret: %d", ret);
        liot_stop_data_call(nSim, cid);
        return false;;
    }


    liot_trace("info->profile_idx: %d", info.cid);
    liot_trace("info->ip_version: %d", info.ip_version);

    liot_trace("info->v4.state: %d", info.v4.state);

    liot_ip4addr_ntoa(&info.v4.addr.ip);
    liot_trace("info.v4.addr.ip: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.ip));

    liot_ip4addr_ntoa(&info.v4.addr.pri_dns);
    liot_trace("info.v4.addr.pri_dns: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.pri_dns));

    liot_ip4addr_ntoa(&info.v4.addr.sec_dns);
    liot_trace("info.v4.addr.sec_dns: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.sec_dns));
    return true;
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
    char *ca_cert_path           = liot_http_ca_cert_path;

    http_ca_cert_init();
    http_user_cert_init();
    http_user_key_init();

    liot_ssl_user_cert_s user_cert = {liot_http_user_cert_path, liot_http_user_key_path, NULL};  //config  user cert and user key

    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSLCTXID, 1);      // config ssl id
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERSION, LIOT_SSL_VERSION_3);     // ssl version
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_HS_TIMEOUT, hs_timeout);
    // liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_SNI, 0);

    liot_trace("%s:%d  verify_level:%d ", __FUNCTION__, __LINE__, verify_level);
    if (verify_level == LIOT_HTTPS_VERIFY_SERVER)
    {
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL, LIOT_HTTPS_VERIFY_SERVER);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_CACERT_PATH, ca_cert_path);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_INVALID_CERT_SIGN, 1);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_CERT_ITEM, MBEDTLS_X509_BADCERT_FUTURE);
    }
    else if (verify_level == LIOT_HTTPS_VERIFY_SERVER_CLIENT) 
    {
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL, LIOT_HTTPS_VERIFY_SERVER_CLIENT);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_CACERT_PATH, ca_cert_path);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_OWNCERT_PATH, user_cert);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_INVALID_CERT_SIGN, 1);
        // liot_httpc_setopt(client,
        //                   LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_CERT_ITEM,
        //                   MBEDTLS_X509_BADCERT_FUTURE | MBEDTLS_X509_BADCERT_NOT_TRUSTED);
    }
    liot_trace(">>>>>verify_level:%d ", verify_level);
    return true;
}
#endif

/**
 * @brief    http request data
 * @param nSim  sim id 
 * @param cid  data call id
 * @param client  http client
 * @param httpHost  http host
 * @param http_method  http method
 * @return int 
 */
int liot_http_requeset_data(uint8_t nSim, uint8_t cid, liot_http_client_t *client, char *httpHost, int http_method)
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
    if (http_method == LIOT_HTTPC_METHOD_POST)
    {  
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, "Content-type: application/json");
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_READ_FUNC, http_request_read_data_cb);
        // liot_trace("%s:%d  %p  %p", __FUNCTION__, __LINE__,&http_client,(void *)&http_client);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_READ_DATA, client);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN, liot_http_upload_len);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_BODY_DATA_TYPE, LIOT_HTTPC_FORM_DATA);
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_RAW_REQUEST, 0);
        liot_httpc_formadd(client, LIOT_HTTP_FORM_NAME, "file");
        liot_httpc_formadd(client, LIOT_HTTP_FORM_FILENAME, "http_upload.txt", liot_http_upload_len);
        liot_httpc_formadd(client, LIOT_HTTP_FORM_CONTENT_TYPE, "text/plain");
    }

    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SIM_ID, nSim);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_PDPCID, cid);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_METHOD, http_method);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_URL, url);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, http_response_write_data_cb);

#if LIOT_HTTP_DEMO_SSL_TEST
    if(url->scheme != LIOT_HTTPC_SCHEME_HTTPS)
    {
        liot_trace("url->scheme =%s", url->scheme);
        return -1;
    }
    if(liot_https_param_cfg(client, LIOT_HTTPS_VERIFY_SERVER_CLIENT) == false)
    {
        liot_trace("liot_https_param_cfg failed!!!!");
        return -1;
    }
#endif
    return 0;
}

void liot_http_demo_thread(void *arg)
{
    uint8_t cid     = 1;
    uint8_t nSim    = 0;
    int i = 0;
    char* httpUrl = NULL;
    do 
    {
        http_uploadfile_init();
        liot_rtos_semaphore_create(&http_semp, 0);

        if(liot_network_init(nSim, cid, LIOT_DATA_TYPE_IP, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE) == false)
        {
            liot_trace("http liot_network_init failed!!!!");
            break;
        }
        if (httpUrl == NULL)
        {
            httpUrl = liot_rtos_malloc(256);
            if(httpUrl == NULL)
            {
                break;
            }
        }
       
        for(i = 0; i < 30; i++)
        {
            int http_method       = LIOT_HTTPC_METHOD_POST;
            liot_http_client_t http_client = 0;
 
            if (liot_httpc_new(&http_client, http_event_cb, NULL) != LIOT_HTTPC_SUCCESS)
            {
                liot_trace("http client create failed!!!!");
                break;
            }
            memset(httpUrl, 0, 256);
            if(http_method == LIOT_HTTPC_METHOD_GET)   // HTTP GET 
            {
                memcpy(httpUrl, LIOT_HTTP_GET_URL, strlen(LIOT_HTTP_GET_URL));
            }
            else if (http_method == LIOT_HTTPC_METHOD_POST)   // HTTP POST
            {
                memcpy(httpUrl, LIOT_HTTP_POST_URL, strlen(LIOT_HTTP_POST_URL));
            }
            liot_trace("http url:%s", httpUrl);
            liot_http_requeset_data(nSim, cid, &http_client, httpUrl, http_method);  //http request param config

            if(liot_httpc_perform(&http_client) == LIOT_HTTPC_SUCCESS)
            {
                liot_rtos_semaphore_wait(http_semp, LIOT_WAIT_FOREVER);
                liot_trace("liot_rtos_semaphore_wait");
            }
            else
            {
                liot_trace("http perform client fail");
            }
            liot_trace("liot_rtos_semaphore_wait!!!!");
            liot_httpc_stop(&http_client);
            liot_rtos_semaphore_wait(http_semp, LIOT_WAIT_FOREVER);
            liot_httpc_release(&http_client);
            liot_rtos_task_sleep_s(10);
        }
    }while (0);
    if(httpUrl != NULL)
    {
        liot_rtos_free(httpUrl);
    }
    liot_rtos_semaphore_release(http_semp);
    liot_rtos_semaphore_delete(http_semp);
    http_semp = NULL;
    liot_trace("===liot_http_app_thread exit===");
    liot_rtos_task_delete(NULL);
}
