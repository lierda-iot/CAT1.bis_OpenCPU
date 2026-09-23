/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * WeChat voice-call facade for the small NT26F6D0 UI.
 */

#ifndef DEMO_WECHAT_H
#define DEMO_WECHAT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DEMO_WECHAT_OFFLINE = 0,
    DEMO_WECHAT_SYNCING,
    DEMO_WECHAT_READY,
    DEMO_WECHAT_RINGING,
    DEMO_WECHAT_DIALING,
    DEMO_WECHAT_CONNECTING,
    DEMO_WECHAT_WAIT_START,
    DEMO_WECHAT_IN_CALL,
    DEMO_WECHAT_STOPPING,
    DEMO_WECHAT_ERROR,
} demo_wechat_state_e;

typedef struct
{
    demo_wechat_state_e state;
    int error;
    uint8_t contact_count;
    uint8_t selected_contact;
    uint32_t contacts_revision;
    uint32_t incoming_generation;
    uint32_t session_sequence;
    uint32_t completed_sequence;
    uint32_t rx_dropped;
    uint32_t tx_dropped;
    char display_name[12];
} demo_wechat_snapshot_t;

/* Dedicated owner of WX signaling, contact cache and G.711 media. */
void demo_wechat_task(void *argv);
void demo_wechat_mark_unavailable(void);

/* UI facade.  Every operation is non-blocking. */
void demo_wechat_enter(void);
void demo_wechat_leave(void);
void demo_wechat_select_next(void);
bool demo_wechat_call_selected(void);
bool demo_wechat_answer(uint32_t incoming_generation);
void demo_wechat_hangup(void);
void demo_wechat_refresh_contacts(void);
void demo_wechat_get_snapshot(demo_wechat_snapshot_t *out);

/* HOME admission is explicit: incoming WX calls ring only while true. */
void demo_wechat_set_home_allowed(bool allowed);
bool demo_wechat_blocks_live(void);
bool demo_wechat_is_idle(void);

void demo_wechat_set_audio_levels(uint8_t speaker_level, uint8_t mic_level);

#endif /* DEMO_WECHAT_H */
