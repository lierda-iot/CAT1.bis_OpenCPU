/* Device/WeChat call facade. UI actions only copy intent; workers own I/O. */
#ifndef TIRTC_CALLS_H
#define TIRTC_CALLS_H
#include <stdbool.h>
#include <stdint.h>
#include "tirtc_ui.h"
#include "../contacts/tirtc_contacts.h"
typedef struct {
    tirtc_ui_call_state_t state;
    tirtc_ui_call_type_t type;
    uint32_t session_id, seconds, revision;
    bool incoming, wechat, video_enabled;
    int error;
    char peer_id[96], peer_name[96], message[128];
} tirtc_calls_snapshot_t;
int tirtc_calls_start_service(void);
/* CALL_AUDIO/VIDEO: text=stable contact id, value=0 device or 1 WeChat.
 * ACCEPT/REJECT/HANGUP: value=(int32_t) displayed nonzero session_id.
 * Stale session actions fail instead of affecting a subsequent invitation.
 * Page navigation does not implicitly accept or terminate a call. */
int tirtc_calls_action(const tirtc_ui_action_t *action);
/* AI supplies a freshly resolved contact and its captured identity/route.
 * RAM-only enqueue; existing call worker stops AI and waits for media release. */
int tirtc_calls_dial_contact(const tirtc_contact_t *contact, bool video);
void tirtc_calls_get_snapshot(tirtc_calls_snapshot_t *out);
void tirtc_calls_set_audio_config(uint8_t volume, uint8_t mic_gain,
                                 bool speaker_enabled, bool mic_enabled);
bool tirtc_calls_has_session(void);
#endif
