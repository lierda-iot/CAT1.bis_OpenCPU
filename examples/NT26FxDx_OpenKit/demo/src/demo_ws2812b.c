/**
 * @file demo_ws2812b.c
 * @brief WS2812B RGB LED demonstration via SPI
 *
 * Demonstrates:
 * 1. WS2812B initialization via SPI
 * 2. HSV to RGB888 color conversion
 * 3. Cycling color display across all LEDs
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_spi.h"
#include "liot_log.h"

#define WS2812B_SPI_PORT    (0)
#define WS2812B_SPI_BIT_L   (0xC0)
#define WS2812B_SPI_BIT_H   (0xFC)
#define WS2812B_LED_NUM     (3)

typedef struct
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
} rgb_color_t;

#define RGB_SET(color, index, r, g, b) \
    do { (color)[index].R = (r); (color)[index].G = (g); (color)[index].B = (b); } while(0)

/**
 * @brief Convert HSV color to RGB888
 * @param[in] h  Hue (0-255)
 * @param[in] s  Saturation (0-255)
 * @param[in] v  Value/brightness (0-255)
 * @return RGB color struct
 */
static rgb_color_t _hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    rgb_color_t rgb;
    uint8_t region, remainder, p, q, t;

    if (s == 0)
    {
        rgb.R = rgb.G = rgb.B = v;
        return rgb;
    }

    region    = h / 43;
    remainder = (h - (region * 43)) * 6;
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0: rgb.R = v; rgb.G = t; rgb.B = p; break;
        case 1: rgb.R = q; rgb.G = v; rgb.B = p; break;
        case 2: rgb.R = p; rgb.G = v; rgb.B = t; break;
        case 3: rgb.R = p; rgb.G = q; rgb.B = v; break;
        case 4: rgb.R = t; rgb.G = p; rgb.B = v; break;
        default: rgb.R = v; rgb.G = p; rgb.B = q; break;
    }
    return rgb;
}

/**
 * @brief Pack one LED color into SPI bit-bang buffer (GRB order)
 * @param[in]  color  RGB color
 * @param[out] data   24-byte output buffer
 */
static void _ws2812b_pack(rgb_color_t color, uint8_t *data)
{
    for (int i = 0; i <= 7; i++)
    {
        data[i]    = (color.G & (1 << (7 - i))) ? WS2812B_SPI_BIT_H : WS2812B_SPI_BIT_L;
        data[8+i]  = (color.R & (1 << (7 - i))) ? WS2812B_SPI_BIT_H : WS2812B_SPI_BIT_L;
        data[16+i] = (color.B & (1 << (7 - i))) ? WS2812B_SPI_BIT_H : WS2812B_SPI_BIT_L;
    }
}

/**
 * @brief Send color data to WS2812B LED strip
 * @param[in] color  Array of RGB colors
 * @param[in] num    Number of LEDs
 */
static void _ws2812b_send(rgb_color_t *color, uint32_t num)
{
    uint8_t *buf = liot_rtos_malloc(24 * num);
    if (buf == NULL)
    {
        liot_trace("ws2812b: malloc failed");
        return;
    }
    for (int i = 0; i < num; i++)
        _ws2812b_pack(color[i], &buf[i * 24]);

    liot_spi_write(WS2812B_SPI_PORT, buf, 24 * num);
    liot_rtos_free(buf);
}

/**
 * @brief WS2812B demo task entry
 * @param[in] argv  Task argument (unused)
 */
void demo_ws2812b_task(void *argv)
{
    liot_trace("demo_ws2812b_task start");
    liot_rtos_task_sleep_ms(5000);

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    /* LDO enable */
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);

    liot_spi_init(WS2812B_SPI_PORT, LIOT_SPI_DMA_IRQ, 5000000U);

    rgb_color_t colors[WS2812B_LED_NUM] = {0};
    uint8_t R = 0x00, G = 0x00, B = 0x00;

    while (1)
    {
        liot_trace("ws2812b task ");
        for (int i = 0; i < WS2812B_LED_NUM; i++)
            RGB_SET(colors, i, R, G, B);

        _ws2812b_send(colors, WS2812B_LED_NUM);

        R += 0x20;
        G += 0x40;
        B += 0x60;

        liot_rtos_task_sleep_ms(500);
    }
}
