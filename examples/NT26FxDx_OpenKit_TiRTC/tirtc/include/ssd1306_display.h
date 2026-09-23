/*
 * SPDX-FileCopyrightText: 2025 Lierda Technology Co., Ltd.
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board-specific SSD1306 initialization and framebuffer drawing primitives.
 */

#ifndef TIRTC_APP_SSD1306_DISPLAY_H
#define TIRTC_APP_SSD1306_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

int demo_oled_init(void);
bool demo_oled_is_ready(void);

void demo_oled_begin_frame(void);
void demo_oled_draw_text(uint32_t x, uint32_t y,
                         const char *text, uint8_t scale);
void demo_oled_draw_text_centered(uint32_t y,
                                  const char *text, uint8_t scale);
void demo_oled_end_frame(void);

#endif /* TIRTC_APP_SSD1306_DISPLAY_H */
