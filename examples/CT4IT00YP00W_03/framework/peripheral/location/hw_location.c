#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "liot_os.h"
#include "liot_dev.h"
#include "liot_nw.h"
#include "liot_lbs_client.h"
#include "liot_log.h"

#include "hw_location.h"
#include "app_nv.h"

#define LBS_GET_TIMEOUT_MS  10000
#define LBS_URL             "http://locator-aep.xiot.senthink.com:80/locator/v0.1/locate"

static liot_sem_t g_lbs_sem = NULL;
static liot_lbs_postion_info_t g_lbs_result;
static volatile int g_lbs_ok = 0;

static void lbs_result_cb(liot_lbs_response_data_t *response_data)
{
    if (response_data == NULL || response_data->hndl == 0) {
        g_lbs_ok = 0;
        if (g_lbs_sem) liot_rtos_semaphore_release(g_lbs_sem);
        return;
    }

    if (response_data->result == LIOT_LBS_OK && response_data->pos_num > 0) {
        memcpy(&g_lbs_result, &response_data->pos_info[0], sizeof(liot_lbs_postion_info_t));
        g_lbs_ok = 1;
    } else {
        g_lbs_ok = 0;
    }

    if (g_lbs_sem) liot_rtos_semaphore_release(g_lbs_sem);
}

int locationModuleGetPosition(location_info_t *info)
{
    liot_lbs_client_hndl lbs_client = 0;
    int ret = -1;
    uint8_t nSim = 0;
    liot_nw_cell_info_s cell_info;
    liot_nw_seclection_info_s select_info;
    liot_lbs_option_t lbs_option;
    liot_lbs_cell_info_t lbs_cell_info[LBS_MAX_CELL_NUM] = {0};
    char imei_str[64] = {0};

    if (info == NULL) return -1;
    memset(info, 0, sizeof(location_info_t));

    liot_dev_get_imei(imei_str, 64, 0);

    liot_lbs_basic_info_t basic_info = {
        .type       = 1,
        .encrypt    = 1,
        .key_index  = 1,
        .pos_format = 1,
        .loc_method = 4,
    };

    liot_lbs_auth_info_t auth_info = {
        .user_name = LBS_DEFAULT_USER_NAME,
        .user_pwd  = LBS_DEFAULT_USER_PWD,
        .token     = LBS_DEFAULT_TOKEN,
        .rand      = liot_rtos_rand(),
    };
    strcpy(auth_info.imei, imei_str);

    liot_rtos_semaphore_create(&g_lbs_sem, 0);
    if (g_lbs_sem == NULL) return -1;

    if (liot_nw_get_cell_info(0, &cell_info) != LIOT_NW_SUCCESS) {
        liot_trace("[LOC] get cell info failed");
        goto exit;
    }

    if (liot_nw_get_selection(nSim, &select_info) != 0) {
        liot_trace("[LOC] get selection failed");
        goto exit;
    }

    if (select_info.act != LIOT_NW_ACCESS_TECH_E_UTRAN) {
        liot_trace("[LOC] unsupported access tech: %d", select_info.act);
        goto exit;
    }

    {
        char mcc_str[5] = {0};
        char mnc_str[5] = {0};
        int i;

        lbs_cell_info[0].radio = 3;
        snprintf(mcc_str, 5, "%03X", cell_info.lte_info[0].mcc);
        lbs_cell_info[0].mcc = atoi(mcc_str);
        snprintf(mnc_str, 5, "%02X", cell_info.lte_info[0].mnc & 0xFFF);
        lbs_cell_info[0].mnc = atoi(mnc_str);
        lbs_cell_info[0].cell_id = cell_info.lte_info[0].cid;
        lbs_cell_info[0].lac_id  = cell_info.lte_info[0].tac;
        lbs_cell_info[0].pci     = cell_info.lte_info[0].pci;
        lbs_cell_info[0].earfcn  = cell_info.lte_info[0].earfcn;
        lbs_cell_info[0].bcch    = cell_info.lte_info[0].earfcn;
        lbs_cell_info[0].signal  = cell_info.lte_info[0].rssi;

        for (i = 0; i < cell_info.lte_info_num && i + 1 < LBS_MAX_CELL_NUM; i++) {
            lbs_cell_info[i + 1].radio = 3;
            snprintf(mcc_str, 5, "%03X", cell_info.lte_info[i + 1].mcc);
            lbs_cell_info[i + 1].mcc = atoi(mcc_str);
            snprintf(mnc_str, 5, "%02X", cell_info.lte_info[i + 1].mnc & 0xFFF);
            lbs_cell_info[i + 1].mnc = atoi(mnc_str);
            lbs_cell_info[i + 1].cell_id = cell_info.lte_info[i + 1].cid;
            lbs_cell_info[i + 1].lac_id  = cell_info.lte_info[i + 1].tac;
            lbs_cell_info[i + 1].pci     = cell_info.lte_info[i + 1].pci;
            lbs_cell_info[i + 1].earfcn  = cell_info.lte_info[i + 1].earfcn;
            lbs_cell_info[i + 1].bcch    = cell_info.lte_info[i + 1].earfcn;
            lbs_cell_info[i + 1].signal  = cell_info.lte_info[i + 1].rssi;
        }
    }

    memset(&lbs_option, 0, sizeof(lbs_option));
    lbs_option.pdp_cid     = 1;
    lbs_option.sim_id      = nSim;
    lbs_option.req_timeout = 60;
    lbs_option.basic_info  = &basic_info;
    lbs_option.auth_info   = &auth_info;
    lbs_option.cell_num    = cell_info.lte_info_num;
    lbs_option.cell_info   = lbs_cell_info;

    g_lbs_ok = 0;
    ret = liot_lbs_get_position(&lbs_client, LBS_URL, &lbs_option, lbs_result_cb, NULL);
    if (ret != LIOT_LBS_OK) {
        liot_trace("[LOC] lbs request failed: %d", ret);
        ret = -1;
        goto exit;
    }

    if (liot_rtos_semaphore_wait(g_lbs_sem, LBS_GET_TIMEOUT_MS) != 0 || !g_lbs_ok) {
        liot_trace("[LOC] lbs timeout or failed");
        ret = -1;
        goto exit;
    }

    strncpy(info->longitude, g_lbs_result.longitude, LOCATION_LONGITUDE_LEN - 1);
    strncpy(info->latitude, g_lbs_result.latitude, LOCATION_LATITUDE_LEN - 1);
    strncpy(info->desc, g_lbs_result.desc, LOCATION_DESC_LEN - 1);
    liot_trace("[LOC] success: %s, %s", info->longitude, info->latitude);
    ret = 0;

exit:
    if (g_lbs_sem) {
        liot_rtos_semaphore_release(g_lbs_sem);
        liot_rtos_semaphore_delete(g_lbs_sem);
        g_lbs_sem = NULL;
    }
    if (ret != 0) {
#if LOCATION_FALLBACK_ENABLE
        strncpy(info->longitude, LOCATION_FALLBACK_LONGITUDE, LOCATION_LONGITUDE_LEN - 1);
        strncpy(info->latitude, LOCATION_FALLBACK_LATITUDE, LOCATION_LATITUDE_LEN - 1);
        strncpy(info->desc, LOCATION_FALLBACK_DESC, LOCATION_DESC_LEN - 1);
        liot_trace("[LOC] fallback to default position");
        ret = 0;
#endif
    }
    return ret;
}
