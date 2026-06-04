/**
 * @File Name: liot_lcd.h
 * @brief  
 * @Author : Chenhz 
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-11-13
 * 
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 */

/***
 * LCD direction and width/height definitions:
 * 
 * width and height are set as .info.width and .info.height in liot_hal_lcdDev_t
 * direction is set as .info.direction in liot_hal_lcdDev_t
 * 
 * When LCD is in LIOT_LCD_DIR_0_ANGLE (0 degree counterclockwise rotation):
 * 
 *   X---------width--------->
 * Y (0, 0)
 * |
 * |
 * height
 * |
 * |
 * V    (width-1, height-1)
 * 
 * When LCD is in LIOT_LCD_DIR_90_ANGLE (90 degree counterclockwise rotation):
 * 
 *   X---------height--------->
 * Y (0, 0)
 * |
 * |
 * width
 * |
 * |
 * V    (height-1, width-1)
 * 
 */

#ifndef __LIOT_LCD_H__
#define __LIOT_LCD_H__

#include "liot_spi.h"
#include "liot_i2c.h"
#include "liot_pwm.h"
#include "stdbool.h"

#define RED                     (0x001f)
#define GREEN                   (0x07e0)
#define BLUE                    (0xf800)
#define WHITE                   (0xffff)
#define BLACK                   (0x0000)
#define YELLOW                  (0xffe0)
#define PURPLE                  (0x8010)
#define GOLDEN                  (0xFEA0)

#define LIOT_ADD_DISPLAY(lcdDev)    extern liot_hal_lcdDev_t lcdDev

#define LIOT_LSPI_6MHZ          (6*1024*1024)
#define LIOT_LSPI_13MHZ         (13*1024*1024)
#define LIOT_LSPI_25MHZ         (25*1024*1024)
#define LIOT_LSPI_33MHZ         (33*1024*1024)
#define LIOT_LSPI_51MHZ         (51*1024*1024)

typedef enum 
{
    LIOT_LCD_OK = 0,                /*!< success */
    LIOT_LCD_ERROR,                 /*!< general error */
    LIOT_LCD_NO_MEM,                /*!< no memory */
    LIOT_LCD_INVALID_PARAM,         /*!< invalid parameter */
    LIOT_LCD_INVALID_HANDLE,        /*!< invalid handle */
    LIOT_LCD_INVALID_INTERFACE,     /*!< invalid interface */
    LIOT_LCD_LOCATION_OVERFLOW,     /*!< location overflow */
}liot_lcd_errcode_e;

typedef enum
{
    LIOT_LSPI_PORT1 = 1, // LSPI1 Line
    LIOT_LSPI_PORT2,     // LSPI2 Line
} liot_lspi_port_e;

typedef enum
{
    LIOT_LSPI_CS0 = 52, // CS0
    LIOT_LSPI_CS1 = 67, // CS1
} liot_lspi_cs_e;

typedef enum
{
    LIOT_QSPI_DATA_LINE_1 = 0, /*!< 1-data-lane (standard SPI). */
    LIOT_QSPI_DATA_LINE_2,     /*!< 2-data-lane (dual SPI). */
    LIOT_QSPI_DATA_LINE_4,     /*!< 4-data-lane (quad SPI, PORT2 only). */
} liot_lcd_qspi_data_line_e;

typedef enum
{
    LIOT_LCD_INTERFACE_LSPI = 0x01,     /*!< LCD uses LSPI interface. */
    LIOT_LCD_INTERFACE_SPI  = 0x02,     /*!< LCD uses SPI interface. */
    LIOT_LCD_INTERFACE_I2C  = 0x04,     /*!< LCD uses I2C interface. */
    LIOT_LCD_INTERFACE_8080 = 0x08,     /*!< LCD uses 8080 interface. */
    LIOT_LCD_INTERFACE_6800 = 0x10,     /*!< LCD uses 6800 interface. */
    LIOT_LCD_INTERFACE_QSPI = 0x20,     /*!< LCD uses QSPI (multi-lane LSPI) interface. */
} liot_lcd_interface_type_e;

typedef enum
{
    LIOT_LCD_NO_BACKLIGHT = 0,  /*!< LCD has no backlight. */
    LIOT_LCD_BACKLIGHT_GPIO,    /*!< LCD backlight is controlled by GPIO. */
    LIOT_LCD_BACKLIGHT_PWM,     /*!< LCD backlight is controlled by PWM. */
} liot_lcd_blk_type_e;

