/* Private F6D_D backend, included once by audio_device.c. Keep the existing
 * codec/lease owner and public interface. SDK contracts are documented in
 * the Lite backend audit. Pro retains its own levels, lease and warm API contracts.
 */
#define AEC_CAPTURE_TIMEOUT_MS 120U
#define AEC_START_TIMEOUT_MS 1000U
/* Record-stop precedes playback-stop (the vendor's own deinit order).
 * Once RX/I2S stops, queued TX may need the SDK's ~5 s discard timeout.
 * Give normal cleanup/restart room beyond that. This is an application
 * budget, not an upper bound for vendor waits, and cannot catch asserts. */
#define AEC_STOP_TIMEOUT_MS 10000U
#define AEC_DRAIN_GRACE_MS 200U
#define AEC_PLAY_SAMPLES 4096U
#define AEC_PARTIAL_WAIT_MS 40U
#define AEC_CHUNK_SAMPLES 640U /* SDK DMA slots: 40 ms at 16 kHz. */
#define AEC_ERR_TIMEOUT DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT
#define AEC_ERR_FAULT (-41)
#define AEC_ERR_BUSY DEMO_AI_AUDIO_ERR_PLAY_BUSY

#if defined(TIRTC_AUDIO_FREE_GUARD_ENABLE) && TIRTC_AUDIO_FREE_GUARD_ENABLE
#include "audio_free_guard.h"
#endif

static liot_task_t s_aec_task;
static liot_mutex_t s_aec_levels_mutex;
static bool s_aec_hw_ready;
static volatile bool s_aec_running, s_aec_seen_capture;
static volatile bool s_aec_capture_waiting, s_aec_capture_ready;
/* ISR completion does not release a task's ownership of the mailbox. */
static bool s_aec_capture_call_busy;
static volatile uint32_t s_aec_capture_bytes, s_aec_capture_epoch;
static volatile uint32_t s_aec_epoch;
static volatile bool s_aec_stop_requested, s_aec_fault, s_aec_worker_busy;
static volatile bool s_aec_stopping;
static bool s_aec_retiring; /* held until release commits the owner change */
static uint32_t s_aec_read, s_aec_count, s_aec_primed;
static uint32_t s_aec_next_feed, s_aec_play_until;
static bool s_aec_play_active;
static bool s_aec_partial_waiting;
static uint32_t s_aec_partial_since;
static __attribute__((aligned(16))) int16_t s_aec_play_ring[AEC_PLAY_SAMPLES];
static __attribute__((aligned(16))) int16_t s_aec_play_chunk[AEC_CHUNK_SAMPLES];

static bool aec_levels_lock(uint32_t timeout_ms);
static int aec_start(void);

/* These deadlines are at most one slot/tail into the future. A stale
 * deadline after weeks of idle must not become a future signed timestamp. */
static bool aec_deadline_pending(uint32_t now, uint32_t deadline, uint32_t window)
{
    uint32_t remaining = deadline - now;
    return remaining != 0U && remaining <= window;
}

