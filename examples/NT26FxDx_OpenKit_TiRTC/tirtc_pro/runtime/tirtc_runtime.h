/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT
 *
 * Small TiRTC runtime adapter for the NT26F6D0 demo.
 *
 * The adapter follows the official minimal-system-examples lifecycle:
 * one device-authenticated TiRTC runtime is started after provisioning and
 * remains alive; feature modules only create and close sessions on it.
 */

#ifndef DEMO_TIRTC_H
#define DEMO_TIRTC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tirtc/tiRTC.h"
#include "../platform/device_binding.h"

/* Start the single managed SDK owner at priority 10. No modem/network work
 * runs in the caller. Repeated calls retain its task and synchronization
 * objects; a task allocation failure can be retried. */
int tirtc_runtime_start_service(void);

typedef struct
{
    /* Optional claim hook for feature-specific inbound P2P flows.  Device
     * calling uses it while the caller waits for the callee to connect back;
     * return 0 to claim, non-zero to let the next router inspect it. */
    int (*on_conn_accepted)(tirtc_conn_t hconn);
    void (*on_conn_error)(tirtc_conn_t hconn, int error);
    void (*on_disconnected)(tirtc_conn_t hconn);
    void (*on_audio)(tirtc_conn_t hconn,
                     const TIRTCFRAMEINFO *frame,
                     void *data);
    void (*on_command)(tirtc_conn_t hconn, uint32_t cmdw,
                       const void *data, uint32_t length);
    int (*on_subscribe_audio)(tirtc_conn_t hconn, uint8_t stream_id);
    void (*on_unsubscribe_audio)(tirtc_conn_t hconn, uint8_t stream_id);
    /* Borrowed SDK data is valid only until this callback returns. Copy into
     * a bounded worker mailbox; do not decode or draw in the SDK callback. */
    void (*on_video)(tirtc_conn_t hconn,
                     const TIRTCFRAMEINFO *frame, void *data);
    int (*on_subscribe_video)(tirtc_conn_t hconn, uint8_t stream_id);
    void (*on_unsubscribe_video)(tirtc_conn_t hconn, uint8_t stream_id);
} demo_tirtc_listener_t;

/* One process-wide TiRTC runtime is shared by independent feature state
 * machines.  Fixed slots avoid dynamic allocation and prevent WX/DEV from
 * overwriting AI's callback router. */
typedef enum
{
    DEMO_TIRTC_FEATURE_AI = 0,
    DEMO_TIRTC_FEATURE_WECHAT,
    DEMO_TIRTC_FEATURE_DEV_CHAT,
#ifdef HWDEMO_GROUP_ROOM_EN
    DEMO_TIRTC_FEATURE_GROUP_ROOM,
#endif
    DEMO_TIRTC_FEATURE_COUNT,
} demo_tirtc_feature_e;

/*
 * Process-wide media-session owner.
 *
 * TiRTC is configured with MAX_CONNECTIONS=1.  A feature must therefore own
 * one generation before it starts an inbound or outbound connection.  The
 * generation makes a late callback from an old session harmless even when
 * the SDK later reuses the same opaque connection address.
 */
typedef enum
{
    DEMO_TIRTC_OWNER_NONE = 0,
    DEMO_TIRTC_OWNER_AI,
    DEMO_TIRTC_OWNER_WECHAT,
    DEMO_TIRTC_OWNER_DEV_CHAT,
    DEMO_TIRTC_OWNER_LIVE,
#ifdef HWDEMO_GROUP_ROOM_EN
    DEMO_TIRTC_OWNER_GROUP_ROOM,
#endif
    DEMO_TIRTC_OWNER_COUNT,
} demo_tirtc_owner_e;

/* Read-only diagnostics for the fixed-memory connection adapter.  Handles
 * deliberately remain private so feature code cannot bypass the reference
 * count or use a handle after disconnect has started. */
typedef struct
{
    demo_tirtc_owner_e owner;
    uint32_t session_generation;
    uint32_t connection_generation;
    uint32_t connection_users;
    bool expected_incoming;
    bool connect_pending;
    bool connect_callback_pending;
    bool connected;
    bool disconnect_pending;
} demo_tirtc_connection_snapshot_t;

/*
 * Router for passive device-mode connections, such as the platform LIVE
 * service.  Returning 0 from on_conn_accepted claims the connection; any
 * other value asks the runtime owner to reject it outside the SDK callback.
 * Like the feature listener, this structure must remain valid while
 * registered and every callback must return quickly.
 */
