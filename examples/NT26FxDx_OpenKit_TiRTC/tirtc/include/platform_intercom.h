/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * Platform LIVE voice-talk service for the shared TiRTC runtime.
 */

#ifndef DEMO_LIVE_TALK_H
#define DEMO_LIVE_TALK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allow passive platform connections only while the product UI is at HOME. */
void demo_live_talk_set_home_allowed(bool allowed);

/* Ten user-facing levels. Values outside 1..10 are clamped by audio adapter. */
void demo_live_talk_set_audio_levels(uint8_t speaker_level,
                                     uint8_t mic_level);

/* True after an accepted platform connection has acquired the audio path. */
bool demo_live_talk_is_active(void);

/*
 * Wait until an accepted/pending LIVE session has completed audio stop and
 * connection cleanup.  Intended for deterministic HOME -> AI/WX/DEV handoff.
 */
bool demo_live_talk_wait_idle(uint32_t timeout_ms);

/* Wait until the passive incoming router and its fixed queues are ready. */
bool demo_live_talk_wait_ready(uint32_t timeout_ms);

/* Long-lived LIVE session owner. */
void demo_live_talk_task(void *argv);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_LIVE_TALK_H */
