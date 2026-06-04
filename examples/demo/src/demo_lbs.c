/**
 * @file liot_lbs_demo.c
 * @author ciot_iot_support@lierda.com
 * @brief Location service example code demonstrating how to use LBS and WiFi services for positioning
 * @version 1.0
 * @date 2025-08-12
 * 
 * @copyright Copyright (c) 2025
 * 
 * @note For wifiscan, if using 716S/718S, it must be compiled in mid mode, 
 * and the network must be in IDLE state to obtain results successfully.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_dev.h"
#include "liot_lbs_client.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_wifiscan.h"
static liot_sem_t lbs_semp;


// Location service URL, default is Lierda, remains unchanged, no user modification required
#define LBS_URL "http://locator-aep.xiot.senthink.com:80/locator/v0.1/locate"   

//Location service TOKEN, needs to be applied for from the lierda server before use. 
#define LBS_TOKEN "B84E427D95B1DF4A3F49B18DDF714C72"    

/**
 * @brief Location method enumeration
 * Used to specify the method for location services, including LBS and WiFi
 */
typedef enum
{
    LIOT_LOC_MOTHED_LBS,   //LBS positioning
    LIOT_LOC_MOTHED_WIFI,  //WiFi positioning
}liot_loc_method;

/**
 * @brief Location service result callback function
 * 
 * This function is called when the location service is completed, used to handle the response data of the location service.
 * It mainly checks the location result, if successful, records the latitude and longitude information, and releases the semaphore used by the location service. If the location result is not successful, it only releases the semaphore.
 * 
 * @param response_data Pointer to the location service response data, containing location results and location information
 */
static void lbs_result_cb(liot_lbs_response_data_t *response_data)
{
    char latitude_str[30]  = {0};
    char longitude_str[30] = {0};

    if (response_data == NULL)
    {
        return;
    }

    liot_trace("lbs_result_cb  lbs result: %d", response_data->result);

    if (response_data->result == LIOT_LBS_OK)
    {
        memcpy(latitude_str, response_data->pos_info->latitude, strlen(response_data->pos_info->latitude));
        memcpy(longitude_str, response_data->pos_info->longitude, strlen(response_data->pos_info->longitude));

        // Record the latitude and longitude information
        liot_trace("lbs_result_cb  Location: longitude:%s, latitude:%s", latitude_str, longitude_str);
    }

    // release semaphore
    liot_rtos_semaphore_release(lbs_semp);
}

/**
 * @brief Set access point information
 * 
 * This function is used to organize and set the information of the scanned WiFi access points into a given structure.
 * It first checks if the input pointers are valid, then adjusts the number of access points based on the maximum limit (LBS_MAX_WIFI_NUM),
 * and finally traverses each access point information, formatting and storing its BSSID, RSSI, and SSID into the target structure. The function also logs the access point information for debugging purposes.
 * 
 * @param p_ap_infos Pointer to the structure containing information of scanned WiFi access points, including BSSID, RSSI, etc.
 * @param lbs_wifi_info Pointer to the structure used to store formatted WiFi MAC information, including BSSID, RSSI, SSID.
 * @return Returns the actual number of processed WiFi access points, or -1 if input parameters are empty
 */
