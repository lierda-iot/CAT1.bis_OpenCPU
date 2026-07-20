/**
 * @file liot_router_atlan1.c
 * @brief LAN#1 实例 (SPI0)
 */

#include "liot_router_user.h"
#include "liot_router.h"
#include "liot_ch390.h"
#include "lierda_app_main.h"
#include "liot_os.h"

static Liot_Ch390Handle_t gLan1Ch390Handle = NULL;
static Liot_RouterLanEventCb gLan1EventCb = NULL;
static Liot_RouterLan_t *gLan1Lan = NULL;

static void atlan1_wakeup_cb(void *arg)
{
    if (gLan1EventCb && gLan1Lan) {
        gLan1EventCb(gLan1Lan);
    }
}

static int32_t atlan1_init(Liot_RouterLan_t *lan, Liot_RouterLanEventCb eventCb)
{
    Liot_Ch390Cfg_t ch390Cfg = {
        .spiPort = LIOT_CH390_1_SPI_PORT,
        .rstGpio = LIOT_CH390_1_RST_GPIO,
        .ssnGpio = LIOT_CH390_1_SSN_GPIO,
        .intWakeupPad = LIOT_CH390_1_INT_WAKEUP_PAD,
        .mac = {0},
        .intWakeupCb = atlan1_wakeup_cb,
        .intWakeupCbArg = NULL,
    };
    gLan1Ch390Handle = Liot_Ch390Create(&ch390Cfg);
    if (!gLan1Ch390Handle) return -1;

    Liot_Ch390HardwareReset(gLan1Ch390Handle);
    Liot_Ch390DefaultConfig(gLan1Ch390Handle);
    gLan1EventCb = eventCb;
    gLan1Lan = lan;
    uint16_t vid = Liot_Ch390GetVendorId(gLan1Ch390Handle);
    uint16_t pid = Liot_Ch390GetProductId(gLan1Ch390Handle);

    liot_trace("LAN1 get VID/PID : 0x%x/0x%x", vid, pid);

    if (vid == 0xffff || vid == 0x0000 || pid == 0xffff || pid == 0x0000) {
        liot_trace("LAN1 VID/PID invalid");
        Liot_Ch390Destroy(gLan1Ch390Handle);
        return -1;
    }

    liot_rtos_task_sleep_ms(50);
    Liot_Ch390GetIntStatus(gLan1Ch390Handle);

    Liot_Ch390GetMac(gLan1Ch390Handle, gLan1Lan->mac);
    liot_trace("LAN1 ok, mac=%x:%x:%x:%x:%x:%x",
        gLan1Lan->mac[0], gLan1Lan->mac[1], gLan1Lan->mac[2],
        gLan1Lan->mac[3], gLan1Lan->mac[4], gLan1Lan->mac[5]);
    return 0;
}

static void atlan1_send(Liot_RouterLan_t *lan, uint8_t *buf, uint16_t len)
{
    Liot_Ch390Send(gLan1Ch390Handle, buf, len);
}

static uint32_t atlan1_receive(Liot_RouterLan_t *lan, uint8_t *buf)
{
    return Liot_Ch390Receive(gLan1Ch390Handle, buf);
}

static uint8_t atlan1_get_int_status(Liot_RouterLan_t *lan)
{
    return Liot_Ch390GetIntStatus(gLan1Ch390Handle);
}

static uint8_t atlan1_get_link_status(Liot_RouterLan_t *lan)
{
    return Liot_Ch390GetLinkStatus(gLan1Ch390Handle);
}

Liot_RouterLanDev_t gLan1 = {
    .init = atlan1_init,
    .send = atlan1_send,
    .receive = atlan1_receive,
    .getIntStatus = atlan1_get_int_status,
    .getLinkStatus = atlan1_get_link_status,
};