typedef enum
{
    LIOT_LCD_DIR_0_ANGLE = 0,   /*!< LCD direction is 0 degrees. */
    LIOT_LCD_DIR_90_ANGLE,      /*!< LCD direction is 90 degrees. */
    LIOT_LCD_DIR_180_ANGLE,     /*!< LCD direction is 180 degrees. */
    LIOT_LCD_DIR_270_ANGLE,     /*!< LCD direction is 270 degrees. */
} liot_lcd_direction_e;

typedef enum
{
    LIOT_LCD_COLOR_RGB565 = 0,  /*!< LCD color depth is RGB565. */
    LIOT_LCD_COLOR_RGB555,      /*!< LCD color depth is RGB555. */
    LIOT_LCD_COLOR_RGB666,      /*!< LCD color depth is RGB666. not support */
    LIOT_LCD_COLOR_RGB888,      /*!< LCD color depth is RGB888. not support */
    LIOT_LCD_COLOR_GRAY16,      /*!< LCD color depth is 16 grayscale. */
    LIOT_LCD_COLOR_MONO,        /*!< LCD color depth is mono. */
}liot_lcd_color_depth_e;

typedef void (*liot_lcd_event_cb)(void);
typedef void* liot_lcd_handle_t;
typedef void (*liot_lcd_i2cSignal_cb) (uint32_t event);  ///< Pointer to \ref ARM_I2C_SignalEvent : Signal I2C Event.

typedef struct
{
    int (*init)(liot_lcd_handle_t handle);  /*!< LCD initialization. must be provided */
    int (*addrSet)(liot_lcd_handle_t handle,uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey);                /*!< LCD set display coordinates. */
    int (*fill)(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void* buf);       /*!< LCD display image in specified area. must be provided */
    int (*full)(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);  /*!< LCD display a color in specified area. must be provided */
    int (*strWrite)(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, char* str);   /*!< LCD display text in specified area. */
    int (*display_on)(liot_lcd_handle_t handle, bool on);   /*!< LCD display on/off. Optional */
    int (*sleep_in)(liot_lcd_handle_t handle, bool in);     /*!< LCD sleep in/out. Optional */
    int (*refresh)(liot_lcd_handle_t handle);   /*!< LCD refresh display, optional. */
    // The following are user-defined interfaces for I2C-type LCDs. If not NULL, they will replace the original interfaces in the program.
    int (*custom_i2c_cmd_send)(liot_lcd_handle_t handle, uint8_t *cmd, uint32_t len);       /*!< I2C interface class LCD user-defined command send interface. not support. */
    int (*custom_i2c_data_send)(liot_lcd_handle_t handle, uint8_t *data, uint32_t len);     /*!< I2C interface class LCD user-defined data send interface. not support. */
} liot_lcd_Device_func_t;

typedef struct
{
    uint16_t id;                        /*!< LCD driver ID. */
    uint32_t interface;                 /*!< LCD supported interfaces. */
    uint32_t width;                     /*!< LCD width in default state. */
    uint32_t height;                    /*!< LCD height in default state. */
    liot_lcd_direction_e direction;     /*!< LCD display direction. */
    liot_lcd_color_depth_e color_depth; /*!< LCD color depth. */
} liot_lcd_Device_info_t;

typedef struct
{
    liot_lspi_port_e num;       /*!< LSPI port number. */
    bool lcd_3_line_spi;        /*!< LSPI whether to enable 3-line mode, which affects whether the DCX pin is used. */
    bool lcd_2_data_lane;       /*!< LSPI whether to enable 2-data-lane, which requires the DCX pin to be enabled. */
    uint32_t speed;             /*!< LSPI bus speed. */
    bool sync;                  /*!< LSPI whether to enable synchronous operation, which waits for LSPI data transmission to complete before returning. */
    liot_lspi_cs_e cs;          /*!< LSPI CS引脚， 仅718M系列支持且只支持特定引脚 */
    liot_lcd_event_cb cb;       /*!< LSPI事件回调 */
} liot_lcd_interface_lspi_t;

typedef struct
{
    liot_spi_port_e num;        /*!< SPI port number. */
    liot_spi_cpol_pol_e cpol;   /*!< SPI clock polarity. */
    liot_spi_cpha_pol_e cpha;   /*!< SPI clock phase. */
    int8_t lcd_dc;              /*!< SPI DCX pin number. */
    int8_t cs;                  /*!< SPI CS pin number. */
    liot_spi_clk_e speed;       /*!< SPI bus speed. */
    bool dma_en;                /*!< SPI whether to enable DMA mode. */
    bool sync;                  /*!< SPI whether to enable synchronous operation, which waits for SPI data transmission to complete before returning. */
    liot_lcd_event_cb cb;       /*!< SPI event callback. */
} liot_lcd_interface_spi_t;

