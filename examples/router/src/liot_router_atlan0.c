/**
 * @file liot_router_atlan0.c
 * @brief LAN#0 实例 (SPI1)
 */

#include "liot_router_user.h"
#include "liot_router.h"
#include "liot_ch390.h"
#include "lierda_app_main.h"
#include "liot_os.h"

static Liot_Ch390Handle_t gLan0Ch390Handle = NULL;
static Liot_RouterLanEventCb gLan0EventCb = NULL;
static Liot_RouterLan_t *gLan0Lan = NULL;

static void atlan0_wakeup_cb(void *arg)
{
    if (gLan0EventCb && gLan0Lan) {
        gLan0EventCb(gLan0Lan);
    }
}

static int32_t atlan0_init(Liot_RouterLan_t *lan, Liot_RouterLanEventCb eventCb)
{
    Liot_Ch390Cfg_t ch390Cfg = {
        .spiPort = LIOT_CH390_0_SPI_PORT,
        .rstGpio = LIOT_CH390_0_RST_GPIO,
        .ssnGpio = LIOT_CH390_0_SSN_GPIO,
        .intWakeupPad = LIOT_CH390_0_INT_WAKEUP_PAD,
        .mac = {0},
        .intWakeupCb = atlan0_wakeup_cb,
        .intWakeupCbArg = NULL,
    };
    gLan0Ch390Handle = Liot_Ch390Create(&ch390Cfg);
    if (!gLan0Ch390Handle) return -1;

    Liot_Ch390HardwareReset(gLan0Ch390Handle);
    Liot_Ch390DefaultConfig(gLan0Ch390Handle);
    gLan0EventCb = eventCb;
    gLan0Lan = lan;
    uint16_t vid = Liot_Ch390GetVendorId(gLan0Ch390Handle);
    uint16_t pid = Liot_Ch390GetProductId(gLan0Ch390Handle);

    liot_trace("LAN0 get VID/PID : 0x%x/0x%x", vid, pid);

    if (vid == 0xffff || vid == 0x0000 || pid == 0xffff || pid == 0x0000) {
        liot_trace("LAN0 VID/PID invalid");
        Liot_Ch390Destroy(gLan0Ch390Handle);
        return -1;
    }

    liot_rtos_task_sleep_ms(50);
    Liot_Ch390GetIntStatus(gLan0Ch390Handle);

    Liot_Ch390GetMac(gLan0Ch390Handle, gLan0Lan->mac);
    liot_trace("LAN0 ok, mac=%x:%x:%x:%x:%x:%x",
        gLan0Lan->mac[0], gLan0Lan->mac[1], gLan0Lan->mac[2],
        gLan0Lan->mac[3], gLan0Lan->mac[4], gLan0Lan->mac[5]);
    return 0;
}

static void atlan0_send(Liot_RouterLan_t *lan, uint8_t *buf, uint16_t len)
{
    Liot_Ch390Send(gLan0Ch390Handle, buf, len);
}

static uint32_t atlan0_receive(Liot_RouterLan_t *lan, uint8_t *buf)
{
    return Liot_Ch390Receive(gLan0Ch390Handle, buf);
}

static uint8_t atlan0_get_int_status(Liot_RouterLan_t *lan)
{
    return Liot_Ch390GetIntStatus(gLan0Ch390Handle);
}

static uint8_t atlan0_get_link_status(Liot_RouterLan_t *lan)
{
    return Liot_Ch390GetLinkStatus(gLan0Ch390Handle);
}

Liot_RouterLanDev_t gLan0 = {
    .init = atlan0_init,
    .send = atlan0_send,
    .receive = atlan0_receive,
    .getIntStatus = atlan0_get_int_status,
    .getLinkStatus = atlan0_get_link_status,
};