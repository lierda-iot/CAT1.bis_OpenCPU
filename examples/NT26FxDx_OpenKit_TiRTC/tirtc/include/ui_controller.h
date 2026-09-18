/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Three-key UI event contract and the OLED controller task interface.
 */

#ifndef TIRTC_APP_UI_CONTROLLER_H
#define TIRTC_APP_UI_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DEMO_UI_KEY_NEXT = 0,
    DEMO_UI_KEY_CONFIRM,
    DEMO_UI_KEY_BACK,
} demo_ui_key_e;

typedef enum
{
    DEMO_UI_FEATURE_AI_CHAT = 0,
    DEMO_UI_FEATURE_WECHAT,
    DEMO_UI_FEATURE_DEV_CHAT,
} demo_ui_feature_e;

int demo_ui_init(void);
void demo_ui_deinit(void);
void demo_ui_task(void *argv);

void demo_ui_post_key_from_isr(demo_ui_key_e key);

#endif /* TIRTC_APP_UI_CONTROLLER_H */