typedef struct
{
    liot_i2c_channel_e num;     /*!< I2C channel number. */   
    int8_t sda;                 /*!< I2C SDA pin number. */
    int8_t scl;                 /*!< I2C SCL pin number. */
    liot_i2c_mode_e speed;      /*!< I2C bus speed. */
    uint8_t addr;               /*!< I2C device address. */
    liot_lcd_i2cSignal_cb cb;       /*!< I2C event callback. */
} liot_lcd_interface_i2c_t;

typedef struct
{
    int8_t dataWidth;            /*!< 8080 interface data bus width, supports 8 and 16 bits. */
    int8_t cs;                   /*!< 8080 interface CS pin number. */
    int8_t dc;                   /*!< 8080 interface DC pin number. */
    int8_t wr;                   /*!< 8080 interface WR pin number. */
    int8_t rd;                   /*!< 8080 interface RD pin number. */
    int8_t data_gpio_start_num;  /*!< 8080 interface data bus start pin number. */
    int8_t data_gpio_end_num;    /*!< 8080 interface data bus end pin number. */
} liot_lcd_interface_8080_t;

typedef struct
{
    int8_t dataWidth;            /*!< 6800 interface data bus width, supports 8 and 16 bits. */
    int8_t cs;                   /*!< 6800 interface CS pin number. */
    int8_t rs;                   /*!< 6800 interface RS pin number. */
    int8_t rw;                   /*!< 6800 interface RW pin number. */
    int8_t data_gpio_start_num;  /*!< 6800 interface data bus start pin number. */
    int8_t data_gpio_end_num;    /*!< 6800 interface data bus end pin number. */
} liot_lcd_interface_6800_t;

typedef struct
{
    liot_lcd_blk_type_e type;       /*!< backlight type. */
    int8_t pin;                     /*!< backlight pin number. */
    liot_pwm_sel_e pwm_num;         /*!< If the backlight type is set to PWM, specify the PWM channel. */
} liot_lcd_blk_t;

typedef struct
{
    int8_t pin;                     /*!< LCD reset pin number. */
    uint16_t delay;                 /*!< LCD reset delay time. */
} liot_lcd_rst_t;

typedef struct
{
    liot_lspi_port_e num;                   /*!< LSPI port number. */
    liot_lspi_cs_e cs;                      /*!< CS pin, only EC718M series supports specific pins. */
    liot_lcd_qspi_data_line_e cmd_line;     /*!< Command/address lane count. */
    liot_lcd_qspi_data_line_e data_line;    /*!< Data lane count. */
    uint8_t instruction;                    /*!< MSPI instruction byte. */
    uint32_t speed;                         /*!< QSPI bus speed. */
    bool sync;                              /*!< Whether to block until DMA transfer completes. */
    liot_lcd_event_cb cb;                   /*!< Frame-done event callback. */
} liot_lcd_interface_qspi_t;

typedef struct
{
    liot_lcd_interface_type_e type;         /*!< LCD interface type. */
    union
    {
        liot_lcd_interface_lspi_t lspi;     /*!< LSPI configuration items. */
        liot_lcd_interface_spi_t spi;       /*!< SPI configuration items. */
        liot_lcd_interface_i2c_t i2c;       /*!< I2C configuration items, not supported. */
        liot_lcd_interface_8080_t l8080;    /*!< 8080 configuration items, not supported. */
        liot_lcd_interface_6800_t l6800;    /*!< 6800 configuration items, not supported. */
        liot_lcd_interface_qspi_t qspi;     /*!< QSPI configuration items. */
    };
    liot_lcd_blk_t blk;                     /*!< Backlight BLK configuration items. */
    liot_lcd_rst_t rst;                     /*!< LCD reset pin configuration items. */
} liot_lcd_interface_t;

typedef struct
{
    liot_lcd_Device_func_t func;    /*!< LCD driver function set. */
    liot_lcd_Device_info_t info;    /*!< LCD driver configuration information. */
} liot_lcd_Device_t;

typedef struct
{   
    liot_lcd_interface_t interface;     /*!< LCD interface configuration items. */
    liot_lcd_Device_t *lcdDev;              /*!< LCD driver, the LCD driver interface is set here. */
} liot_lcd_config_t;

typedef liot_lcd_Device_t liot_hal_lcdDev_t;

/**
 * @brief LCD init
 * @param[in] config LCD config
 * @return liot_lcd_handle_t LCD handle 
 */
