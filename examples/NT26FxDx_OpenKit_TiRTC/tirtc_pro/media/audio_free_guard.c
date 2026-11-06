#include "audio_free_guard.h"

#if defined(TIRTC_AUDIO_FREE_GUARD_ENABLE) && TIRTC_AUDIO_FREE_GUARD_ENABLE
#include "liot_os.h"
#include <stddef.h>

/* Audited F6D_D ABI. This late-buffer guard is not a general SDK
 * replacement. A late VEM IPC ISR can run after the CCIO task unbinds the
 * audio entity. The vendor function dereferences that entity unconditionally.
 * Keep returned PCB identities until the stop handshake and SDK teardown
 * finish. At that point the old ring is gone; only its static PCB can remain.
 * Never dequeue against the deleted ring, or return a PCB twice. */
extern void *ccioGetAudioServEnt(void);
extern uint8_t ccioRbufGetFreePcbCnt(void);
extern void __real_audioFreeRecordBuf(void *pdu);
extern void ccioRbufUngetPtUlPcb(void *pdu);
typedef void (*voice_ret_callback_t)(uint32_t event, void *data);
extern void __real_halSetVoiceEngRetCallback(voice_ret_callback_t callback);

/* F6D_D static pool ABI, independently checked against its DWARF and
 * Get/Unget implementations. Entry includes a 12-byte ring prefix. The
 * linker binds this object only with the pinned base ELF hash. */
typedef struct pro_pcb {
    uint8_t flags_and_length[4];
    void *pcm;
    struct pro_pcb *next;
    uint8_t metadata[8];
} pro_pcb_t;
typedef struct { uint8_t prefix[12]; pro_pcb_t pdu; } pro_pcb_slot_t;
typedef struct {
    uint8_t initialized, dynamic, free_count, reserved;
    pro_pcb_t *free_head;
    pro_pcb_slot_t slots[16];
    uint32_t alloc_count, return_count;
} pro_pcb_pool_t;
extern pro_pcb_pool_t pro_audio_pcb_pool;
#if !defined(PRO_AUDIO_FREE_GUARD_HOST_TEST)
typedef char pcb_abi_pointer[(sizeof(void *) == 4) ? 1 : -1];
typedef char pcb_abi_size[(sizeof(pro_pcb_slot_t) == 32) ? 1 : -1];
typedef char pcb_abi_next[(offsetof(pro_pcb_t, next) == 8) ? 1 : -1];
typedef char pcb_abi_head[(offsetof(pro_pcb_pool_t, free_head) == 4) ? 1 : -1];
typedef char pcb_abi_first[(offsetof(pro_pcb_pool_t, slots[0].pdu) == 20) ? 1 : -1];
typedef char pcb_abi_alloc[(offsetof(pro_pcb_pool_t, alloc_count) == 520) ? 1 : -1];
#endif

static volatile uint32_t s_skipped;
static volatile uintptr_t s_last_pdu;
static volatile uint32_t s_pending, s_fault, s_stop_confirmations;
static uint32_t s_reclaimed, s_stop_baseline;
static uint32_t s_stop_alloc_count;
static volatile int s_stop_armed;
static voice_ret_callback_t volatile s_vendor_callback;

static int pcb_index(const pro_pcb_t *pdu)
{
    uintptr_t first = (uintptr_t)&pro_audio_pcb_pool.slots[0].pdu;
    uintptr_t address = (uintptr_t)pdu;
    uintptr_t delta;
    if (address < first) return -1;
    delta = address - first;
    if (delta % sizeof(pro_pcb_slot_t) || delta / sizeof(pro_pcb_slot_t) >= 16U)
        return -1;
    return (int)(delta / sizeof(pro_pcb_slot_t));
}

static void voice_ret_bridge(uint32_t event, void *data)
{
    voice_ret_callback_t callback = s_vendor_callback;
    if (callback != 0) callback(event, data);
    /* Same IPC ISR, after forwarding the real confirmation to the vendor.
     * A task cannot resume until this ISR has completed. */
    if (event == 3U) ++s_stop_confirmations;
}

void __wrap_halSetVoiceEngRetCallback(voice_ret_callback_t callback)
{
    s_vendor_callback = callback;
    __real_halSetVoiceEngRetCallback(callback == 0 ? callback : voice_ret_bridge);
}

