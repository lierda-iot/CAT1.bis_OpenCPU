/**
 * @file liot_router_nv.c
 * @brief 路由器配置持久化 - 读写 LittleFS 文件 + 加载配置到各模块
 */

#include "liot_router_nv.h"
#include "liot_router.h"
#include "lierda_app_main.h"
#include "liot_fs_api.h"
#include <string.h>

static Liot_RouterNv_t gNvCfg;

static void nv_set_defaults(Liot_RouterNv_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic      = LIOT_ROUTER_NV_MAGIC;
    cfg->version    = LIOT_ROUTER_NV_VERSION;
    cfg->dns1       = 0;
    cfg->dns2       = 0;
    cfg->poolStart  = LIOT_ROUTER_DEFAULT_GATEWAY_IP + 1;
    cfg->poolEnd    = LIOT_ROUTER_DEFAULT_GATEWAY_IP + LIOT_DHCP_POOL_SIZE;
    cfg->gatewayIp  = LIOT_ROUTER_DEFAULT_GATEWAY_IP;
    cfg->subnetMask = LIOT_ROUTER_DEFAULT_SUBNET_MASK;
}

static void nv_apply(const Liot_RouterNv_t *nv)
{
    if (nv->gatewayIp != LIOT_ROUTER_DEFAULT_GATEWAY_IP ||
        nv->subnetMask != LIOT_ROUTER_DEFAULT_SUBNET_MASK) {
        Liot_RouterDhcpSetGateway(nv->gatewayIp, nv->subnetMask);
        liot_netif_update_ip(nv->gatewayIp, nv->subnetMask);
    }

    uint32_t defaultPoolStart = nv->gatewayIp + 1;
    uint32_t defaultPoolEnd   = nv->gatewayIp + LIOT_DHCP_POOL_SIZE;
    if (nv->poolStart != defaultPoolStart || nv->poolEnd != defaultPoolEnd) {
        Liot_RouterDhcpSetPool(nv->poolStart, nv->poolEnd);
    }

    if (nv->dns1 != 0) {
        Liot_RouterDhcpSetDns(nv->dns1, nv->dns2);
    }

    for (uint8_t i = 0; i < nv->staticBindCount; i++) {
        Liot_RouterDhcpBindStatic(nv->staticBind[i].mac, nv->staticBind[i].ip);
    }

    for (uint8_t i = 0; i < nv->natRuleCount; i++) {
        liot_nat_add_rule(nv->natRules[i].proto, nv->natRules[i].extPort,
                          nv->natRules[i].intIp, nv->natRules[i].intPort);
    }
}

void Liot_RouterNvInit(void)
{
    nv_set_defaults(&gNvCfg);

    LFILE file = liot_fopen(LIOT_ROUTER_NV_FILE, "r");
    if (file >= 0) {
        Liot_RouterNv_t tmp;
        int rd = liot_fread(&tmp, 1, sizeof(tmp), file);
        liot_fclose(file);

        if (rd == (int)sizeof(tmp) &&
            tmp.magic == LIOT_ROUTER_NV_MAGIC &&
            tmp.version == LIOT_ROUTER_NV_VERSION) {

            if (tmp.staticBindCount > LIOT_NV_MAX_STATIC_BIND)
                tmp.staticBindCount = LIOT_NV_MAX_STATIC_BIND;
            if (tmp.natRuleCount > LIOT_NV_MAX_NAT_RULES)
                tmp.natRuleCount = LIOT_NV_MAX_NAT_RULES;

            memcpy(&gNvCfg, &tmp, sizeof(gNvCfg));

            liot_trace("NV: loaded, pool=%d~%d binds=%d nat=%d",
                       gNvCfg.poolStart & 0xFF, gNvCfg.poolEnd & 0xFF,
                       gNvCfg.staticBindCount, gNvCfg.natRuleCount);
        } else {
            liot_trace("NV: file invalid, use defaults");
        }
    } else {
        liot_trace("NV: no config file, use defaults");
    }

    nv_apply(&gNvCfg);
}

const Liot_RouterNv_t *Liot_RouterNvGet(void)
{
    return &gNvCfg;
}

void Liot_RouterNvSet(const Liot_RouterNv_t *cfg)
{
    Liot_RouterNv_t tmp;
    memcpy(&tmp, cfg, sizeof(tmp));
    tmp.magic = LIOT_ROUTER_NV_MAGIC;
    tmp.version = LIOT_ROUTER_NV_VERSION;

    LFILE file = liot_fopen(LIOT_ROUTER_NV_FILE, "w");
    if (file < 0) {
        liot_trace("NV: failed to open for write, ret=%d", file);
        return;
    }

    liot_fwrite(&tmp, 1, sizeof(tmp), file);
    liot_fclose(file);

    liot_trace("NV: config saved");
}

void Liot_RouterNvReset(void)
{
    liot_remove(LIOT_ROUTER_NV_FILE);
    nv_set_defaults(&gNvCfg);
    liot_trace("NV: reset to defaults");
}
