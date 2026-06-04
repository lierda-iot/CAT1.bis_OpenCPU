/**
 * @file ws2812b.c
 * @brief WS2812B LED strip driver implementation
 * @details Uses SPI bus to emulate WS2812B single-wire protocol timing for LED color control.
 *
 *          WS2812B protocol:
 *          - Single-wire serial, each LED receives 24-bit data (G8R8B8, MSB first)
 *          - Logic "0": high ~0.4us + low ~0.85us
 *          - Logic "1": high ~0.8us + low ~0.45us
 *          - This driver uses SPI MOSI to emulate the timing, recommended 6.5MHz clock,
 *            each protocol bit represented by 1 SPI byte (8 SPI bits ~ 1.23us)
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#include "ws2812b.h"
#include "liot_spi.h"
#include "liot_os.h"
#include <string.h>

/* Logic "0" SPI byte: 11000000b, 2-bit high (~0.3us), 6-bit low (~0.9us) */
#define WS2812B_SPI_BIT_L  0xC0
/* Logic "1" SPI byte: 11111100b, 6-bit high (~0.9us), 2-bit low (~0.3us) */
#define WS2812B_SPI_BIT_H  0xFC
/* SPI encoded bytes per LED: 24-bit color x 1 byte/bit = 24 bytes */
#define WS2812B_BYTES_PER_LED  24

/** @brief RGB color value (internal) */
typedef struct {
    uint8_t r;  ///< Red component
    uint8_t g;  ///< Green component
    uint8_t b;  ///< Blue component
} ws2812b_color_t;

/** @brief WS2812B device control block */
typedef struct {
    ws2812b_config_t  config;     ///< User config copy
    ws2812b_color_t  *color_buf;  ///< RGB color buffer (led_num elements)
    uint8_t          *spi_buf;    ///< SPI transmit buffer (led_num x 24 bytes)
    uint8_t           brightness; ///< Global brightness 0~255
} ws2812b_dev_t;

/**
 * @brief Encode single pixel RGB values to SPI waveform data
 * @param[out] out        Output buffer, at least 24 bytes
 * @param[in]  r          Red component raw value
 * @param[in]  g          Green component raw value
 * @param[in]  b          Blue component raw value
 * @param[in]  brightness Global brightness (scales color values proportionally)
 *
 * @note WS2812B data order is G-R-B, MSB first
 */
static void ws2812b_encode_pixel(uint8_t *out, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    uint8_t gr = (uint16_t)g * brightness / 255;
    uint8_t rr = (uint16_t)r * brightness / 255;
    uint8_t br = (uint16_t)b * brightness / 255;

    for (int i = 0; i < 8; i++) {
        out[i]      = (gr & (1 << (7 - i))) ? WS2812B_SPI_BIT_H : WS2812B_SPI_BIT_L;
        out[8 + i]  = (rr & (1 << (7 - i))) ? WS2812B_SPI_BIT_H : WS2812B_SPI_BIT_L;
        out[16 + i] = (br & (1 << (7 - i))) ? WS2812B_SPI_BIT_H : WS2812B_SPI_BIT_L;
    }
}

/**
 * @brief Encode color buffer to SPI waveform and transmit
 * @param[in] dev Device control block pointer
 * @return 0 on success, non-zero on failure
 */
static int ws2812b_refresh(ws2812b_dev_t *dev)
{
    for (uint32_t i = 0; i < dev->config.led_num; i++) {
        ws2812b_encode_pixel(&dev->spi_buf[i * WS2812B_BYTES_PER_LED],
                             dev->color_buf[i].r,
                             dev->color_buf[i].g,
                             dev->color_buf[i].b,
                             dev->brightness);
    }

    uint32_t total_bytes = dev->config.led_num * WS2812B_BYTES_PER_LED;
    return liot_spi_write((liot_spi_port_e)dev->config.spi_port, dev->spi_buf, total_bytes);
}

/**
 * @brief Initialize WS2812B device
 */
ws2812b_handle_t ws2812b_init(const ws2812b_config_t *config)
{
    if (!config || config->led_num == 0)
        return NULL;

    ws2812b_dev_t *dev = liot_rtos_malloc(sizeof(ws2812b_dev_t));
    if (!dev)
        return NULL;

    memset(dev, 0, sizeof(ws2812b_dev_t));
    dev->config     = *config;
    dev->brightness = config->brightness;

    dev->color_buf = liot_rtos_malloc(config->led_num * sizeof(ws2812b_color_t));
    dev->spi_buf   = liot_rtos_malloc(config->led_num * WS2812B_BYTES_PER_LED);
    if (!dev->color_buf || !dev->spi_buf) {
        if (dev->color_buf) liot_rtos_free(dev->color_buf);
        if (dev->spi_buf)   liot_rtos_free(dev->spi_buf);
        liot_rtos_free(dev);
        return NULL;
    }

    memset(dev->color_buf, 0, config->led_num * sizeof(ws2812b_color_t));
    memset(dev->spi_buf, 0, config->led_num * WS2812B_BYTES_PER_LED);

    liot_spi_init((liot_spi_port_e)config->spi_port,
                  LIOT_SPI_DMA_IRQ,
                  (liot_spi_clk_e)config->spi_clk);

    return (ws2812b_handle_t)dev;
}

/**
 * @brief Release WS2812B device resources
 */
void ws2812b_deinit(ws2812b_handle_t handle)
{
    if (!handle)
        return;

    ws2812b_dev_t *dev = (ws2812b_dev_t *)handle;

    liot_spi_release((liot_spi_port_e)dev->config.spi_port);
    liot_rtos_free(dev->color_buf);
    liot_rtos_free(dev->spi_buf);
    liot_rtos_free(dev);
}

/**
 * @brief Set LED color and refresh display immediately
 */
int ws2812b_set_color(ws2812b_handle_t handle, int32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!handle)
        return -1;

    ws2812b_dev_t *dev = (ws2812b_dev_t *)handle;

    if (index == -1) {
        for (uint32_t i = 0; i < dev->config.led_num; i++) {
            dev->color_buf[i].r = r;
            dev->color_buf[i].g = g;
            dev->color_buf[i].b = b;
        }
    } else {
        if ((uint32_t)index >= dev->config.led_num)
            return -1;
        dev->color_buf[index].r = r;
        dev->color_buf[index].g = g;
        dev->color_buf[index].b = b;
    }

    return ws2812b_refresh(dev);
}

/**
 * @brief Turn off all LEDs
 */
int ws2812b_clear(ws2812b_handle_t handle)
{
    return ws2812b_set_color(handle, -1, 0, 0, 0);
}