int liot_wifiscan_ap_info_set(liot_wifi_ap_info_s *p_ap_infos, liot_lbs_wifi_mac_info_t *lbs_wifi_info)
{

    if (NULL == p_ap_infos || NULL == lbs_wifi_info)
    {
        return -1;
        liot_trace("result NULL");
    }
    if(p_ap_infos->bssidNum  > LBS_MAX_WIFI_NUM)
    {
        p_ap_infos->bssidNum = LBS_MAX_WIFI_NUM;
    }
    for (uint16 i = 0; i < p_ap_infos->bssidNum; i++)
    {
        // Format BSSID information
        sprintf(lbs_wifi_info[i].wifi_mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
                p_ap_infos->bssid[i][0],
                p_ap_infos->bssid[i][1],
                p_ap_infos->bssid[i][2],
                p_ap_infos->bssid[i][3],
                p_ap_infos->bssid[i][4],
                p_ap_infos->bssid[i][5]);

        // Set RSSI information
        lbs_wifi_info[i].wifi_rssi = p_ap_infos->rssi[i];

        // Format SSID information
        sprintf(lbs_wifi_info[i].wifi_ssid, "%X", (unsigned int)p_ap_infos->ssidHex[i]);

        // Record access point information log
        liot_trace("WIFISCAN:(%s,-,%d,%X:%X:%X:%X:%X:%X,%d",
                    lbs_wifi_info[i].wifi_ssid,
                   p_ap_infos->rssi[i],
                   p_ap_infos->bssid[i][0],
                   p_ap_infos->bssid[i][1],
                   p_ap_infos->bssid[i][2],
                   p_ap_infos->bssid[i][3],
                   p_ap_infos->bssid[i][4],
                   p_ap_infos->bssid[i][5],
                   p_ap_infos->channel[i]);
    }
    return p_ap_infos->bssidNum;
}

/**
 * @brief Get WiFi scan information
 * 
 * This function obtains information about surrounding WiFi APs by calling WiFi scan related functions, and sets this information into the given structure.
 * It first sets the WiFi scan options, then opens the WiFi scan device, performs the scan operation, and finally sets the scan results into the target structure.
 * 
 * @param lbs_wifi_info Pointer to a structure for storing WiFi scan information
 * @return Returns 0 on success, -1 on failure
 */
int  liot_get_wifiscan_info(liot_lbs_wifi_mac_info_t *lbs_wifi_info)
{
    // Set WiFi scan options, including scan time, default round, default AP count, and scan timeout value
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_option_set(LIOT_WIFI_SCAN_TIME_VAL_DEF,
                                                          LIOT_WIFI_SCAN_DEFAULT_ROUND,
                                                          LIOT_WIFI_SCAN_DEFAULT_AP_CNT,
                                                          LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_DEF,
                                                          1))
    {
        liot_trace("option set err");
        return -1;
    }

    // Open the WiFi scan device
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_open())
    {
        liot_trace("device open err");
        return -1;
    }

    liot_wifi_ap_info_s ap_infos = {0};

    // Perform the WiFi scan operation
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_do(&ap_infos))
    {
        liot_wifiscan_close();
        liot_trace("to do a scan err");
        return -1;
    }

    liot_trace("ap_cnt=%d", ap_infos.bssidNum);
    // Close the WiFi scan device
    liot_wifiscan_close();
    // Set the scan results into the target structure
    return liot_wifiscan_ap_info_set(&ap_infos, lbs_wifi_info);
}

/**
 * @brief Set cell information
 * 
 * This function converts and sets the data from the network cell information structure into the location service cell information structure.
 * It mainly handles LTE cell information, including MCC, MNC, cell ID, LAC ID, and signal strength.
 * 
 * @param p_cell_infos Pointer to the network cell information structure
 * @param lbs_cell_info Pointer to the location service cell information structure
 */
