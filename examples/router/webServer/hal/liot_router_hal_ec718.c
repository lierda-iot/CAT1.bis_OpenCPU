/**
 * @file hal_ec718.c
 * @brief Web HAL - EC718 芯片平台适配实现
 *
 * 将 liot_router_hal.h 的平台无关接口映射到 EC718/利尔达 SDK：
 *   存储 → LittleFS      设备信息 → liot_dev_*
 *   系统 → ResetECSystemReset  固件 → liot_fota2 Liot_FotaUpgrade(本地文件包升级)
 *   加密 → mbedtls sha256       随机 → tick 混淆 + sha256
 */

#include "liot_router_hal.h"
#include "liot_router_user.h"
#include "liot_router.h"
#include "lierda_app_main.h"
#include "cmsis_os2.h"
#include "liot_power.h"
#include "liot_dev.h"
#include "liot_fota2.h"
#include "liot_fs_api.h"
#include "liot_nw.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <stdio.h>

/* WAN 信息经核心库 Liot_RouterGetWaninfo() 获取；
 * DHCP 租约表经 liot_dhcp_get_lease_table() 获取。二者声明均见 liot_router.h(已 include)。 */

/* KV 存储：key → LittleFS 文件路径前缀 */
#define HAL_KV_PREFIX   "/web_"

/* ───────── 存储 ───────── */

static void kv_path(const char *key, char *out, size_t outlen) {
    snprintf(out, outlen, HAL_KV_PREFIX "%s.dat", key);
}

int Liot_WebHalKvRead(const char *key, void *buf, size_t len) {
    char path[64];
    kv_path(key, path, sizeof(path));
    LFILE fd = liot_fopen(path, "r");               /* 只读，文件不存在返回负值 */
    if (fd < LIOT_FS_OK) return -1;
    /* liot_fread(buffer,size,num,fd)：按字节读(size=1,num=len)，返回已读元素数 */
    int rd = liot_fread(buf, 1, len, fd);
    liot_fclose(fd);
    return (rd < 0) ? -1 : rd;
}

int Liot_WebHalKvWrite(const char *key, const void *buf, size_t len) {
    char path[64];
    kv_path(key, path, sizeof(path));
    LFILE fd = liot_fopen(path, "w");               /* 写模式新建/截断 */
    if (fd < LIOT_FS_OK) return -1;
    int wr = liot_fwrite((void *)buf, 1, len, fd);
    liot_fclose(fd);
    return (wr == (int)len) ? 0 : -1;
}

int Liot_WebHalKvErase(const char *key) {
    char path[64];
    kv_path(key, path, sizeof(path));
    int r = liot_remove(path);
    /* 不存在视为已擦除成功 */
    return (r == LIOT_FS_OK || r == LIOT_FS_NOT_EXIST) ? 0 : -1;
}

/* ───────── 设备信息 ───────── */

int Liot_WebHalDevGetInfo(Liot_WebHalDevInfo_t *info) {
    if (!info) return -1;
    /* 型号/IMEI/SN/固件版本/产品ID 运行期不变，首次查到即缓存，
     * 之后直接返回缓存，省去每次 5 次模组查询(登录落地页明显提速)。 */
    static Liot_WebHalDevInfo_t cache;
    static uint8_t cached = 0;
    if (!cached) {
        memset(&cache, 0, sizeof(cache));
        liot_dev_get_model(cache.model, sizeof(cache.model));
        liot_dev_get_imei(cache.imei, sizeof(cache.imei), 0);
        liot_dev_get_sn(cache.sn, sizeof(cache.sn), 0);
        liot_dev_get_firmware_version(cache.fwVer, sizeof(cache.fwVer));
        liot_dev_get_product_id(cache.productId, sizeof(cache.productId));
        /* 至少型号或IMEI拿到才算有效缓存，避免开机早期空值被永久缓存 */
        if (cache.model[0] || cache.imei[0]) cached = 1;
    }
    memcpy(info, &cache, sizeof(*info));
    return 0;
}

/* ───────── 网络状态 ───────── */