void __wrap_audioFreeRecordBuf(void *pdu)
{
    /* Called by the vendor VEM IPC ISR; no allocation, logging, waits, or
     * task locks. The unbinding task cannot run between this test and real. */
    if (ccioGetAudioServEnt() == 0) {
        pro_pcb_t *node = (pro_pcb_t *)pdu;
        uint32_t chain = 0U;
        s_last_pdu = (uintptr_t)pdu;
        if (s_skipped != UINT32_MAX) ++s_skipped;
        if (pro_audio_pcb_pool.initialized != 1U || pro_audio_pcb_pool.dynamic != 0U) {
            s_fault = 1U;
            return;
        }
        if (!s_stop_armed) {
            s_fault = 5U;
            return;
        }
        /* Snapshot the complete returned chain now. SDK cleanup can later
         * repurpose next for its free list. A bit per static slot deduplicates
         * callbacks and bounds ISR work to at most sixteen nodes. */
        while (node != 0) {
            int index = pcb_index(node);
            uint32_t bit;
            if (index < 0) { s_fault = 2U; break; }
            bit = 1U << (unsigned)index;
            if (chain & bit) { s_fault = 3U; break; }
            if (s_pending & bit) break;
            chain |= bit;
            s_pending |= bit;
            node = node->next;
        }
        return;
    }
    __real_audioFreeRecordBuf(pdu);
}

void demo_audio_free_guard_begin_stop(void)
{
    liot_rtos_enter_critical();
    s_stop_baseline = s_stop_confirmations;
    s_stop_alloc_count = pro_audio_pcb_pool.alloc_count;
    s_stop_armed = 1;
    liot_rtos_exit_critical();
}

static int free_mask(uint32_t *mask)
{
    pro_pcb_t *node = pro_audio_pcb_pool.free_head;
    uint32_t result = 0U;
    unsigned count = 0U;
    while (node != 0) {
        int index = pcb_index(node);
        uint32_t bit;
        if (index < 0) return -1;
        bit = 1U << (unsigned)index;
        if (result & bit) return -1;
        result |= bit;
        ++count;
        node = node->next;
    }
    if (count != pro_audio_pcb_pool.free_count || count > 16U) return -1;
    *mask = result;
    return 0;
}

int demo_audio_free_guard_reclaim(void)
{
    uint32_t available = 0U;
    int result = -1;
    liot_rtos_enter_critical();
    /* Public RecordStreamStop returns success even when its internal
     * confirmation wait fails. Require the real STOP_CNF for this request.
     * Never rebuild or guess at ownership if the barrier was not reached. */
    if (!s_fault && s_stop_armed && s_stop_confirmations != s_stop_baseline &&
        ccioGetAudioServEnt() == 0 && pro_audio_pcb_pool.initialized == 1U &&
        pro_audio_pcb_pool.dynamic == 0U && free_mask(&available) == 0 &&
        /* This is a shared CCIO pool. If allocation happened during this
         * stop, an absent node might have been returned and reused by
         * another producer, even before its old callback arrived. Fail
         * before ANY return rather than take another owner's node. */
        (!(s_pending & ~available) ||
         pro_audio_pcb_pool.alloc_count == s_stop_alloc_count)) {
        for (unsigned i = 0U; i < 16U; ++i) {
            uint32_t bit = 1U << i;
            if (!(s_pending & bit)) continue;
            if (!(available & bit)) {
                /* I2S and VEM are stopped, old ring/entity destroyed, and
                 * this returned node is absent from the SDK's free list. */
                ccioRbufUngetPtUlPcb(&pro_audio_pcb_pool.slots[i].pdu);
                ++s_reclaimed;
                available |= bit;
            }
            s_pending &= ~bit;
        }
        result = free_mask(&available);
        if (result == 0) s_stop_armed = 0;
    }
    if (result != 0 && s_fault == 0U) s_fault = 4U;
    liot_rtos_exit_critical();
    return result;
}

void demo_audio_free_guard_snapshot(demo_audio_free_guard_stats_t *out)
{
    if (out == 0) return;
    liot_rtos_enter_critical();
    out->skipped = s_skipped;
    out->last_pdu = s_last_pdu;
    out->free_pcb = ccioRbufGetFreePcbCnt();
    out->pending = s_pending;
    out->reclaimed = s_reclaimed;
    out->stop_confirmations = s_stop_confirmations;
    out->fault = s_fault;
    liot_rtos_exit_critical();
}
#endif
