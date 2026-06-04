/**
 * @file liot_dev_demo.c
 * @brief Device information demo implementation
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-07-08
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#include "lierda_app_main.h"
#include "liot_dev.h"
#include "liot_os.h"
#include "string.h"

void liot_dev_test_band_mode()
{
    liot_errcode_dev_e ret = 0;
    uint8_t sbandlist[32];
    memset(sbandlist, 0, sizeof(sbandlist));
    uint8_t snumband = 32;
    ret = Liot_DevGetBandMode(LIOT_DEV_GET_SUPPORT_BAND_LIST, &snumband, sbandlist);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("get band mode failed");
        return;
    }
    else
    {
        liot_trace("get band mode success snumband %d", snumband);
        for(int i = 0; i < snumband; i++)
        {
            liot_trace("get support band list: %d", sbandlist[i]);
        }   
    }
    /* Set band mode */
    uint8_t bandlist[32] = {3,5,8};
    uint8_t numband = 3;
    ret = Liot_DevSetBandMode(numband, bandlist);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set band mode failed");
        return;
    }

    liot_trace("set band mode success");

    memset(bandlist, 0, sizeof(bandlist));
    numband = 32;
    ret = Liot_DevGetBandMode(LIOT_DEV_GET_CAN_USED_BAND_LIST, &numband, bandlist);
    if(LIOT_DEV_SUCCESS )
    {
        liot_trace("get band mode failed");
        return;
    }

    for(int i = 0; i < numband; i++)
    {
        liot_trace("band used band list: %d", bandlist[i]);
    }

    liot_trace("get band mode success");    

    /* Set support band mode */
    ret = Liot_DevSetBandMode(snumband, sbandlist);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set band mode failed");
        return;
    }

    liot_trace("set band mode success");
}

void liot_dev_test_freq_config()
{
    liot_errcode_dev_e ret = 0;
    
    //关闭cfun 
    liot_dev_set_modem_fun(0, 0, 0);
    
    //锁小区与频点
    Liot_DevFreqConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_SET_LOCK_FREQ_OR_CELLID;
    cfg.phyCellId = 143;
    cfg.arfcnNum = 1;
    cfg.arfcnList[0] = 3738;

    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_DevFreqConfig failed");
    }
    else
    {
        liot_trace("set Liot_DevFreqConfig success");
    }

    //get
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_GET_FREQ;
    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("get Liot_DevFreqConfig failed");
    }
    else
    {
        liot_trace("get freq: cfg.phyCellId: %d cfg.lockedArfcn: %d ", cfg.phyCellId, cfg.lockedArfcn);
    }
    
    //解除锁频点
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_SET_UNLOCK;
    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_DevFreqConfig failed");
    }
    else
    {
        liot_trace("set Liot_DevFreqConfig success");
    }

    //锁频点 AT+ECFREQ=1,3738,3734
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_SET_PRIORITY_FREQ;
    cfg.arfcnNum = 2;
    cfg.arfcnList[0] = 3738;
    cfg.arfcnList[1] = 3734;
    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_DevFreqConfig failed");
    }
    else
    {
        liot_trace("set Liot_DevFreqConfig success");
    }

    //get
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_GET_FREQ;
    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_DevFreqConfig failed");
    }
    else
    {
        for(int i = 0; i < cfg.arfcnNum; i++)   
        {
            liot_trace("get freq: cfg.arfcnList[0]%d ", cfg.arfcnList[i]);
        }
    }
  
    //解除锁频点
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_SET_UNLOCK;
    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_DevFreqConfig failed");
    }
    else
    {
        liot_trace("set Liot_DevFreqConfig success");
    }

    //清除优先频点    
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LIOT_DEV_SET_CLEAN_PRIORITY_FREQ;
    ret = Liot_DevFreqConfig(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_DevFreqConfig failed");
    }
    else
    {
        liot_trace("set Liot_DevFreqConfig success");
    }
        
    //打开cfun 
    liot_dev_set_modem_fun(1, 0, 0);
}