int Liot_WebHalNetWanStatus(Liot_WebHalWanStatus_t *st) {
    if (!st) return -1;
    memset(st, 0, sizeof(*st));
    /* WAN 信息由核心库提供：返回 NULL 表示 WAN 未激活(取代旧 valid 字段) */
    const Liot_RouterWanInfo_t *w = Liot_RouterGetWaninfo();
    if (w != NULL) {
        st->valid = 1;
        /* 业务侧存网络序，转主机序对外 */
        st->ip   = liot_router_ntohl(w->wanIp);
        st->mask = liot_router_ntohl(w->wanMask);
        st->gw   = liot_router_ntohl(w->wanGw);
        st->dns1 = liot_router_ntohl(w->dns1);
        st->dns2 = liot_router_ntohl(w->dns2);
    }
    /* 信号：CSQ(0-31,99无效) → dBm。查询走模组/AT 层，单次可能数百 ms，
     * 若每次请求都同步查会拖慢页面。故加 60s 缓存：60s 内直接返回上次值，
     * 不阻塞请求。信号值无需实时，1 分钟刷新一次足够。 */
    static int   rssiCache = 0;
    static uint32_t rssiTs = 0;
    uint32_t now = Liot_WebHalUptimeMs();
    if (rssiTs == 0 || (uint32_t)(now - rssiTs) > 60000) {
        unsigned char csq = 99;
        if (liot_nw_get_csq(0, &csq) == 0 && csq != 99 && csq <= 31)
            rssiCache = -113 + 2 * (int)csq;   /* 标准换算 */
        else
            rssiCache = 0;                     /* 0 表示未知 */
        rssiTs = now;
    }
    st->rssi = rssiCache;
    return 0;
}

int Liot_WebHalNetLanStatus(Liot_WebHalLanStatus_t *st) {
    if (!st) return -1;
    memset(st, 0, sizeof(*st));
    /* lease 表返回的 count 是地址池总容量，需自己数 valid 条目才是真实在线数 */
    typedef struct { uint8_t mac[6]; uint8_t lan_port; uint8_t valid; uint32_t ip; uint32_t leaseStart; } Liot_Lease_t;
    uint8_t total = 0;
    const Liot_Lease_t *table = (const Liot_Lease_t *)liot_dhcp_get_lease_table(&total);
    uint8_t online = 0;
    for (uint8_t i = 0; i < total; i++) if (table[i].valid) online++;
    st->clientCount = online;
    st->gatewayIp   = LIOT_ROUTER_DEFAULT_GATEWAY_IP;
    st->subnetMask  = LIOT_ROUTER_DEFAULT_SUBNET_MASK;
    st->linkUp      = 1;   /* 暂无网口链路查询接口，保留字段但前端不展示 */
    return 0;
}

/* ───────── 系统控制 ───────── */

void Liot_WebHalReboot(void) {
    liot_power_reset(LIOT_RESET_NORMAL);
}

/* 删除单个文件用于恢复出厂：文件不存在(NOENT)视为成功(目标就是让它没有)，
 * 仅真正的删除失败(I/O/文件系统异常)返回 -1 并打日志。 */
static int factory_del(const char *path) {
    int r = liot_remove(path);
    if (r == LIOT_FS_OK || r == LIOT_FS_NOT_EXIST) {
        return 0;
    }
    liot_trace("factory reset: remove fail, ret=%d", r);
    return -1;
}

int Liot_WebHalFactoryReset(void) {
    int fail = 0;

    char authPath[64];
    kv_path("auth", authPath, sizeof(authPath));   /* /web_auth.dat */

    /* 路由配置 + Web 账号，逐个删并统计失败数 */
    if (factory_del(LIOT_ROUTER_NV_FILE) != 0) fail++;
    if (factory_del(authPath) != 0) fail++;

    if (fail != 0) {
        liot_trace("factory reset: %d file(s) FAIL to remove", fail);
        return -1;   /* 有删除失败，上层据此提示且不重启 */
    }
    liot_trace("factory reset: all config cleared");
    return 0;
}

uint32_t Liot_WebHalUptimeMs(void) {
    return (uint32_t)(osKernelGetTickCount() * (1000U / osKernelGetTickFreq()));
}

/* ───────── 固件升级（走 liot_fota2 Liot_FotaUpgrade，本地文件包升级）─────────
 * 流程：begin(打开 liot_fs 文件) → write(liot_fwrite 追加落地整包)
 *      → finish(关闭文件，仅落地不校验) → apply(Liot_FotaUpgrade "FILE:xxx")。
 * Liot_FotaUpgrade 内部完成 校验→(enable=1)重启升级，一个接口全包；
 * 文件必须用 liot_fs API 落地，与 FILE: 前缀底层的 liot_fopen 同域。 */

#define HAL_FW_FILE      "web_fw.par"        /* liot_fs 里的固件包文件名 */
#define HAL_FW_FILE_URL  "FILE:web_fw.par"   /* Liot_FotaUpgrade 的本地文件 URL */

static LFILE   gFwFd     = -1;   /* liot_fs 文件句柄 */
static uint8_t gFwActive = 0;    /* 当前是否在一次升级会话中 */

static void fw_progress_cb(uint8_t progress) {
    liot_trace("fw: upgrade progress %d%%", progress);
}

