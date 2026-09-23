/*
 * SPDX-FileCopyrightText: 2023 Lierda Science & Technology Group Co., Ltd.
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ssd1306_display.c
 * @brief Board-specific SSD1306 display service.
 *
 * This module owns only the OLED hardware and framebuffer primitives.
 * Menu state, keys, network and TiRTC actions live in separate modules.
 * Based on Lierda's SSD1306 board example by Chenhz
 * <ciot_iot_support@lierda.com>, then adapted for the product UI.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_i2c.h"
#include "liot_lcd.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "ssd1306_display.h"

LIOT_ADD_DISPLAY(liot_ssd1306_12864_dev);

#define OLED_DRIVER_VERSION       "SSD1306_SERVICE_V1_20260825"
#define SSD1306_I2C_NUM           1
#define SSD1306_I2C_ADDR_0        0x3C
#define SSD1306_I2C_ADDR_1        0x3D
#define SSD1306_I2C_SDA_PIN       57
#define SSD1306_I2C_SDA_FUNC      3
#define SSD1306_I2C_SCL_PIN       66
#define SSD1306_I2C_SCL_FUNC      2

static const uint8_t s_font_digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static const uint8_t s_font_upper[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x7F, 0x20, 0x18, 0x20, 0x7F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x03, 0x04, 0x78, 0x04, 0x03},
    {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t s_font_blank[5]   = {0, 0, 0, 0, 0};
static const uint8_t s_font_colon[5]   = {0x00, 0x36, 0x36, 0x00, 0x00};
static const uint8_t s_font_hyphen[5]  = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t s_font_plus[5]    = {0x08, 0x08, 0x3E, 0x08, 0x08};
static const uint8_t s_font_greater[5] = {0x00, 0x41, 0x22, 0x14, 0x08};

static liot_lcd_handle_t s_oled;
static bool s_oled_ready;

static void oled_prepare_bus(void)
{
    Liot_SetPinFunc(SSD1306_I2C_SCL_PIN, SSD1306_I2C_SCL_FUNC);
    Liot_SetPinFunc(SSD1306_I2C_SDA_PIN, SSD1306_I2C_SDA_FUNC);
}

static const uint8_t *oled_get_glyph(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return s_font_digits[(uint8_t)(ch - '0')];
    }
    if (ch >= 'A' && ch <= 'Z')
    {
        return s_font_upper[(uint8_t)(ch - 'A')];
    }

    switch (ch)
    {
    case ':':
        return s_font_colon;
    case '-':
        return s_font_hyphen;
    case '+':
        return s_font_plus;
    case '>':
        return s_font_greater;
    default:
        return s_font_blank;
    }
}

static void oled_draw_char(uint32_t x, uint32_t y, char ch, uint8_t scale)
{
    const uint8_t *glyph;
    uint8_t col;
    uint8_t row;
    uint8_t dx;
    uint8_t dy;

    if (!s_oled_ready || s_oled == NULL || scale == 0U)
    {
        return;
    }

    glyph = oled_get_glyph(ch);
    for (col = 0; col < 5U; col++)
    {
        for (row = 0; row < 7U; row++)
        {
            if ((glyph[col] & (1U << row)) == 0U)
            {
                continue;
            }

            for (dx = 0; dx < scale; dx++)
            {
                for (dy = 0; dy < scale; dy++)
                {
                    liot_lcd_draw_point(s_oled,
                                        x + (uint32_t)col * scale + dx,
                                        y + (uint32_t)row * scale + dy,
                                        WHITE);
                }
            }
        }
    }
}

int demo_oled_init(void)
{
    int ret;
    int ret_3c;
    int ret_3d;
    uint8_t probe_cmd = 0xAE;
    liot_hal_lcdDev_t *lcd_dev = &liot_ssd1306_12864_dev;
    LiotSleepModeCfg_t sleep_cfg = {LIOT_SLEEP_MODE_NORMAL};
    liot_lcd_config_t cfg = {
        .interface = {
            .type = LIOT_LCD_INTERFACE_I2C,
            .i2c = {
                .num = SSD1306_I2C_NUM,
                .scl = SSD1306_I2C_SCL_PIN,
                .sda = SSD1306_I2C_SDA_PIN,
                .speed = LIOT_STANDARD_MODE,
                .addr = SSD1306_I2C_ADDR_0,
                .cb = NULL,
            },
            .blk.type = LIOT_LCD_NO_BACKLIGHT,
            .blk.pin = -1,
            .rst.pin = -1,
            .rst.delay = 0,
        },
        .lcdDev = lcd_dev,
    };

    s_oled_ready = false;

    liot_trace("[OLED] driver=%s\r\n", OLED_DRIVER_VERSION);

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_SleepSetMode(&sleep_cfg);
    liot_rtos_task_sleep_ms(300);

    oled_prepare_bus();
    ret = liot_I2cInit(SSD1306_I2C_NUM, LIOT_STANDARD_MODE);
    liot_trace("[OLED] I2C init ret=%d\r\n", ret);
    if (ret != LIOT_I2C_SUCCESS)
    {
        return -1;
    }

    ret_3c = liot_I2cWrite(SSD1306_I2C_NUM, SSD1306_I2C_ADDR_0,
                           0x00, &probe_cmd, 1);
    ret_3d = liot_I2cWrite(SSD1306_I2C_NUM, SSD1306_I2C_ADDR_1,
                           0x00, &probe_cmd, 1);
    liot_I2cRelease(SSD1306_I2C_NUM);
    liot_trace("[OLED] probe 0x3C=%d 0x3D=%d\r\n", ret_3c, ret_3d);

    if (ret_3c == LIOT_I2C_SUCCESS)
    {
        cfg.interface.i2c.addr = SSD1306_I2C_ADDR_0;
    }
    else if (ret_3d == LIOT_I2C_SUCCESS)
    {
        cfg.interface.i2c.addr = SSD1306_I2C_ADDR_1;
    }
    else
    {
        liot_trace("[OLED] no device at 0x3C/0x3D\r\n");
        return -2;
    }

    oled_prepare_bus();
    s_oled = liot_lcd_init(&cfg);
    if (s_oled == NULL)
    {
        liot_trace("[OLED] liot_lcd_init failed\r\n");
        return -3;
    }

    /* liot_lcd_init() creates the framebuffer/transport handle.  This board
     * has no separate OLED reset pin, and the verified NT26F6D0 module needs
     * one deterministic controller init after the address probe. */
    oled_prepare_bus();
    if (lcd_dev->func.init == NULL)
    {
        liot_trace("[OLED] SSD1306 driver has no init callback\r\n");
        return -4;
    }
    ret = lcd_dev->func.init(s_oled);
    liot_trace("[OLED] SSD1306 reinit ret=%d addr=0x%02X\r\n",
               ret, cfg.interface.i2c.addr);
    if (ret != 0)
    {
        return -5;
    }

    /*
     * Keep the controller sequence used by the verified _bak_0903 build.
     * On this I2C LCD backend liot_lcd_send_cmd() can return 1 even though
     * the command was accepted by the SSD1306.  In particular, treating the
     * 0xA5 result as fatal leaves the panel in "entire display ON" mode and
     * produces a permanent white screen.  Always restore RAM display mode
     * with 0xA4 before rendering the first frame.
     */
    ret = liot_lcd_send_cmd(s_oled, 0xA5, NULL, 0);
    liot_trace("[OLED] display-test command ret=%d\r\n", ret);
    liot_rtos_task_sleep_ms(300);
    oled_prepare_bus();
    ret = liot_lcd_send_cmd(s_oled, 0xA4, NULL, 0);
    liot_trace("[OLED] RAM-display command ret=%d\r\n", ret);
    ret = liot_lcd_send_cmd(s_oled, 0xA7, NULL, 0);
    liot_trace("[OLED] inverse-display command ret=%d\r\n", ret);

    /* This verified panel is clearest with bright background/dark text. */
    s_oled_ready = true;
    demo_oled_begin_frame();
    demo_oled_draw_text_centered(26, "UI READY", 1);
    demo_oled_end_frame();

    if (!s_oled_ready)
    {
        return -6;
    }

    return 0;
}