void liot_dns_test()
{
    liot_errcode_dev_e ret;
    Liot_DevDnsServer_t dns_servers;

    memset(&dns_servers, 0, sizeof(dns_servers));
    
    //get dns addr
    ret = Liot_DevGetDnsServersAddr(&dns_servers);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("get dns addr failed");
    }
    else
    {
        for(int i = 0; i < LIOT_WARE_DEFAULT_DNS_NUM; i++)
        {
            liot_trace("get v4 dns addr: %s", dns_servers.ipv4Dns[i]);
            liot_trace("get v6 dns addr: %s", dns_servers.ipv6Dns[i]);
        }
    }

    //set ipv4 addr
    memset(&dns_servers, 0, sizeof(dns_servers));
    memcpy(dns_servers.ipv4Dns[0], "8.8.8.8", strlen("8.8.8.8"));
    memcpy(dns_servers.ipv4Dns[1], "114.114.114.114", strlen("114.114.114.114"));
    ret = Liot_DevSetDnsServersAddr(&dns_servers);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set dns addr failed");
    }
    
    //get dns addr
    memset(&dns_servers, 0, sizeof(dns_servers));
    ret = Liot_DevGetDnsServersAddr(&dns_servers);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("get dns addr failed");
    }
    else
    {
        for(int i = 0; i < LIOT_WARE_DEFAULT_DNS_NUM; i++)
        {
            liot_trace("get v4 dns addr: %s", dns_servers.ipv4Dns[i]);
            liot_trace("get v6 dns addr: %s", dns_servers.ipv6Dns[i]);
        }
    }
    
    //set ipv6 addr
    memset(&dns_servers, 0, sizeof(dns_servers));
    memcpy(dns_servers.ipv6Dns[0], "2001:4860:4860::8888", strlen("2001:4860:4860::8888"));
    memcpy(dns_servers.ipv6Dns[1], "2001:4860:4860::8844", strlen("2001:4860:4860::8844"));
    ret = Liot_DevSetDnsServersAddr(&dns_servers);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set dns addr failed");
    }

    //get dns addr
    memset(&dns_servers, 0, sizeof(dns_servers));
    ret = Liot_DevGetDnsServersAddr(&dns_servers);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("get dns addr failed");
    }
    else
    {
        for(int i = 0; i < LIOT_WARE_DEFAULT_DNS_NUM; i++)
        {
            liot_trace("get v4 dns addr: %s", dns_servers.ipv4Dns[i]);
            liot_trace("get v6 dns addr: %s", dns_servers.ipv6Dns[i]);
        }
    }
}
/**
 * @brief Application main thread for device info demo
 *
 * @param argv No argument required (unused)
 * @return void
 */
void liot_dev_demo_thread(void *argv)
{
    liot_errcode_dev_e ret = LIOT_DEV_SUCCESS;

    // Trace entry into the device demo thread
    liot_trace("dev demo thread enter");

    // Initial delay
    for (int n = 0; n < 10; n++)
    {
        liot_rtos_task_sleep_ms(500);
    }

    /* Get IMEI */
    char devinfo[64] = {0};
    liot_dev_get_imei(devinfo, 64, 0);
    liot_trace("IMEI: %s", devinfo);

    /* Get SN */
    memset(devinfo, 0, sizeof(devinfo));
    liot_dev_get_sn(devinfo, 64, 0);
    liot_trace("SN: %s", devinfo);

    /* Get product ID */
    memset(devinfo, 0, sizeof(devinfo));
    liot_dev_get_product_id(devinfo, 64);
    liot_trace("Product: %s", devinfo);

    /* Get model name */
    memset(devinfo, 0, sizeof(devinfo));
    liot_dev_get_model(devinfo, 64);
    liot_trace("Model: %s", devinfo);

    /* Get firmware version */
    memset(devinfo, 0, sizeof(devinfo));
    liot_dev_get_firmware_version(devinfo, 64);
    liot_trace("FW: %s", devinfo);

    /* Get sub-firmware version */
    memset(devinfo, 0, sizeof(devinfo));
    liot_dev_get_firmware_subversion(devinfo, 64);
    liot_trace("subVer: %s", devinfo);

    /* Query heap size */
    liot_trace("app_ram: %d,%d", liot_xPortGetTotalHeapSize() >> 10, liot_xPortGetFreeHeapSize() >> 10);

    /* Modem function mode */
    uint8_t cfun = 0;
    liot_dev_get_modem_fun(&cfun, 0);
    liot_trace("cfun: %d", cfun);
    liot_dev_set_modem_fun(LIOT_DEV_CFUN_FULL, 0, 0);

    char hdversion[32 + 1] = {0};
    /* get Hardver info*/
    ret = Liot_DevGetHardWareInfo(hdversion, sizeof(hdversion));
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("get Hardver info failed");
    }
    else
    {
        liot_trace("get Hardver info: %s", hdversion);    
    }    
        
    //band test
    liot_dev_test_band_mode();

    //Liot_DevFreqConfig fuc teset
    liot_dev_test_freq_config();
    
    Liot_DevRRCRelease_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = 1;
    cfg.idle_time = 2;
    cfg.retry_time = 0;
    ret = Liot_RRCRelease(&cfg);
    if(ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("set Liot_RRCRelease failed");
    }

    liot_trace("set Liot_RRCRelease success");    
    
    liot_dns_test();

    /* Exit task */
    ret = liot_rtos_task_delete(NULL);
    if (ret != LIOT_DEV_SUCCESS)
    {
        liot_trace("dev task deleted failed");
    }
}
