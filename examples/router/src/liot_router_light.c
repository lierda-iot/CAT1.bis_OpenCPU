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
 * LED 状态逻辑：
 *   无 SIM / 无信号：红色常亮
 *   搜网注册中：绿色闪烁
 *   4G 驻网在线：绿色常亮
 */

#include "liot_router_user.h"
#include "liot_log.h"
#include "liot_gpio2.h"
#include "liot_nw.h"
#include "liot_os.h"

#define LIOT_ROUTER_LAN_LED_MAX      2
#define LIOT_NET_LED_BLINK_MS        500

/* LAN 口 -> LED GPIO 映射（active-high） */
static const liot_gpio_e s_lanLedGpio[LIOT_ROUTER_LAN_LED_MAX] = {
    (liot_gpio_e)LIOT_LED_LAN0_GPIO_IDX,   /* LAN0 -> LED_LAN1 */
    (liot_gpio_e)LIOT_LED_LAN1_GPIO_IDX,   /* LAN1 -> LED_LAN2 */
};

static liot_timer_t s_blinkTimer = NULL;

void Liot_RouterLightLanSet(uint8_t lanPort, uint8_t on)
{
    if (lanPort >= LIOT_ROUTER_LAN_LED_MAX)
        return;
    Liot_GpioSetLevel(s_lanLedGpio[lanPort], on ? L_IO_HIGH : L_IO_LOW);
}

static void liot_light_blink_cb(void *arg)
{
    (void)arg;
    static uint8_t s_blinkToggle = 0;
    s_blinkToggle = !s_blinkToggle;
    Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_G_GPIO_IDX, s_blinkToggle ? L_IO_HIGH : L_IO_LOW);
}

static void liot_light_update_led(liot_nw_reg_state_e state)
{
    if (s_blinkTimer != NULL)
    {
        liot_rtos_timer_stop(s_blinkTimer);
    }

    if (state == LIOT_NW_REG_STATE_TRYING_ATTACH_OR_SEARCHING)
    {
        /* 搜网注册中：绿色闪烁 */
        Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_R_GPIO_IDX, L_IO_LOW);
        liot_rtos_timer_start(s_blinkTimer, LIOT_NET_LED_BLINK_MS);
    }
    else if (state == LIOT_NW_REG_STATE_HOME_NETWORK || state == LIOT_NW_REG_STATE_ROAMING)
    {
        /* 4G 驻网在线：绿色常亮 */
        Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_G_GPIO_IDX, L_IO_HIGH);
        Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_R_GPIO_IDX, L_IO_LOW);
    }
    else
    {
        /* 无 SIM / 无信号：红色常亮 */
        Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_G_GPIO_IDX, L_IO_LOW);
        Liot_GpioSetLevel((liot_gpio_e)LIOT_LED_LTE_R_GPIO_IDX, L_IO_HIGH);
    }
}

static void liot_light_nw_cb(uint8_t nSim, unsigned int ind_type, void *ctx)
{
    (void)nSim;
    if (ind_type == LIOT_NW_DATA_REG_STATUS_IND && ctx != NULL)
    {
        liot_nw_common_reg_status_info_s *info = (liot_nw_common_reg_status_info_s *)ctx;
        liot_light_update_led(info->state);
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

    /* 创建闪烁定时器 */
    liot_rtos_timer_create(&s_blinkTimer, LIOT_TimerPeriodic, liot_light_blink_cb, NULL);

    /* 开机查询当前注册状态 */
    liot_nw_reg_status_info_s regInfo = {0};
    if (liot_nw_get_reg_status(0, &regInfo) == LIOT_NW_SUCCESS)
    {
        liot_light_update_led(regInfo.data_reg.state);
    }

    /* 注册状态变化回调 */
    liot_nw_register_cb(liot_light_nw_cb);

    liot_trace("light init done");
}