bool demo_oled_is_ready(void)
{
    return s_oled_ready && s_oled != NULL;
}

void demo_oled_begin_frame(void)
{
    if (s_oled_ready && s_oled != NULL)
    {
        oled_prepare_bus();
        /* The verified vendor SSD1306 path owns its framebuffer internally.
         * Keep drawing enabled even if a board backend reports a non-zero
         * transport value after accepting the operation. */
        (void)liot_lcd_clear_screen(s_oled, BLACK);
    }
}

void demo_oled_draw_text(uint32_t x, uint32_t y,
                         const char *text, uint8_t scale)
{
    size_t i;

    if (text == NULL)
    {
        return;
    }

    for (i = 0; text[i] != '\0'; i++)
    {
        oled_draw_char(x, y, text[i], scale);
        x += 6U * scale;
    }
}

void demo_oled_draw_text_centered(uint32_t y,
                                  const char *text, uint8_t scale)
{
    size_t len;
    uint32_t width;
    uint32_t x;

    if (text == NULL)
    {
        return;
    }

    len = strlen(text);
    width = (uint32_t)len * 6U * scale;
    x = (width < 128U) ? (128U - width) / 2U : 0U;
    demo_oled_draw_text(x, y, text, scale);
}

void demo_oled_end_frame(void)
{
    if (s_oled_ready && s_oled != NULL)
    {
        oled_prepare_bus();
        (void)liot_lcd_refresh(s_oled);
    }
}
