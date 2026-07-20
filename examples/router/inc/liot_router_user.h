/**
 * @file liot_router_user.h
 * @brief liotRouter 用户层配置 - 硬件引脚/功能开关/NV存储 + 用户接口
 */

#ifndef __LIOT_ROUTER_USER_H__
#define __LIOT_ROUTER_USER_H__

#include <stdint.h>

/*----------------------------------------------------------------------------*
 *                    网络默认值                                               *
 *----------------------------------------------------------------------------*/
#define LIOT_ROUTER_DEFAULT_GATEWAY_IP    0xC0A80101   /* 192.168.1.1 */
#define LIOT_ROUTER_DEFAULT_SUBNET_MASK   0xFFFFFF00   /* 255.255.255.0 */
#define LIOT_ROUTER_DEFAULT_LEASE_TIME    7200         /* DHCP租约秒数 */

/*----------------------------------------------------------------------------*
 *                    WAN配置                                                  *
 *----------------------------------------------------------------------------*/
#define LIOT_ROUTER_WAN_CID            1
#define LIOT_ROUTER_DEVICE_INDEX       0
#define LIOT_ROUTER_TASK_PRIORITY      32
#define LIOT_ROUTER_WAN_MAC    {0x30, 0x89, 0x84, 0x6A, 0x96, 0xAB}

/*----------------------------------------------------------------------------*
 *                    DHCP 地址池                                              *
 *----------------------------------------------------------------------------*/
#define LIOT_DHCP_POOL_SIZE       32

/*----------------------------------------------------------------------------*
 *                    CH390 #1 (SPI1) 引脚                                    *
 *----------------------------------------------------------------------------*/
#define LIOT_CH390_1_SPI_PORT          1
#define LIOT_CH390_1_RST_GPIO          2
#define LIOT_CH390_1_SSN_GPIO          12
#define LIOT_CH390_1_INT_WAKEUP_PAD    4

/*----------------------------------------------------------------------------*
 *                    CH390 #0 (SPI0) 引脚                                    *
 *----------------------------------------------------------------------------*/
#define LIOT_CH390_0_SPI_PORT          0
#define LIOT_CH390_0_RST_GPIO          1   /* instance1*16 + idx9 */
#define LIOT_CH390_0_SSN_GPIO          8    /* SPI0 SSN: instance0*16 + idx8 */
#define LIOT_CH390_0_INT_WAKEUP_PAD    3

/*----------------------------------------------------------------------------*
 *                    指示灯 (NPN 三极管驱动，GPIO 高=点亮)                     *
 *----------------------------------------------------------------------------*/
#define LIOT_LED_LAN0_GPIO_IDX         5    /* LED_LAN1: LAN0 初始化完成 */
#define LIOT_LED_LAN1_GPIO_IDX         6    /* LED_LAN2: LAN1 初始化完成 */
#define LIOT_LED_LTE_G_GPIO_IDX        4    /* LED_LTE_G: 网络正常(绿) */
#define LIOT_LED_LTE_R_GPIO_IDX        3    /* LED_LTE_R: 网络异常(红) */

/*----------------------------------------------------------------------------*
 *                    复位按键                                                 *
 *----------------------------------------------------------------------------*/
#define LIOT_ROUTER_RESET_WAKEUP_PAD   0
#define LIOT_ROUTER_RESET_LONG_PRESS_MS 5000

/*----------------------------------------------------------------------------*
 *                    NV配置                                                   *
 *----------------------------------------------------------------------------*/
#define LIOT_ROUTER_NV_FILE            "/router_cfg.dat"
#define LIOT_ROUTER_NV_MAGIC           0x4C524347  /* "LRCG" */
#define LIOT_ROUTER_NV_VERSION         1

/*----------------------------------------------------------------------------*
 *                    电源配置                                                   *
 *----------------------------------------------------------------------------*/
#define PIN_LDO33_EN 25

/*----------------------------------------------------------------------------*
 *                    用户接口                                                 *
 *----------------------------------------------------------------------------*/
void Liot_RouterStart(void);

/**
 * @brief Initialize the indicator LEDs (2x LAN + LTE green/red)
 * @note  All LEDs are active-high (NPN transistor). Initial state:
 *        LAN LEDs off, LTE red on (no network yet), LTE green off.
 */
void Liot_RouterLightInit(void);

/**
 * @brief Set a LAN port indicator LED
 * @param lanPort LAN port index (0 -> LED_LAN1, 1 -> LED_LAN2)
 * @param on      1 = on (init done), 0 = off
 */
void Liot_RouterLightLanSet(uint8_t lanPort, uint8_t on);

/**
 * @brief Set the LTE network status LED (green/red mutually exclusive)
 * @param normal 1 = network normal (green on/red off), 0 = abnormal (red on/green off)
 */
void Liot_RouterLightNetSet(uint8_t normal);

/**
 * @brief Initialize the reset button (long-press clears NV and reboots)
 */
void Liot_RouterResetInit(void);


#ifdef LIOT_ROUTER_WEB_ENABLE
/**
 * @brief Initialize the Web management service (registers API modules and
 *        static-asset routes, then starts the httpd listening on :80).
 * @details Must be called after the router core and LAN are initialized so the
 *          lwip socket layer is ready; see Liot_RouterStart().
 */
void Liot_WebInit(void);
#endif

#endif /* __LIOT_ROUTER_USER_H__ */
