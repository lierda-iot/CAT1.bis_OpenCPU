/* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 */
#ifndef DEMO_GROUP_INTERCOM_H
#define DEMO_GROUP_INTERCOM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DEMO_GROUP_IDLE = 0,
    DEMO_GROUP_SYNCING,
    DEMO_GROUP_NO_ROOM,
    DEMO_GROUP_WAIT_RESOURCE,
    DEMO_GROUP_CONNECTING,
    DEMO_GROUP_JOINING,
    DEMO_GROUP_LISTENING,
    DEMO_GROUP_MIC_ON,
    DEMO_GROUP_SUSPENDING,
    DEMO_GROUP_SUSPENDED,
    DEMO_GROUP_RECONNECTING,
    DEMO_GROUP_STOPPING,
    DEMO_GROUP_ERROR
} demo_group_state_e;

typedef struct {
    demo_group_state_e state;
    int error;
    char room_code[7];
    bool mic_on;
    bool enabled;
    uint32_t generation;
    uint32_t rx_dropped;
    uint32_t tx_dropped;
} demo_group_snapshot_t;

typedef enum {
    DEMO_GROUP_CALL_NONE = 0,
    DEMO_GROUP_CALL_WECHAT,
    DEMO_GROUP_CALL_DEVICE
} demo_group_call_e;

/* Init registers routers before binding starts. No callback owns hardware. */
int demo_group_intercom_init(void);
void demo_group_intercom_enter(void);
void demo_group_intercom_exit(void);
void demo_group_intercom_toggle_mic(void);
bool demo_group_intercom_toggle_mic_for_generation(uint32_t generation);
void demo_group_intercom_retry(void);
void demo_group_intercom_set_foreground_allowed(bool allowed);
void demo_group_intercom_set_audio_levels(uint8_t speaker, uint8_t mic);
bool demo_group_intercom_is_idle(void);
bool demo_group_intercom_is_enabled(void);
void demo_group_intercom_get_snapshot(demo_group_snapshot_t *out);

/* One reservation lasts through ringing, call and its resource cleanup.
 * 1=reserved; 0=ROOM has no intent/resources, use original call path;
 * -1=another reservation owns the handoff. Same caller must retain its ticket. */
int demo_group_intercom_reserve_call(demo_group_call_e caller,
                                    uint32_t *ticket);
bool demo_group_intercom_call_ready(demo_group_call_e caller,
                                   uint32_t ticket);
void demo_group_intercom_release_call(demo_group_call_e caller,
                                      uint32_t ticket);
bool demo_group_intercom_has_call(void);

#include "../ui/tirtc_ui.h"
int tirtc_group_start_service(void);
int tirtc_group_action(const tirtc_ui_action_t *action);
void tirtc_group_set_audio_config(uint8_t volume, uint8_t gain,
                                 bool speaker_enabled, bool mic_enabled);
/* RAM-only preemption; room page intent resumes after higher priority media drains. */
void tirtc_group_suspend(void);
bool tirtc_group_is_idle(void);
bool tirtc_group_is_active(void);
#endif
