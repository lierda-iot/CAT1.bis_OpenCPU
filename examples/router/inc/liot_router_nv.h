/**
 * @file liot_router_nv.h
 * @brief 路由器配置持久化 - 数据结构 + API
 */

#ifndef __LIOT_ROUTER_NV_H__
#define __LIOT_ROUTER_NV_H__

#include <stdint.h>
#include "liot_router_user.h"

#define LIOT_NV_MAX_STATIC_BIND   8
#define LIOT_NV_MAX_NAT_RULES     16

typedef struct Liot_RouterNvStaticBind {
    uint8_t  mac[6];
    uint8_t  rsvd[2];
    uint32_t ip;          // 网络序
} Liot_RouterNvStaticBind_t;

typedef struct Liot_RouterNvNatRule {
    uint8_t  proto;       // 6=TCP, 17=UDP
    uint8_t  rsvd;
    uint16_t extPort;
    uint32_t intIp;       // 网络序
    uint16_t intPort;
    uint16_t rsvd2;
} Liot_RouterNvNatRule_t;

typedef struct Liot_RouterNv {
    uint32_t magic;
    uint8_t  version;
    uint8_t  rsvd[3];

    uint32_t dns1;            // 网络序，0=使用运营商DNS
    uint32_t dns2;

    uint32_t poolStart;       // 主机序
    uint32_t poolEnd;         // 主机序

    uint32_t gatewayIp;       // 主机序
    uint32_t subnetMask;      // 主机序

    uint8_t  staticBindCount;
    uint8_t  natRuleCount;
    uint8_t  rsvd2[2];

    Liot_RouterNvStaticBind_t staticBind[LIOT_NV_MAX_STATIC_BIND];
    Liot_RouterNvNatRule_t    natRules[LIOT_NV_MAX_NAT_RULES];
} Liot_RouterNv_t;

void Liot_RouterNvInit(void);
const Liot_RouterNv_t *Liot_RouterNvGet(void);
void Liot_RouterNvSet(const Liot_RouterNv_t *cfg);
void Liot_RouterNvReset(void);

#endif /* __LIOT_ROUTER_NV_H__ */
