/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * Small, non-blocking UI facade for the TiRTC AI Chat session task.
 */

#ifndef DEMO_AI_CHAT_H
#define DEMO_AI_CHAT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DEMO_AI_CHAT_IDLE = 0,
    DEMO_AI_CHAT_GET_TOKEN,
    DEMO_AI_CHAT_START_SDK,
    DEMO_AI_CHAT_CONNECTING,
    DEMO_AI_CHAT_NEGOTIATING,
    DEMO_AI_CHAT_LISTENING,
    DEMO_AI_CHAT_SPEAKING,
    DEMO_AI_CHAT_STOPPING,
    DEMO_AI_CHAT_ERROR,
} demo_ai_chat_state_e;

typedef struct
{
    demo_ai_chat_state_e state;
    int error;
    uint32_t rx_dropped;
    uint32_t tx_dropped;
    uint32_t completed_request_id;
    int completed_result;
} demo_ai_chat_snapshot_t;

/* AI session owner.  The long-lived TiRTC runtime is owned by demo_tirtc. */
void demo_ai_chat_task(void *argv);
void demo_ai_chat_mark_unavailable(void);

/* UI-safe hint to prepare one short-lived credential for the next session. */
void demo_ai_chat_prepare(void);
/* Non-blocking start request.  The returned id identifies this session. */
uint32_t demo_ai_chat_start(void);
void demo_ai_chat_stop(void);
/* True only after the worker has acknowledged stop and released audio. */
bool demo_ai_chat_is_idle(void);
void demo_ai_chat_get_snapshot(demo_ai_chat_snapshot_t *out);

/* Speaker and microphone levels use the same user-facing 1..10 scale. */
void demo_ai_chat_set_audio_levels(uint8_t speaker_level, uint8_t mic_level);

#endif /* DEMO_AI_CHAT_H */
