#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define TIRTC_AUDIO_FREE_GUARD_ENABLE 1
#include "../media/audio_free_guard.c"

pro_pcb_pool_t pro_audio_pcb_pool;
static void *entity;
static void *received;
static unsigned calls, critical, returns, events;
static voice_ret_callback_t registered;
void *ccioGetAudioServEnt(void) { return entity; }
uint8_t ccioRbufGetFreePcbCnt(void) { assert(critical == 1); return pro_audio_pcb_pool.free_count; }
void __real_audioFreeRecordBuf(void *pdu) { assert(entity != 0); received = pdu; ++calls; }
void liot_rtos_enter_critical(void) { assert(critical++ == 0); }
void liot_rtos_exit_critical(void) { assert(critical-- == 1); }
void __real_halSetVoiceEngRetCallback(voice_ret_callback_t callback) { registered = callback; }
static void vendor_callback(uint32_t event, void *data) { (void)data; events += event == 3U; }
void ccioRbufUngetPtUlPcb(void *pdu)
{
    pro_pcb_t *node;
    assert(critical == 1 && pcb_index(pdu) >= 0);
    for (node = pro_audio_pcb_pool.free_head; node; node = node->next) assert(node != pdu);
    node = pdu;
    node->next = pro_audio_pcb_pool.free_head;
    pro_audio_pcb_pool.free_head = node;
    ++pro_audio_pcb_pool.free_count;
    ++pro_audio_pcb_pool.return_count;
    ++returns;
}
static pro_pcb_t *node(unsigned i) { return &pro_audio_pcb_pool.slots[i].pdu; }
static void initialize(unsigned busy)
{
    pro_audio_pcb_pool.initialized = 1;
    for (unsigned i = busy; i < 16U; ++i) {
        node(i)->next = pro_audio_pcb_pool.free_head;
        pro_audio_pcb_pool.free_head = node(i);
        ++pro_audio_pcb_pool.free_count;
    }
    for (unsigned i = 0U; i < busy; ++i) node(i)->next = i + 1U < busy ? node(i + 1U) : 0;
    __wrap_halSetVoiceEngRetCallback(vendor_callback);
    assert(registered != vendor_callback && registered != 0);
}
int main(int argc, char **argv)
{
    demo_audio_free_guard_stats_t stats;
    const char *scenario = argc == 2 ? argv[1] : "chain";
    initialize(strcmp(scenario, "chain") == 0 ? 3U : 1U);
    demo_audio_free_guard_snapshot(0);
    if (strcmp(scenario, "forward") == 0) {
        entity = &entity;
        __wrap_audioFreeRecordBuf(node(0));
        assert(calls == 1 && received == node(0) && s_pending == 0);
    } else if (strcmp(scenario, "repeated") == 0) {
        for (unsigned i = 0U; i < 32U; ++i) {
            pro_pcb_t *pdu = i == 0U ? node(0) : pro_audio_pcb_pool.free_head;
            if (i != 0U) {
                pro_audio_pcb_pool.free_head = pdu->next;
                --pro_audio_pcb_pool.free_count;
                ++pro_audio_pcb_pool.alloc_count;
            }
            pdu->next = 0;
            demo_audio_free_guard_begin_stop();
            __wrap_audioFreeRecordBuf(pdu);
            registered(3U, 0);
            assert(demo_audio_free_guard_reclaim() == 0);
            assert(pro_audio_pcb_pool.free_count == 16U && s_pending == 0U);
        }
        assert(s_reclaimed == 32U && events == 32U);
    } else {
        demo_audio_free_guard_begin_stop();
        if (strcmp(scenario, "reallocated-before-callback") == 0) ++pro_audio_pcb_pool.alloc_count;
        __wrap_audioFreeRecordBuf(node(0));
        if (strcmp(scenario, "chain") == 0) __wrap_audioFreeRecordBuf(node(0));
        if (strcmp(scenario, "unconfirmed") != 0) registered(3U, 0);
        if (strcmp(scenario, "already-free") == 0) {
            liot_rtos_enter_critical();
            ccioRbufUngetPtUlPcb(node(0));
            liot_rtos_exit_critical();
        }
        if (strcmp(scenario, "corrupt-free-list") == 0) pro_audio_pcb_pool.free_head->next = pro_audio_pcb_pool.free_head;
        if (strcmp(scenario, "entity-live") == 0) entity = &entity;
        if (strcmp(scenario, "reallocated") == 0) ++pro_audio_pcb_pool.alloc_count;
        if (strcmp(scenario, "unconfirmed") == 0 || strcmp(scenario, "corrupt-free-list") == 0 || strcmp(scenario, "entity-live") == 0 || strcmp(scenario, "reallocated") == 0 || strcmp(scenario, "reallocated-before-callback") == 0) {
            assert(demo_audio_free_guard_reclaim() != 0);
            assert(returns == 0U && s_fault != 0U && s_pending != 0U);
        } else {
            assert(demo_audio_free_guard_reclaim() == 0);
            assert(pro_audio_pcb_pool.free_count == 16U && s_pending == 0U && s_fault == 0U);
            assert(returns == (strcmp(scenario, "chain") == 0 ? 3U : 1U));
            if (strcmp(scenario, "already-free") == 0) assert(s_reclaimed == 0U);
        }
    }
    demo_audio_free_guard_snapshot(&stats);
    assert(critical == 0 && stats.pending == s_pending);
    printf("PASS: %s\n", scenario);
    return 0;
}