static void liot_set_cell_info(liot_nw_cell_info_s *p_cell_infos,   liot_lbs_cell_info_t *lbs_cell_info)
{
    char mcc_str[5] = {0};
    char mnc_str[5] = {0};
    int i  = 0;
    
    // Traverse the LTE cell information array
    for (i = 0; i < p_cell_infos->lte_info_num; i++)
    {
        // Convert MCC to a three-digit hexadecimal string and store it in mcc_str
        snprintf(mcc_str, 5, "%03X", p_cell_infos->lte_info[i].mcc);
        // Convert mcc_str to an integer and assign it to the mcc field in lbs_cell_info
        lbs_cell_info[i].mcc = atoi(mcc_str);
        memset(mcc_str, 0x0, 5);

        // Convert MNC to a two-digit hexadecimal string and store it in mnc_str, while handling the high 12 bits of MNC
        snprintf(mnc_str, 5, "%02X", p_cell_infos->lte_info[i].mnc & 0XFFF);
        // Convert mnc_str to an integer and assign it to the mnc field in lbs_cell_info
        lbs_cell_info[i].mnc = atoi(mnc_str);
        memset(mnc_str, 0x0, 5);

        // Set cell information
        lbs_cell_info[i].cell_id = p_cell_infos->lte_info[i].cid;
        lbs_cell_info[i].lac_id  = p_cell_infos->lte_info[i].tac;
        lbs_cell_info[i].signal  = p_cell_infos->lte_info[i].rssi;
        lbs_cell_info[i].pci     = p_cell_infos->lte_info[i].pci;
        lbs_cell_info[i].earfcn  = p_cell_infos->lte_info[i].earfcn;
        lbs_cell_info[i].bcch    = p_cell_infos->lte_info[i].earfcn;
    }
   liot_trace("mcc=%d,mnc=%d,cell_id=%d,signal=%d,bcch=%d,pci=%d,earfcn=%d,lac_id:%d",
                   lbs_cell_info[0].mcc,
                   lbs_cell_info[0].mnc,
                   lbs_cell_info[0].cell_id,
                   lbs_cell_info[0].signal,
                   lbs_cell_info[0].bcch,
                   lbs_cell_info[0].pci,
                   lbs_cell_info[0].earfcn,
                   lbs_cell_info[0].lac_id);

}

/**
 * @brief Get cell information
 * 
 * @param nSim SIM card identifier
 * @param lbs_cell_info Pointer to the structure for storing cell information
 * @return Returns the number of LTE cell information, returns -1 on failure
 * 
 * This function obtains cell information for the specified SIM card by calling the network module's liot_nw_get_cell_info function.
 * If the retrieval is successful, the relevant information is set into the lbs_cell_info structure, and the number of LTE cell information is returned.
 * If the retrieval fails, an error message is printed, and -1 is returned.
 */
int  liot_get_cell_info(uint8_t nSim, liot_lbs_cell_info_t *lbs_cell_info)
{
    liot_nw_cell_info_s cell_info = {0};
    if (liot_nw_get_cell_info(nSim, &cell_info) != LIOT_NW_SUCCESS)
    {
        liot_trace("===============lbs get cell info fail===============\n");
        return -1;
    }
    // Set cell information
    liot_set_cell_info(&cell_info, lbs_cell_info);
    
    liot_trace("%s:%d  info_num:%d  valid:%d", __FUNCTION__, __LINE__, cell_info.lte_info_num, cell_info.lte_info_valid);

    // Return the number of LTE cell information
    return cell_info.lte_info_num;
}

/**
 * @brief LBS positioning demo thread function
 * 
 * This function demonstrates how to use LBS service for location positioning. It includes the entire process from initiating data calls, obtaining data call information,
 * acquiring network selection information, collecting cell information or WiFi scan information, and finally requesting location positioning.
 * 
 * @param arg Thread parameter, unused
 */
