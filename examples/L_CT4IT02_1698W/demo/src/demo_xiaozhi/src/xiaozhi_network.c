#include "xiaozhi_core.h"

#include <string.h>

#include "liot_datacall.h"
#include "liot_log.h"
#include "liot_os.h"

int xiaozhi_network_start(void)
{
    int ret;
    int retry = 0;
    liot_data_call_info_t info;

    liot_trace("[xiaozhi] network register wait");
    while ((ret = liot_network_register_wait(0, 60)) != LIOT_DATACALL_SUCCESS && retry++ < 5) {
        liot_trace("[xiaozhi] register retry=%d ret=0x%x", retry, ret);
    }
    if (ret != LIOT_DATACALL_SUCCESS) return -1;
    liot_set_data_call_asyn_mode(0, 1, 0);
    ret = liot_start_data_call(0, 1, LIOT_DATA_TYPE_IPV4V6,
                               (CHAR *)"APNTEST", (CHAR *)"", (CHAR *)"",
                               LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("[xiaozhi] data call ret=0x%x", ret);
    if (ret != LIOT_DATACALL_SUCCESS && ret != LIOT_DATACALL_REPEAT_ACTIVE_ERR)
        return -2;

    liot_rtos_task_sleep_s(4);
    memset(&info, 0, sizeof(info));
    ret = liot_get_data_call_info(0, 1, &info);
    liot_trace("[xiaozhi] PDP ret=0x%x cid=%d ipver=%d v4state=%d v6state=%d",
               ret, info.cid, info.ip_version, info.v4.state, info.v6.state);
    liot_trace("[xiaozhi] PDP IPv4=%s", liot_ip4addr_ntoa(&info.v4.addr.ip));
    liot_trace("[xiaozhi] PDP DNS1=%s", liot_ip4addr_ntoa(&info.v4.addr.pri_dns));
    liot_trace("[xiaozhi] PDP DNS2=%s", liot_ip4addr_ntoa(&info.v4.addr.sec_dns));
    liot_trace("[xiaozhi] network ready");
    return 0;
}