int Liot_WebHalFwBegin(uint32_t totalSize) {
    (void)totalSize;
    if (gFwActive && gFwFd >= LIOT_FS_OK) liot_fclose(gFwFd);
    liot_remove(HAL_FW_FILE);                       /* 清掉上次残留 */
    gFwFd = liot_fopen(HAL_FW_FILE, "w");           /* 写模式新建 */
    if (gFwFd < LIOT_FS_OK) {
        liot_trace("fw: fopen %s fail, ret=%d", HAL_FW_FILE, (int)gFwFd);
        gFwActive = 0;
        return -1;
    }
    gFwActive = 1;
    return 0;
}

int Liot_WebHalFwWrite(const void *data, size_t len) {
    if (!gFwActive || gFwFd < LIOT_FS_OK || !data || len == 0) return -1;
    /* liot_fwrite(buffer,size,num,fd)：按字节写(size=1,num=len)，返回已写元素数 */
    int wr = liot_fwrite((void *)data, 1, len, gFwFd);
    if (wr != (int)len) {
        liot_trace("fw: fwrite want=%u got=%d", (unsigned)len, wr);
        return -1;
    }
    return 0;
}

int Liot_WebHalFwFinish(uint8_t reboot) {
    if (!gFwActive) return -1;
    gFwActive = 0;
    if (gFwFd >= LIOT_FS_OK) { liot_fclose(gFwFd); gFwFd = -1; }
    /* 校验+升级统一由 Liot_FotaUpgrade 完成(见 Liot_WebHalFwApply)。
     * 这里只负责把整包落地；reboot=1 时立即触发升级。 */
    if (reboot) {
        return Liot_WebHalFwApply();   /* 内部会校验，失败返回 -1 */
    }
    return 0;
}

int Liot_WebHalFwApply(void) {
    /* 用本地文件包触发升级：Liot_FotaUpgrade 内部读文件→校验→(enable=1)重启刷写。
     * enable=1 时正常不返回；若返回则一定是校验/参数等失败。 */
    Liot_FotaConfig_t cfg;
    cfg.url      = HAL_FW_FILE_URL;
    cfg.enable   = 1;                 /* 校验通过后自动重启升级 */
    cfg.timeout  = 60000;
    cfg.callback = fw_progress_cb;
    liot_fota_err_e r = Liot_FotaUpgrade(&cfg);
    if (r != L_FOTA_UPGRADE_SUCCESS) {
        liot_trace("fw: FOTA fail, ret=%d", r);
        liot_remove(HAL_FW_FILE);     /* 失败清残留，避免误升级 */
        return -1;
    }
    return 0;   /* 正常到不了这里(enable=1 已重启) */
}

void Liot_WebHalFwAbort(void) {
    if (gFwActive && gFwFd >= LIOT_FS_OK) liot_fclose(gFwFd);
    gFwFd = -1;
    gFwActive = 0;
    liot_remove(HAL_FW_FILE);         /* 清除未完成的上传文件 */
}

/* ───────── 加密 / 随机数 ───────── */

int Liot_WebHalSha256(const void *data, size_t len, uint8_t out32[32]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int ok = mbedtls_sha256_starts_ret(&ctx, 0) == 0
          && mbedtls_sha256_update_ret(&ctx, (const unsigned char *)data, len) == 0
          && mbedtls_sha256_finish_ret(&ctx, out32) == 0;
    mbedtls_sha256_free(&ctx);
    return ok ? 0 : -1;
}

int Liot_WebHalRandom(void *buf, size_t len) {
    /* 无独立 TRNG 头暴露时，用 tick/地址等熵源多轮 sha256 生成流。
     * 用于 session token / 盐，足够不可预测。 */
    static uint32_t counter = 0;
    uint8_t seed[32];
    uint8_t block[32];
    uint8_t *out = (uint8_t *)buf;
    uint32_t mix[4];

    mix[0] = osKernelGetTickCount();
    mix[1] = ++counter;
    mix[2] = (uint32_t)(uintptr_t)&seed;
    mix[3] = (uint32_t)(uintptr_t)buf ^ (uint32_t)len;
    if (Liot_WebHalSha256(mix, sizeof(mix), seed) != 0) return -1;

    size_t done = 0;
    while (done < len) {
        mix[0] = osKernelGetTickCount();
        mix[1] = ++counter;
        uint8_t in[32 + sizeof(mix)];
        memcpy(in, seed, 32);
        memcpy(in + 32, mix, sizeof(mix));
        if (Liot_WebHalSha256(in, sizeof(in), block) != 0) return -1;
        memcpy(seed, block, 32);
        size_t n = (len - done < 32) ? (len - done) : 32;
        memcpy(out + done, block, n);
        done += n;
    }
    return 0;
}