liot_lcd_handle_t liot_lcd_init(liot_lcd_config_t *config);

/**
 * @brief LCD full screen refresh
 * @param[in] handle LCD handle
 * @param[in] color color
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_clear_screen(liot_lcd_handle_t handle, uint16_t color);

/**
 * @brief LCD draw point
 * @param[in] handle LCD handle
 * @param[in] x X axis coordinate
 * @param[in] y Y axis coordinate
 * @param[in] color color
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_draw_point(liot_lcd_handle_t handle, uint32_t x, uint32_t y, uint16_t color);

/**
 * @brief LCD draw line
 * 
 * @param[in] handle LCD handle
 * @param[in] sx X axis coordinate
 * @param[in] sy Y axis coordinate
 * @param[in] ex X axis coordinate
 * @param[in] ey Y axis coordinate
 * @param[in] color color
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_draw_line(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);

/**
 * @brief LCD draw rectangle
 * 
 * @param[in] handle LCD handle
 * @param[in] sx X axis coordinate
 * @param[in] sy Y axis coordinate
 * @param[in] ex X axis coordinate
 * @param[in] ey Y axis coordinate
 * @param[in] color color
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_draw_rectangle(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);

/**
 * @brief LCD draw circle
 * 
 * @param[in] handle LCD handle
 * @param[in] sx X axis coordinate
 * @param[in] sy Y axis coordinate
 * @param[in] r radius
 * @param[in] color color
 * @return liot_lcd_errcode_e error code 
 */
liot_lcd_errcode_e liot_lcd_draw_circle(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t r, uint16_t color);

/**
 * @brief LCD show image
 * 
 * @param[in] handle LCD handle
 * @param[in] sx X axis coordinate
 * @param[in] sy Y axis coordinate
 * @param[in] ex X axis coordinate
 * @param[in] ey Y axis coordinate
 * @param[in] buf image data address
 * @return liot_lcd_errcode_e error code    
 */
liot_lcd_errcode_e liot_lcd_write(liot_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint8_t *buf);

/**
 * @brief LCD Screen refresh, non-essential interface, suitable for LCD screens that require manual refresh
 *        Ultimately executes to lcdDev.func.refresh
 * 
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_refresh(liot_lcd_handle_t handle);

/**
 * @brief LCD set backlight brightness
 *        When LCD is configured with backlight BLK as LIOT_LCD_BACKLIGHT_PWM, the backlight brightness level can be set from 0 to 100
 *        When LCD is configured with backlight BLK as LIOT_LCD_BACKLIGHT_GPIO, the backlight brightness is only set to 0 or 1, any value greater than 1 is treated as 1
 *        When LCD is configured with backlight BLK as LIOT_LCD_NO_BACKLIGHT, this configuration item is invalid
 * @param[in] handle LCD handle
 * @param[in] level Brightness level 0-100
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_set_brightness(liot_lcd_handle_t handle, uint8_t level);

/**
 * @brief LCD enable display
 * 
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_display_on(liot_lcd_handle_t handle);

/**
 * @brief LCD disable display
 * 
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_display_off(liot_lcd_handle_t handle);

/**
 * @brief LCD enter sleep mode
 * 
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_sleep_in(liot_lcd_handle_t handle);

/**
 * @brief LCD exit sleep mode
 * 
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_sleep_out(liot_lcd_handle_t handle);

/**
 * @brief LCD send cmd
 * 
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_send_cmd(liot_lcd_handle_t handle, uint8_t cmd, void *data, uint16_t size);

/**
 * @brief LCD send data
 *
 * @param[in] handle LCD handle
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_send(liot_lcd_handle_t handle, uint8_t *data, uint32_t length);

/**
 * @brief Set MSPI (multi-lane SPI) lane configuration at runtime.
 *        Only effective when the LCD interface type is LIOT_LCD_INTERFACE_QSPI
 *        and the chip is EC718M (TYPE_EC718M). Returns LIOT_LCD_ERROR on
 *        unsupported chips or interface types.
 *
 * @param[in] handle      LCD handle
 * @param[in] enable      1 to enable MSPI mode, 0 to disable
 * @param[in] addrLane    Address/command lane count
 * @param[in] dataLane    Data lane count
 * @param[in] instruction MSPI instruction byte
 * @return liot_lcd_errcode_e error code
 */
liot_lcd_errcode_e liot_lcd_set_mspi(liot_lcd_handle_t handle, uint8_t enable,
                                      uint8_t addrLane, uint8_t dataLane,
                                      uint8_t instruction);

#endif // !__LIOT_LCD_H__