extern void liot_lbs_demo_thread(void *arg)
{
    liot_lbs_client_hndl lbs_client = 0;
    int ret                         = 0;
    // Data call ID 
    int cid                         = 1;
    // WiFi AP count
    int ap_count                    = 0;

    liot_data_call_info_t info;
    // SIM ID
    uint8_t nSim = 0;
    liot_nw_seclection_info_s select_info;
    liot_lbs_option_t lbs_option;
    liot_lbs_cell_info_t lbs_cell_info[LBS_MAX_CELL_NUM] = {0};
    char imei_str[64]                                    = {0};
    liot_lbs_wifi_mac_info_t lbs_wifi_info[LBS_MAX_WIFI_NUM] = {0};
    
    // get device IMEI
    liot_dev_get_imei(imei_str, 64, nSim);

    // lbs basic info
    liot_lbs_basic_info_t basic_info = {
        .type       = 1,
        .encrypt    = 1,
        .key_index  = 1,
        .pos_format = 1,
        .loc_method = LIOT_LOC_MOTHED_LBS,
    };

    // lbs auth info
    liot_lbs_auth_info_t auth_info = {
        .user_name = "lierda",
        .user_pwd  = "123456",
        .token     = LBS_TOKEN,
    };
    strcpy(auth_info.imei, imei_str);
    memset(&imei_str, 0x00, sizeof(imei_str));
    liot_rtos_semaphore_create(&lbs_semp, 0);

    liot_trace("==========lbs demo start ==========");
    liot_trace("===start data call====");
    ret = liot_start_data_call(nSim, cid, LIOT_DATA_TYPE_IP, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("===data call result:%d", ret);
    if (ret != 0)
    {
        liot_trace("====data call failure!!!!=====");
    }
    liot_rtos_task_sleep_ms(2000);

    // set data call asyn mode
    liot_set_data_call_asyn_mode(nSim, cid, 1);

    ret = liot_get_data_call_info(nSim, cid, &info);
    if (ret != 0)
    {
        liot_trace("liot_get_data_call_info ret: %d", ret);
        liot_stop_data_call(nSim, cid);
        goto exit;
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

    // get network selection info
    ret = liot_nw_get_selection(nSim, &select_info);
    if (ret != 0)
    {
        liot_trace("liot_nw_get_selection ret: %d", ret);
        goto exit;
    }
    liot_trace("nw_act_type=%d", select_info.act);
    if (select_info.act != LIOT_NW_ACCESS_TECH_E_UTRAN)
    {
        liot_trace("network access technology type error");
         goto exit;
    }
   // init lbs option
    memset(&lbs_option, 0x00, sizeof(liot_lbs_option_t));
    lbs_option.pdp_cid     = cid;
    lbs_option.sim_id      = nSim;
    lbs_option.req_timeout = 60;
    lbs_option.auth_info   = &auth_info;
    while(1)
    {   
        if(basic_info.loc_method == LIOT_LOC_MOTHED_LBS)  // lbs
        {
            liot_trace("LBS start*****************");

            if((ap_count = liot_get_cell_info(nSim ,lbs_cell_info)) <= 0)
            {
                liot_trace("cell_info error");
                break;
            }
            lbs_option.cell_num    = ap_count;
            lbs_option.cell_info   = lbs_cell_info;
            
        }
        else
        {
            liot_trace("WIFI LOCTION start*****************");
            if((ap_count = liot_get_wifiscan_info(lbs_wifi_info)) <= 0)
            {
                liot_trace("wifi_scan error error");
                break;
            }
            lbs_option.wifi_num    = ap_count;
            lbs_option.wifi_info   = lbs_wifi_info;
        }
        lbs_option.basic_info  = &basic_info;
        if (LIOT_LBS_OK != liot_lbs_get_position(&lbs_client, LBS_URL, &lbs_option,lbs_result_cb, NULL))
        {
             liot_trace("lbs failed");
             break;
        }

        if(basic_info.loc_method == LIOT_LOC_MOTHED_LBS)
        {
             //Switch to WIFI positioning
            basic_info.loc_method = LIOT_LOC_MOTHED_WIFI;
            basic_info.type       = 0; 
        }
        else
        {
             //Switch to LBS positioning
            basic_info.loc_method = LIOT_LOC_MOTHED_LBS;
            basic_info.type       = 1;  
        }

        liot_rtos_semaphore_wait(lbs_semp, LIOT_WAIT_FOREVER); 
        liot_trace("lbs success");
        liot_rtos_task_sleep_s(20);
    }
exit:
    liot_rtos_semaphore_delete(lbs_semp);
    liot_trace("===liot_lbs_thread exit===");
    liot_rtos_task_delete(NULL);
}
