/**
 * @file ws2812b.h
 * @brief WS2812B LED strip driver interface
 * @details Provides initialization, color setting, and clear functions for WS2812B strips.
 *          Internally uses SPI bus to emulate WS2812B single-wire protocol timing.
 *
 * @note Usage example:
 * @code
 *   ws2812b_config_t cfg = {
 *       .spi_port   = 0,
 *       .spi_clk    = WS2812B_CLK_6_5MHZ,
 *       .led_num    = 8,
 *       .brightness = 128,
 *   };
 *   ws2812b_handle_t led = ws2812b_init(&cfg);
 *   ws2812b_set_color(led, -1, 255, 0, 0);  // Set all LEDs to red
 *   ws2812b_clear(led);                      // Turn off all LEDs
 *   ws2812b_deinit(led);                     // Release resources
 * @endcode
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#ifndef __WS2812B_H__
#define __WS2812B_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SPI clock frequency enum (based on 100MHz divider)
 */
typedef enum {
    WS2812B_CLK_812_5KHZ = 812500,   ///< 812.5KHz
    WS2812B_CLK_1_625MHZ = 1625000,  ///< 1.625MHz
    WS2812B_CLK_3_25MHZ  = 3250000,  ///< 3.25MHz
    WS2812B_CLK_6_5MHZ   = 6500000,  ///< 6.5MHz (recommended)
    WS2812B_CLK_13MHZ    = 13000000, ///< 13MHz
} ws2812b_clk_e;

/**
 * @brief WS2812B initialization config
 */
typedef struct {
    uint8_t         spi_port;    ///< SPI port number (0 or 1)
    ws2812b_clk_e   spi_clk;    ///< SPI clock frequency (recommend WS2812B_CLK_6_5MHZ)
    uint8_t         led_num;    ///< Number of LED pixels
    uint8_t         brightness; ///< Global brightness 0~255 (0=off, 255=max)
} ws2812b_config_t;

/** @brief WS2812B device handle (opaque pointer) */
typedef void *ws2812b_handle_t;

/**
 * @brief Initialize WS2812B device
 * @param[in] config Config pointer, must not be NULL
 * @return Device handle on success, NULL on failure
 */
ws2812b_handle_t ws2812b_init(const ws2812b_config_t *config);

/**
 * @brief Release WS2812B device resources
 * @param[in] handle Device handle, safely ignored if NULL
 */
void ws2812b_deinit(ws2812b_handle_t handle);

/**
 * @brief Set LED color and refresh display immediately
 * @param[in] handle Device handle
 * @param[in] index  LED index (0 ~ led_num-1), -1 to set all LEDs
 * @param[in] r      Red component 0~255
 * @param[in] g      Green component 0~255
 * @param[in] b      Blue component 0~255
 * @return 0 on success, -1 on failure (invalid handle or index out of range)
 */
int ws2812b_set_color(ws2812b_handle_t handle, int32_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Turn off all LEDs (equivalent to setting all colors to 0,0,0)
 * @param[in] handle Device handle
 * @return 0 on success, -1 on failure
 */
int ws2812b_clear(ws2812b_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812B_H__ */