static int aec_mute(void)
{
    int ret;
    if (!aec_levels_lock(1000U)) return AEC_ERR_TIMEOUT;
    ret = Liot_AudioSetCodecVolume(0);
    (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
    return ret == L_AUD_ERR_SUCCESS ? 0 : AEC_ERR_FAULT;
}

/* Run from the task even when callers are only waiting for a tail. The
 * public adapter checks at most every 100 ms and holds the same mutex as
 * Stop. Do not depend on capture PCM as proof that later TX init is done. */
static void aec_service_levels(void)
{
    demo_ai_audio_lease_t lease;
    bool eligible;
    if (!s_audio_ready || s_stopped || !s_aec_play_active || s_aec_primed < 3U) return;
    liot_rtos_enter_critical();
    eligible = s_aec_running && !s_aec_stopping && !s_aec_retiring &&
        !s_aec_stop_requested && !s_aec_fault &&
        s_audio_owner != DEMO_AI_AUDIO_OWNER_NONE && !(s_active & OP_CONTROL);
    lease.owner = s_audio_owner;
    lease.generation = s_audio_generation;
    liot_rtos_exit_critical();
    if (eligible) (void)aec_apply_levels(&lease, true);
}

/* Called from an IPC ISR. No task critical sections, logs, SDK calls,
 * allocation, or waits here. Tasks mask interrupts while changing the
 * mailbox. Publish ready only after copying the vendor-owned PCM. */
static void aec_record_callback(uint8_t *data, uint16_t length)
{
    uint32_t room;
    if (!s_aec_running || data == NULL || length == 0U || (length & 1U)) return;
    s_aec_seen_capture = true;
    if (!s_aec_capture_waiting || s_aec_capture_epoch != s_aec_epoch) return;
    room = DEMO_AI_AUDIO_FRAME_BYTES - s_aec_capture_bytes;
    if (length > room) length = (uint16_t)room;
    memcpy((uint8_t *)s_record_frame + s_aec_capture_bytes, data, length);
    s_aec_capture_bytes += length;
    if (s_aec_capture_bytes == DEMO_AI_AUDIO_FRAME_BYTES) {
        s_aec_capture_waiting = false;
        s_aec_capture_ready = true;
    }
}

/* Only this worker calls the potentially blocking vendor playback APIs.
 * Pacing bounds normal SDK backlog; a stalled SDK cannot block the network
 * caller. Cancellation mutes first and never kills a thread in vendor code. */
static void aec_worker_step(void)
{
    uint32_t count, epoch, deadline, now;
    bool stop, failed = false;
    Liot_AudErr_e ret;
    aec_service_levels();
    now = liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    stop = s_aec_stop_requested;
    if (stop) {
        bool rebuild = s_aec_play_active;
        int restart_ret = 0;
        s_aec_worker_busy = true;
        if (rebuild) s_aec_running = false;
        liot_rtos_exit_critical();
        ret = L_AUD_ERR_SUCCESS;
        if (rebuild) {
            /* The codec is muted and no more application PCM can enter.
             * Keep DMA alive briefly so the SDK timer can consume common
             * TX leftovers before RecordStop removes I2S. This is best
             * effort: retain the SDK cleanup/fault path if it cannot drain. */
            liot_rtos_task_sleep_ms(AEC_DRAIN_GRACE_MS);
            /* Match _Liot_Aud3aDeInit: stop recording/VEM before playback.
             * PlayStreamStop with live RX directly deinitializes shared
             * I2S without the record-stop handshake; board logs show panic
             * before that call returns. RecordStreamStop quiesces RX first.
             * Keep use3A/config alive so the existing engine can restart. */
#if defined(TIRTC_AUDIO_FREE_GUARD_ENABLE) && TIRTC_AUDIO_FREE_GUARD_ENABLE
            demo_audio_free_guard_begin_stop();
#endif
            ret = Liot_AudioRecordStreamStop();
            if (ret != L_AUD_ERR_SUCCESS) liot_trace("[AI-AEC] stop record done ret=%d t=%lu\r\n",
                       (int)ret, (unsigned long)liot_rtos_get_running_time());
            if (ret == L_AUD_ERR_SUCCESS) {
                Liot_AudErr_e play_ret = Liot_AudioPlayStreamStop();
                if (play_ret != L_AUD_ERR_SUCCESS) liot_trace("[AI-AEC] stop play done ret=%d t=%lu\r\n",
                           (int)play_ret, (unsigned long)liot_rtos_get_running_time());
                if (play_ret != L_AUD_ERR_SUCCESS) ret = play_ret;
            }
#if defined(TIRTC_AUDIO_FREE_GUARD_ENABLE) && TIRTC_AUDIO_FREE_GUARD_ENABLE
            if (ret == L_AUD_ERR_SUCCESS) {
                if (demo_audio_free_guard_reclaim() != 0) {
                    ret = L_AUD_ERR_EXECUTE;
                    liot_trace("[AI-AEC] stop ownership check failed; capture restart blocked\r\n");
                }
            }
#endif
            if (ret != L_AUD_ERR_SUCCESS) liot_trace("[AI-AEC] streams stopped combined_ret=%d t=%lu\r\n",
                       (int)ret, (unsigned long)liot_rtos_get_running_time());
            if (ret == L_AUD_ERR_SUCCESS && !s_aec_fault) {
                restart_ret = aec_start();
                if (restart_ret != 0) liot_trace("[AI-AEC] capture rebuild done ret=%d t=%lu\r\n",
                           restart_ret, (unsigned long)liot_rtos_get_running_time());
                if (restart_ret != 0) {
                    /* The SDK may have partially started before failing. */
                    s_aec_running = false;
                    (void)Liot_AudioRecordStreamStop();
                } else {
                    /* Reinitialization resets codec registers. Keep Stop
                     * silent until a later record/play restores the levels. */
                    restart_ret = aec_mute();
                    if (restart_ret != 0) liot_trace("[AI-AEC] rebuild mute done ret=%d t=%lu\r\n",
                               restart_ret, (unsigned long)liot_rtos_get_running_time());
                }
            }
            if (restart_ret != 0)
                liot_trace("[AI-AEC] capture rebuild failed ret=%d; reboot required\r\n",
                           restart_ret);
        }
        /* No TX submitted means there is no vendor playback to stop. Leave
         * the capture-only engine intact; release must not dismantle it. */
        liot_rtos_enter_critical();
        s_aec_read = s_aec_count = s_aec_primed = 0U;
        s_aec_play_active = false;
        s_aec_partial_waiting = false;
        s_aec_worker_busy = false;
        if (ret != L_AUD_ERR_SUCCESS || restart_ret != 0) s_aec_fault = true;
        s_aec_stop_requested = false;
        liot_rtos_exit_critical();
        return;
    }
    if (!s_aec_running || s_aec_fault || s_aec_stopping || s_aec_retiring ||
        (!s_aec_count && (!s_aec_play_active || s_aec_primed >= 3U)) ||
        (s_aec_play_active && aec_deadline_pending(now, s_aec_next_feed, 40U))) {
        liot_rtos_exit_critical();
        return;
    }
    /* A 20 ms network packet must not become 20 ms of speech plus 20 ms of
     * inserted silence. Coalesce a partial DMA slot before committing it;
     * bound the wait so the final short packet and priming still complete. */
    if (s_aec_count > 0U && s_aec_count < AEC_CHUNK_SAMPLES) {
        if (!s_aec_partial_waiting) {
            s_aec_partial_waiting = true;
            s_aec_partial_since = now;
        }
        if ((uint32_t)(now - s_aec_partial_since) < AEC_PARTIAL_WAIT_MS) {
            liot_rtos_exit_critical();
            return;
        }
    }
    s_aec_partial_waiting = false;
    epoch = s_aec_epoch;
    deadline = s_aec_play_active ? s_aec_next_feed : now;
    /* Preserve the 16 kHz clock across API/scheduler latency. After a long
     * stall, restart pacing instead of bursting a large vendor backlog. */
    if ((uint32_t)(now - deadline) >= 40U) deadline = now;
    count = s_aec_count < AEC_CHUNK_SAMPLES ? s_aec_count : AEC_CHUNK_SAMPLES;
    for (uint32_t i = 0; i < count; ++i) {
        s_aec_play_chunk[i] = s_aec_play_ring[s_aec_read];
        s_aec_read = (s_aec_read + 1U) % AEC_PLAY_SAMPLES;
    }
    s_aec_count -= count;
    memset(s_aec_play_chunk + count, 0, (AEC_CHUNK_SAMPLES - count) * sizeof(int16_t));
    s_aec_worker_busy = true;
    s_aec_play_active = true;
    liot_rtos_exit_critical();

    /* A stop that preempted this copy must not submit another old frame. If
     * it preempts the vendor call, the codec is muted until worker Stop. */
    liot_rtos_enter_critical();
    stop = s_aec_stopping || s_aec_stop_requested || epoch != s_aec_epoch;
    liot_rtos_exit_critical();
    ret = stop ? L_AUD_ERR_SUCCESS :
        Liot_AudioPlayStream((uint8_t *)s_aec_play_chunk, sizeof(s_aec_play_chunk));
    liot_rtos_enter_critical();
    s_aec_worker_busy = false;
    if (!stop && ret != L_AUD_ERR_SUCCESS) {
        s_aec_fault = true; /* An async failure must not replay a partial conversation. */
        s_aec_count = 0U;
        s_aec_stop_requested = true; /* This worker performs cleanup next step. */
        s_levels_dirty = true;
        failed = true;
    }
    if (!stop && epoch == s_aec_epoch && ret == L_AUD_ERR_SUCCESS) {
        now = liot_rtos_get_running_time();
        s_aec_next_feed = deadline + 40U;
        if (s_aec_primed < 3U) ++s_aec_primed;
        /* No stream FINISH callback exists. Cover the three DMA slots and
         * one scheduling slot conservatively; silence padding has no tail. */
        if (count) s_aec_play_until = now + 160U;
    }
    liot_rtos_exit_critical();
    if (failed) {
        /* Rejecting new PCM does not silence an already running DMA. Never
         * call aec_stop here: that would wait for this worker itself. */
        int mute_ret = aec_mute();
        liot_trace("[AI-AEC] playback stream failed; mute_ret=%d; reboot required\r\n",
                   mute_ret);
    }
}

static void aec_worker(void *argument)
{
    (void)argument;
    for (;;) {
        aec_worker_step();
        liot_rtos_task_sleep_ms(5U);
    }
}

#ifdef DEMO_AI_AUDIO_AEC_TEST
/* Deterministic host scheduler; not linked into device firmware. */
void demo_ai_audio_aec_service_for_test(void) { aec_worker_step(); }
#endif

static int aec_start(void)
{
    uint32_t started;
    int ret;
    if (s_aec_fault) return AEC_ERR_FAULT;
    if (s_aec_levels_mutex == NULL) {
        ret = liot_rtos_mutex_create(&s_aec_levels_mutex);
        if (ret != 0 || s_aec_levels_mutex == NULL) {
            s_aec_fault = true;
            return AEC_ERR_FAULT;
        }
    }
    if (s_aec_task == NULL) {
        ret = liot_rtos_task_create(&s_aec_task, 4096U, 12U, "pro_aec",
                                   aec_worker, NULL);
        if (ret != 0 || s_aec_task == NULL) {
            s_aec_fault = true;
            return AEC_ERR_FAULT;
        }
    }
    if (!s_aec_running) {
        s_aec_seen_capture = false;
        s_aec_running = true; /* Permit an immediate ISR during Start. */
        ret = Liot_AudioRecordStream(aec_record_callback);
        if (ret != L_AUD_ERR_SUCCESS) liot_trace("[AI-AEC] RecordStream done ret=%d t=%lu\r\n",
                   ret, (unsigned long)liot_rtos_get_running_time());
        if (ret != L_AUD_ERR_SUCCESS) {
            s_aec_running = false;
            s_aec_fault = true;
            return AEC_ERR_FAULT;
        }
    }
    started = liot_rtos_get_running_time();
    while (!s_aec_seen_capture && !s_aec_fault) {
        if ((uint32_t)(liot_rtos_get_running_time() - started) >= AEC_START_TIMEOUT_MS) {
            s_aec_running = false;
            s_aec_fault = true;
            return AEC_ERR_TIMEOUT;
        }
        liot_rtos_task_sleep_ms(1U);
    }
    if (s_aec_fault) {
        s_aec_running = false;
        return AEC_ERR_FAULT;
    }
    /* StartRecord is asynchronous. The first PCM proves codec init finished;
     * the caller now restores the requested microphone/speaker registers. */
    return 0;
}

/* SDK mutex timeouts are ticks. Use one-tick waits with an elapsed-ms limit
 * so the control bound does not depend on the configured tick frequency. */
static bool aec_levels_lock(uint32_t timeout_ms)
{
    uint32_t started = liot_rtos_get_running_time();
    if (s_aec_levels_mutex == NULL) return false;
    do {
        if (liot_rtos_mutex_lock(s_aec_levels_mutex, 1U) == 0) return true;
        liot_rtos_task_sleep_ms(1U);
    } while ((uint32_t)(liot_rtos_get_running_time() - started) < timeout_ms);
    return false;
}

static int aec_stop(const demo_ai_audio_lease_t *lease)
{
    uint32_t started = liot_rtos_get_running_time();
    int mute_ret;
    liot_rtos_enter_critical();
    if (!lease_valid(lease)) {
        liot_rtos_exit_critical();
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    /* A prior bounded stop already failed. Release must not spend another
     * full timeout waiting for the same stuck worker. Keep its cleanup
     * request and the latched fault; subsequent owners cannot submit audio. */
    if (s_aec_fault && (s_aec_stop_requested || s_aec_worker_busy)) {
        liot_rtos_exit_critical();
        return AEC_ERR_FAULT;
    }
    if (s_aec_stopping) {
        liot_rtos_exit_critical();
        /* Join an existing stop; never clear another caller's control gate. */
        while (s_aec_stopping) {
            if ((uint32_t)(liot_rtos_get_running_time() - started) >= AEC_STOP_TIMEOUT_MS)
                return AEC_ERR_TIMEOUT;
            liot_rtos_task_sleep_ms(1U);
        }
        return s_aec_fault ? AEC_ERR_FAULT : 0;
    }
    ++s_aec_epoch;
    s_aec_capture_waiting = s_aec_capture_ready = false;
    s_aec_capture_bytes = 0U;
    s_aec_read = s_aec_count = 0U;
    if (!s_aec_running) {
        liot_rtos_exit_critical();
        return 0;
    }
    s_aec_stopping = true;
    s_levels_dirty = true;
    liot_rtos_exit_critical();
    /* StreamStop drains instead of cancelling. The gate plus this mutex
     * ensures a level write already in progress cannot undo our mute.
     * Publish worker teardown only after the codec write returns: stopping
     * already blocks new feeds, but teardown can deinitialize that codec. */
    mute_ret = aec_mute();
    if (mute_ret != 0) liot_trace("[AI-AEC] stop mute done ret=%d t=%lu\r\n",
               mute_ret, (unsigned long)liot_rtos_get_running_time());
    liot_rtos_enter_critical();
    if (mute_ret != 0) s_aec_fault = true;
    s_aec_stop_requested = true;
    liot_rtos_exit_critical();
    while (s_aec_stop_requested || s_aec_worker_busy) {
        if ((uint32_t)(liot_rtos_get_running_time() - started) >= AEC_STOP_TIMEOUT_MS) {
            s_aec_fault = true;
            s_aec_stopping = false; /* fault remains latched; no new PCM allowed */
            liot_trace("[AI-AEC] playback worker stop timeout; reboot required\r\n");
            return AEC_ERR_TIMEOUT;
        }
        liot_rtos_task_sleep_ms(1U);
    }
    s_aec_stopping = false;
    return !s_aec_fault ? 0 : AEC_ERR_FAULT;
}

static int aec_record(const demo_ai_audio_lease_t *lease, int16_t *pcm)
{
    uint32_t started, epoch;
    int ret = AEC_ERR_TIMEOUT;
    liot_rtos_enter_critical();
    if (!lease_valid(lease) || s_aec_fault ||
        s_aec_stopping || s_aec_retiring || s_aec_stop_requested || s_aec_capture_call_busy) {
        liot_rtos_exit_critical();
        return s_aec_fault ? AEC_ERR_FAULT : DEMO_AI_AUDIO_ERR_BUSY;
    }
    epoch = s_aec_epoch;
    s_aec_capture_call_busy = true;
    s_aec_capture_epoch = epoch;
    s_aec_capture_bytes = 0U;
    s_aec_capture_ready = false;
    s_aec_capture_waiting = true;
    liot_rtos_exit_critical();
    started = liot_rtos_get_running_time();
    for (;;) {
        liot_rtos_enter_critical();
        if (epoch != s_aec_epoch || !lease_valid(lease) || s_aec_fault) {
            ret = DEMO_AI_AUDIO_ERR_INVALID_LEASE;
        } else if (s_aec_capture_ready) {
            memcpy(pcm, s_record_frame, DEMO_AI_AUDIO_FRAME_BYTES);
            (void)audio_guard_impulses(lease->owner, s_record_frame, pcm);
            s_aec_capture_ready = false;
            s_aec_capture_call_busy = false;
            liot_rtos_exit_critical();
            return 0;
        } else if ((uint32_t)(liot_rtos_get_running_time() - started) < AEC_CAPTURE_TIMEOUT_MS) {
            liot_rtos_exit_critical();
            liot_rtos_task_sleep_ms(1U);
            continue;
        }
        if (s_aec_capture_epoch == epoch) {
            s_aec_capture_waiting = s_aec_capture_ready = false;
        }
        s_aec_capture_call_busy = false;
        liot_rtos_exit_critical();
        return ret;
    }
}

static int aec_play(const demo_ai_audio_lease_t *lease, const int16_t *pcm, uint32_t samples)
{
    uint32_t write;
    int volume, gain;
    if (samples > AEC_PLAY_SAMPLES) return AEC_ERR_BUSY;
    volume = lease->owner == DEMO_AI_AUDIO_OWNER_DEVICE || group_owner() ? 10 :
        s_speaker_volume[s_speaker_level];
    gain = 256 * (volume / 10) + 26 * (volume % 10);
    liot_rtos_enter_critical();
    if (!lease_valid(lease) || s_aec_fault) {
        int ret = s_aec_fault ? AEC_ERR_FAULT : DEMO_AI_AUDIO_ERR_INVALID_LEASE;
        liot_rtos_exit_critical();
        return ret;
    }
    if (s_aec_stopping || s_aec_retiring || s_aec_stop_requested ||
        samples > AEC_PLAY_SAMPLES - s_aec_count) {
        liot_rtos_exit_critical();
        return AEC_ERR_BUSY;
    }
    write = (s_aec_read + s_aec_count) % AEC_PLAY_SAMPLES;
    for (uint32_t i = 0; i < samples; ++i) {
        int32_t scaled = (int32_t)pcm[i] * gain;
        /* Match the ordinary SDK's Q8 ASR and saturation, once only. The
         * 3A stream does not apply SetVolume. Never mutate caller PCM. */
        scaled = scaled < 0 ? -((-scaled + 255) / 256) : scaled / 256;
        if (scaled > INT16_MAX) scaled = INT16_MAX;
        if (scaled < INT16_MIN) scaled = INT16_MIN;
        s_aec_play_ring[write] = (int16_t)scaled;
        write = (write + 1U) % AEC_PLAY_SAMPLES;
    }
    s_aec_count += samples;
    liot_rtos_exit_critical();
    return 0;
}

static bool aec_play_done(void)
{
    bool done;
    liot_rtos_enter_critical();
    done = !s_aec_count && !s_aec_worker_busy &&
        (!s_aec_play_active || (s_aec_primed >= 3U &&
         !aec_deadline_pending(liot_rtos_get_running_time(), s_aec_play_until, 160U)));
    liot_rtos_exit_critical();
    return done;
}

/* Pro retains serialized public control and level-zero mute. The service
 * worker uses the same mutex and checks OP_CONTROL before each write. */
#define AEC_LEVELS_CHECK_MS 100U
static uint32_t s_aec_levels_checked_at;
static bool aec_snapshot_lease(const demo_ai_audio_lease_t *lease, demo_ai_audio_lease_t *out)
{
    bool valid;
    liot_rtos_enter_critical();
    valid = lease_valid(lease);
    if (valid) *out = *lease;
    liot_rtos_exit_critical();
    return valid;
}
static int aec_apply_levels(const demo_ai_audio_lease_t *lease,
                                          bool playback_service)
{
#if DEMO_AI_AUDIO_AEC_ENABLE
    demo_ai_audio_lease_t snapshot;
    if (!aec_snapshot_lease(lease, &snapshot))
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    lease = &snapshot;
#else
    (void)playback_service;
#endif
    uint8_t speaker_level;
    uint8_t mic_level;
    uint8_t speaker_volume;
    uint8_t codec_volume;
    uint8_t mic_gain;
    uint8_t mic_volume;
    Liot_AudErr_e sw_ret;
    Liot_AudErr_e codec_ret;
    Liot_AudErr_e mic_ret;

    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    if (!s_audio_ready)
    {
        return -1;
    }
#if DEMO_AI_AUDIO_AEC_ENABLE
    if (s_aec_fault) return AEC_ERR_FAULT;
    if (s_aec_stopping || s_aec_retiring || s_aec_stop_requested) return DEMO_AI_AUDIO_ERR_BUSY;
#endif
    if (!s_levels_dirty
#if DEMO_AI_AUDIO_AEC_ENABLE
        && (!s_aec_play_active || s_aec_primed < 3U ||
            (uint32_t)(liot_rtos_get_running_time() - s_aec_levels_checked_at) <
                AEC_LEVELS_CHECK_MS)
#endif
        )
    {
        return 0;
    }

#if DEMO_AI_AUDIO_AEC_ENABLE
    bool levels_locked = playback_service ?
        (s_aec_levels_mutex != NULL &&
         liot_rtos_mutex_lock(s_aec_levels_mutex, 0U) == 0) :
        aec_levels_lock(100U);
    if (!levels_locked) {
        /* Ordinary UI contention must not turn a healthy call into an audio
         * failure. The playback worker never waits here: retry next step so
         * a volume update cannot consume a 40-ms playback deadline. */
        return s_aec_stopping || s_aec_retiring || s_aec_fault ? DEMO_AI_AUDIO_ERR_BUSY : 0;
    }
    if (!demo_ai_audio_lease_is_valid(lease) || s_aec_stopping || s_aec_retiring ||
        s_aec_stop_requested || s_aec_fault) {
        (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
        return DEMO_AI_AUDIO_ERR_BUSY;
    }
    /* Stop can finish after the worker's lease snapshot without releasing
     * the lease. Only an explicit caller may resume that stopped session. */
    if (playback_service &&
        (s_stopped || (s_active & OP_CONTROL) || !s_aec_running || !s_aec_play_active || s_aec_primed < 3U)) {
        (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
        return 0;
    }
#endif
    speaker_level = clamp(s_speaker_level);
    mic_level = clamp(s_mic_level);

    speaker_volume = s_speaker_volume[speaker_level];
    codec_volume = speaker_volume;
    if (lease->owner == DEMO_AI_AUDIO_OWNER_DEVICE || group_owner())
    {
        speaker_volume = 10U;
        codec_volume = s_dev_codec_volume[speaker_level];
    }
    mic_gain = s_mic_gain[mic_level];
    mic_volume = s_mic_volume[mic_level];

#if DEMO_AI_AUDIO_AEC_ENABLE
    if (!s_levels_dirty) {
        int actual = -1;
        /* ES8311 SetVolume writes floor(255*v/100); GetVolume converts
         * that register back with floor(reg*100/255). Comparing directly
         * with the requested level would rewrite most settings forever. */
        int expected = ((255 * (int)codec_volume / 100) * 100) / 255;
        Liot_AudErr_e read_ret;
        if (!s_aec_play_active || s_aec_primed < 3U ||
            (uint32_t)(liot_rtos_get_running_time() - s_aec_levels_checked_at) <
                AEC_LEVELS_CHECK_MS) {
            (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
            return 0;
        }
        read_ret = Liot_AudioGetCodecVolume(&actual);
        s_aec_levels_checked_at = liot_rtos_get_running_time();
        if (read_ret == L_AUD_ERR_SUCCESS && actual == expected) {
            (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
            return 0;
        }
        /* codecInit also overwrites mic gain/volume with 8/191. Restore
         * the complete setting. The SDK getter hides I2C read errors as
         * zero: a successful return is not proof of a successful read or
         * write, so subsequent 100-ms checks remain enabled after retry. */
        s_levels_dirty = true;
        if (read_ret != L_AUD_ERR_SUCCESS) liot_trace("[AI-AUDIO] codec level drift: read=%d expected=%d ret=%d; restoring\r\n",
                   actual, expected, (int)read_ret);
    }
    /* A read may yield to Stop or an owner change. Never restore volume
     * after cancellation has published its gate; Stop mutes after us. */
    if (!demo_ai_audio_lease_is_valid(lease) || s_aec_stopping || s_aec_retiring ||
        s_aec_stop_requested || s_aec_fault) {
        (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
        return DEMO_AI_AUDIO_ERR_BUSY;
    }
    if (playback_service &&
        (s_stopped || (s_active & OP_CONTROL) || !s_aec_running || !s_aec_play_active || s_aec_primed < 3U)) {
        (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
        return 0;
    }
#endif

    sw_ret = Liot_AudioSetVolume(speaker_volume);
    codec_ret = Liot_AudioSetCodecVolume(codec_volume);
    mic_ret = Liot_AudioSetMicVolume(mic_gain, mic_volume);

    if (sw_ret != L_AUD_ERR_SUCCESS ||
        codec_ret != L_AUD_ERR_SUCCESS ||
        mic_ret != L_AUD_ERR_SUCCESS)
    {
        s_levels_dirty = true;
#if DEMO_AI_AUDIO_AEC_ENABLE
        (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
#endif
        liot_trace("[AI-AUDIO] level apply failed ret=%d/%d/%d\r\n",
                   (int)sw_ret, (int)codec_ret, (int)mic_ret);
        return -2;
    }

    /* Stop may publish its gate while waiting for our codec writes. Preserve
     * dirty in that case so a later session restores the level after mute. */
    s_levels_dirty = false;
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_aec_levels_checked_at = liot_rtos_get_running_time();
    if (s_aec_stopping) s_levels_dirty = true;
    (void)liot_rtos_mutex_unlock(s_aec_levels_mutex);
#endif
    /* Preserve a simultaneous UI update for the next service point. */
    if (speaker_level != s_speaker_level || mic_level != s_mic_level)
    {
        s_levels_dirty = true;
    }
    return 0;
}
