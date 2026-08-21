/**
 * @file liot_router_user.c
 * @brief liotRouter 用户层入口 - 初始化各模块并启动路由器
 */

#include "liot_router.h"
#include "liot_router_nv.h"
#include "liot_router_user.h"
#include "liot_gpio2.h"

#ifdef LIOT_ROUTER_LAN0_ENABLE
extern Liot_RouterLanDev_t gLan0;
#endif

#ifdef LIOT_ROUTER_LAN1_ENABLE
extern Liot_RouterLanDev_t gLan1;
#endif

/* ──── 注册表 ──── */
static Liot_RouterLanDev_t *gRegistLan[3] = {
#ifdef LIOT_ROUTER_LAN0_ENABLE
    &gLan0,
#endif
#ifdef LIOT_ROUTER_LAN1_ENABLE
    &gLan1,
#endif
};

void Liot_RouterStart(void)
{

#ifdef LIOT_ROUTER_RESET_ENABLE
    Liot_RouterResetInit();
#endif

    Liot_RouterNvInit();

    Liot_RouterCfg_t routerCfg = {
        .wanNetmode      = 0,  /* 0: NAT, 1: router */
        .wanCid          = LIOT_ROUTER_WAN_CID,
        .wanTaskPriority = LIOT_ROUTER_TASK_PRIORITY,
        .wanMac          = LIOT_ROUTER_WAN_MAC,
        .dhcpGatewayIp   = LIOT_ROUTER_DEFAULT_GATEWAY_IP,
        .dhcpSubnetMask  = LIOT_ROUTER_DEFAULT_SUBNET_MASK,
        .dhcpLeaseTime   = LIOT_ROUTER_DEFAULT_LEASE_TIME,
        .dhcpDns1        = 0,
        .dhcpDns2        = 0,
    };
    Liot_RouterInit(&routerCfg);

    Liot_RouterLanInit(gRegistLan, sizeof(gRegistLan) / sizeof(gRegistLan[0]));

#ifdef LIOT_ROUTER_LIGHT_ENABLE
    /* 在 Liot_RouterInit(内部 wan 注册 PS 回调)之后再 init，
     * 使 light 的 Liot_PsEventCb 回调不被 wan 覆盖 */
    Liot_RouterLightInit();

    /* 点亮初始化成功的 LAN 口指示灯 */
    uint8_t lanCount = Liot_RouterLanGetCount();
    for (uint8_t k = 0; k < lanCount; k++) {
        Liot_RouterLan_t *lan = Liot_RouterLanGetInfo(k);
        if (lan)
            Liot_RouterLightLanSet(lan->lanPort, 1);
    }
#endif


#ifdef LIOT_ROUTER_WEB_ENABLE
    /* Web 管理服务：须在路由核心与 LAN 初始化之后启动，此时 lwip socket 就绪 */
    Liot_WebInit();
#endif

}