typedef struct
{
    int (*on_conn_accepted)(tirtc_conn_t hconn);
    void (*on_conn_error)(tirtc_conn_t hconn, int error);
    void (*on_disconnected)(tirtc_conn_t hconn);
    void (*on_audio)(tirtc_conn_t hconn,
                     const TIRTCFRAMEINFO *frame,
                     void *data);
    void (*on_command)(tirtc_conn_t hconn, uint32_t cmdw,
                       const void *data, uint32_t length);
    int (*on_subscribe_video)(tirtc_conn_t hconn, uint8_t stream_id);
    void (*on_unsubscribe_video)(tirtc_conn_t hconn, uint8_t stream_id);
    int (*on_subscribe_audio)(tirtc_conn_t hconn, uint8_t stream_id);
    void (*on_unsubscribe_audio)(tirtc_conn_t hconn, uint8_t stream_id);
    void (*on_video)(tirtc_conn_t hconn,
                     const TIRTCFRAMEINFO *frame, void *data);
} demo_tirtc_incoming_listener_t;

/* Compatibility wrapper: register the AI feature router. */
void demo_tirtc_set_listener(const demo_tirtc_listener_t *listener);

/* Register one long-lived feature router in its fixed slot. */
void demo_tirtc_set_feature_listener(
    demo_tirtc_feature_e feature,
    const demo_tirtc_listener_t *listener);

/* Register the long-lived passive-connection router used by platform LIVE. */
void demo_tirtc_set_incoming_listener(
    const demo_tirtc_incoming_listener_t *listener);

/*
 * Managed single-connection API.
 *
 * These calls mirror the ownership rules used by the official ESP32-P4
 * minimal-system adapter.  Every feature uses this API for connection setup,
 * media access and teardown.  Session-scoped TiRtc* calls must not bypass the
 * adapter: it owns the opaque handles and rejects stale generations.
 */

/* Atomically acquire the sole media-session owner and a non-zero generation.
 * Returns TIRTC_E_BUSY while another session, connect callback, handle user or
 * SDK-side disconnect is still live. */
int demo_tirtc_session_claim(demo_tirtc_owner_e owner,
                             uint32_t *session_generation);

/* Optional AI-only background connection. All four calls are bounded RAM
 * operations in task context. allowed() is a hint; claim() atomically repeats
 * the ready/route/idle/listener checks and acquires the normal AI generation.
 * Background connections are disabled whenever a non-AI feature router or
 * passive incoming router is registered. A foreground feature claim/admission
 * revokes standby and returns BUSY/false; that feature must retry after AI's
 * normal disconnect/release completes. The runtime never steals a live lease.
 * valid() becomes false on revocation or route loss. The AI worker must then
 * stop new operations and use normal safe cleanup. promote() succeeds only
 * for a still-valid standby generation, preserving its generation and handle;
 * after promotion valid() is false because the session is now foreground.
 * Inbound audio/commands/subscriptions observed while standby are discarded;
 * promotion cannot reclassify an already-borrowed callback as foreground.
 * Connection results, errors and disconnect notifications remain enabled. */
bool demo_tirtc_ai_standby_allowed(void);
int demo_tirtc_ai_standby_claim(uint32_t *session_generation);
bool demo_tirtc_ai_standby_valid(uint32_t session_generation);
int demo_tirtc_ai_standby_promote(uint32_t session_generation);

/* Release a claimed owner only after all expected/pending/active/closing
 * connection state has drained. */
int demo_tirtc_session_release(demo_tirtc_owner_e owner,
                               uint32_t session_generation);

/* Arm/cancel the one inbound connection expected by the current owner. */
int demo_tirtc_expect_incoming(demo_tirtc_owner_e owner,
                               uint32_t session_generation);
int demo_tirtc_cancel_expected_incoming(demo_tirtc_owner_e owner,
                                        uint32_t session_generation);

/* Adoption path for a passive incoming listener (currently LIVE).  Call it
 * from on_conn_accepted before publishing or signaling the handle, then use
 * only managed wrappers for that connection.  It atomically claims a
 * generation and adopts the SDK-supplied handle. */
int demo_tirtc_adopt_incoming(demo_tirtc_owner_e owner,
                              tirtc_conn_t connection,
                              uint32_t *session_generation);

/* Asynchronous outbound connections.  String inputs are copied into a fixed
 * adapter context and may be released when this function returns. */
int demo_tirtc_connect(demo_tirtc_owner_e owner,
                       uint32_t session_generation,
                       const char *remote_id,
                       const char *token,
                       TIRTCCONNECTCALLBACK callback,
                       void *user_data);
int demo_tirtc_whip_connect(demo_tirtc_owner_e owner,
                            uint32_t session_generation,
                            const char *service_description,
                            const char *token,
                            TIRTCCONNECTCALLBACK callback,
                            void *user_data);

/* Operations below temporarily acquire a reference to the active handle.
 * Disconnect first detaches that handle, then the runtime task calls the SDK;
 * its reservation remains asserted until on_disconnected. */
int demo_tirtc_send_command(demo_tirtc_owner_e owner,
                            uint32_t session_generation,
                            uint32_t command,
                            const void *data,
                            uint32_t length);
int demo_tirtc_send_audio(demo_tirtc_owner_e owner,
                          uint32_t session_generation,
                          const TIRTCFRAMEINFO *frame,
                          const void *data);
