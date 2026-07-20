/**
 * @file liot_router_light.c
 * @brief 指示灯模块 - 2x LAN 口灯 + LTE 绿/红网络状态灯
 *
 * 硬件：全部为 NPN 三极管驱动，GPIO 高电平点亮。
 *   LED_LAN1 (GPIO5) - LAN0 初始化完成
 *   LED_LAN2 (GPIO6) - LAN1 初始化完成
 *   LED_LTE_G(GPIO4) - 网络正常(绿)
 *   LED_LTE_R(GPIO3) - 网络异常(红)
 *
 * @note LTE 状态：开机用 liot_nw_get_reg_status() 查一次设初值（解决开机已注网
 *       却收不到事件的问题），再用 liot_nw_register_cb() 跟踪后续变化。不使用
 *       Liot_PsEventCb（其单回调会被核心引擎 wan.c 覆盖），也不用定时器/常驻任务。
 */

#include "liot_router_user.h"
#include "liot_log.h"
#include "liot_gpio2.h"
#include "liot_nw.h"

#define LIOT_ROUTER_LAN_LED_MAX      2

/* LAN 口 -> LED GPIO 映射（active-high） */
static const liot_gpio_e s_lanLedGpio[LIOT_ROUTER_LAN_LED_MAX] = {
    (liot_gpio_e)LIOT_LED_LAN0_GPIO_IDX,   /* LAN0 -> LED_LAN1 */
    (liot_gpio_e)LIOT_LED_LAN1_GPIO_IDX,   /* LAN1 -> LED_LAN2 */
};

void Liot_RouterLightLanSet(uint8_t lanPort, uint8_t on)
{
    if (lanPort >= LIOT_ROUTER_LAN_LED_MAX)
        return;
    Liot_GpioSetLevel(s_lanLedGpio[lanPort], on ? L_IO_HIGH : L_IO_LOW);
}

void Liot_RouterLightNetSet(uint8_t normal)
{
    /* 绿/红互斥：正常=绿亮红灭，异常=红亮绿灭 */
    Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_G_GPIO_IDX, normal ? L_IO_HIGH : L_IO_LOW);
    Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_R_GPIO_IDX, normal ? L_IO_LOW  : L_IO_HIGH);
}

/* 注册状态视为在线：本地网 / 漫游 */
static uint8_t liot_light_state_is_online(liot_nw_reg_state_e state)
{
    return (state == LIOT_NW_REG_STATE_HOME_NETWORK ||
            state == LIOT_NW_REG_STATE_ROAMING) ? 1 : 0;
}

/* 网络注册状态变化回调（跟踪后续掉网/注网） */
static void liot_light_nw_cb(uint8_t nSim, unsigned int ind_type, void *ctx)
{
    (void)nSim;
    if (ind_type == LIOT_NW_DATA_REG_STATUS_IND && ctx != NULL) {
        liot_nw_common_reg_status_info_s *info = (liot_nw_common_reg_status_info_s *)ctx;
        Liot_RouterLightNetSet(liot_light_state_is_online(info->state));
    }
}

void Liot_RouterLightInit(void)
{
    /* LAN 口灯：初始熄灭 */
    Liot_GpioInit((liot_gpio_e)LIOT_LED_LAN0_GPIO_IDX, L_IO_OUTPUT, L_IO_LOW, NULL);
    Liot_GpioInit((liot_gpio_e)LIOT_LED_LAN1_GPIO_IDX, L_IO_OUTPUT, L_IO_LOW, NULL);

    /* LTE 灯：默认红亮绿灭 */
    Liot_GpioInit((liot_gpio_e)LIOT_LED_LTE_G_GPIO_IDX, L_IO_OUTPUT, L_IO_LOW,  NULL);
    Liot_GpioInit((liot_gpio_e)LIOT_LED_LTE_R_GPIO_IDX, L_IO_OUTPUT, L_IO_HIGH, NULL);

    /* 开机主动查一次当前注册状态，设正确初值 */
    liot_nw_reg_status_info_s regInfo = {0};
    if (liot_nw_get_reg_status(0, &regInfo) == LIOT_NW_SUCCESS) {
        uint8_t online = liot_light_state_is_online(regInfo.data_reg.state);
        Liot_RouterLightNetSet(online);
        liot_trace("light: init LTE %s (state=%d)",
                   online ? "GREEN" : "RED", regInfo.data_reg.state);
    }

    /* 注册状态变化回调，跟踪后续注网/掉网 */
    liot_nw_register_cb(liot_light_nw_cb);

    liot_trace("light init done");
}