int demo_tirtc_subscribe_audio(demo_tirtc_owner_e owner,
                               uint32_t session_generation,
                               uint8_t stream_id);
/* Video uses the same active-handle lease as audio. No raw SDK handle is
 * exposed. The video receiver must bound/copy the payload before returning.
 * Unknown, retiring and background AI handles never dispatch video. */
int demo_tirtc_send_video(demo_tirtc_owner_e owner,
                         uint32_t session_generation,
                         const TIRTCFRAMEINFO *frame, const void *data);
int demo_tirtc_subscribe_video(demo_tirtc_owner_e owner,
                              uint32_t session_generation, uint8_t stream_id);
int demo_tirtc_unsubscribe_video(demo_tirtc_owner_e owner,
                                uint32_t session_generation, uint8_t stream_id);
int demo_tirtc_get_send_buffer_used(demo_tirtc_owner_e owner,
                                    uint32_t session_generation,
                                    size_t *used_bytes);

/* Snapshot of the last SDK video-send control event, not a periodic network
 * sample and not cumulative packet-loss counters. An absent/stale event must
 * never be reported as zero packet loss. Values describe the TiRTC peer link,
 * which may end at the WeChat gateway rather than the phone itself. */
typedef struct
{
    bool attempted;
    bool registered; /* asynchronous registration was submitted successfully */
    bool available;  /* at least one complete base event has arrived */
    bool stale;      /* last event is more than 10 seconds old */
    bool loss_available; /* receiver BWE event with valid optional loss fields */
    int registration_result;
    uint32_t generation;
    uint32_t event_at_ms;
    uint32_t age_ms;
    uint32_t event_seq;
    uint32_t rtt_ms;
    uint32_t receive_bitrate_bps;
    uint32_t loss_rate_ppm; /* 10,000 = 1%; check loss_available and !stale */
    uint32_t unacked_segments; /* outstanding segments, not a lost-packet count */
    uint32_t pending_frames;
    uint32_t pending_wait_ms;
    uint32_t dropped_pending_frames; /* event's local queue discard count */
    uint8_t stream_id;
    uint8_t event_type;
    uint8_t reason;
} demo_tirtc_video_send_stats_t;

/* Optional diagnostics: failure must not stop the media session. Call from a
 * worker once for its active WeChat/device stream (SDK permits one callback).
 * Repeated calls for the same connection return its original result. Do not
 * also install TiRtcConnSetVideoBitrateParams on that stream: it owns the same
 * SDK callback. No blocking I/O or logging runs in the event callback. */
int demo_tirtc_video_send_monitor_start(demo_tirtc_owner_e owner,
                                        uint32_t session_generation,
                                        uint8_t stream_id);
/* Bounded RAM copy, suitable for a five-second worker diagnostic. Returns an
 * empty snapshot for an active connection without registration; invalid or
 * retired connections return INVALID_HANDLE and a zeroed snapshot. */
int demo_tirtc_get_video_send_stats(demo_tirtc_owner_e owner,
                                   uint32_t session_generation,
                                   demo_tirtc_video_send_stats_t *stats);
int demo_tirtc_disconnect(demo_tirtc_owner_e owner,
                          uint32_t session_generation);
void demo_tirtc_get_connection_snapshot(
    demo_tirtc_connection_snapshot_t *snapshot);

/* Runtime owner task.  It waits for binding credentials, starts the shared
 * SDK, and owns any graceful Stop/Uninit/Start recovery. */
void demo_tirtc_task(void *argv);

bool demo_tirtc_is_ready(void);
/* Worker-only, synchronous in the pinned SDK. Holds a global SDK lease until
 * the request and its callback return, so Stop/Uninit cannot race network I/O.
 * No connection owner is claimed (busy-call rejection may coexist with AI).
 * Caller keeps body/user alive through return; callback must not stop runtime.
 * A binding/route change before or during I/O returns a stale-route error. */
int demo_tirtc_service_request(const demo_binding_service_context_t *context,
    const char *path, const char *body, TIRTCSERVICEREQUESTCALLBACK callback,
    void *user);
int demo_tirtc_wait_ready(uint32_t timeout_ms);
int demo_tirtc_get_error(void);

/* Ask the runtime owner to recycle an SDK lifetime whose public connections
 * can still be drained.  Callers must never stop or reset directly from an SDK
 * callback. */
void demo_tirtc_require_restart(int error);

/* Non-blocking transaction gate for asynchronous HOME-page admissions.  It
 * protects only the final idle recheck + local state commit; media lifetime
 * remains owned by each existing feature state machine. */
bool demo_tirtc_admission_try_enter(void);
void demo_tirtc_admission_leave(void);

/* Wait for deferred rejection disconnects to finish releasing SDK handles. */
bool demo_tirtc_wait_connections_idle(uint32_t timeout_ms);

#endif /* DEMO_TIRTC_H */
