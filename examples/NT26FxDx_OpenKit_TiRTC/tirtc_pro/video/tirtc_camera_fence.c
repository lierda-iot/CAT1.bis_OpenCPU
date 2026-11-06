/* F6D_D completion adapter (re-audited 2026-10-09), guarded by SDK hashes in the board Makefile.
 * Liot_CspiRecv consumes a semaphore also released by the USP frame-end IRQ;
 * it is not proof that the DMA has finished writing the capture buffer.
 * The SDK's cspiDmaRxEvent invokes its registered callback with event 1 only
 * after DMA_END (hardware event 3). Intercept that registration, preserving
 * the original callback and the original single DMA-channel initialization.
 * See tirtc_app.mk for the required CSPI/camera/base ELF hashes. */
#include "tirtc_camera_fence.h"
#include "liot_os.h"
#include <stddef.h>

#ifndef TIRTC_CAMERA_F6D_ABI_VERIFIED
#error The camera completion adapter requires the audited F6D_D SDK
#endif

/* Exact 20-byte ARM table from cspiDrvInterface_t in the base ELF DWARF.
 * This is an API table copy, never a private resource/handle layout or MMIO
 * address. Successful operations keep the original parameters; initial FULL
 * power passes before init, but failed init cannot operate an unowned DMA. */
typedef enum { FENCE_CSPI_POWER_OFF, FENCE_CSPI_POWER_FULL } fence_power_t;
typedef struct {
    int32_t (*init)(camRecvCb callback);
    int32_t (*deInit)(void);
    int32_t (*powerCtrl)(fence_power_t state);
    int32_t (*recv)(void);
    int32_t (*ctrl)(uint32_t control, uint32_t argument);
} fence_cspi_interface_t;
#if UINTPTR_MAX == UINT32_MAX
typedef char fence_table_size_check[sizeof(fence_cspi_interface_t)==20 ? 1 : -1];
typedef char fence_table_ctrl_check[offsetof(fence_cspi_interface_t,ctrl)==16 ? 1 : -1];
#endif

extern const fence_cspi_interface_t *__real_liot_cspi_get_intf(uint32_t port);
static const fence_cspi_interface_t *s_vendor;
static fence_cspi_interface_t s_proxy;
static camRecvCb s_vendor_callback;
/* One persistent binary semaphore, allocated in task context before init.
 * Retaining it across calls removes ISR-vs-semaphore-delete lifetime races. */
static liot_sem_t s_done;
static bool s_ready, s_init_failed;
static volatile uint32_t s_in_flight;
static volatile uint32_t s_completions, s_unexpected;
/* Per-flight ISR timestamps are separate from the last returned call's
 * diagnostics. A late DMA may release ownership, but cannot publish timing
 * for a call which already returned a timeout. */
static volatile uint32_t s_dma_completed_at, s_dma_timing_ready;
static uint32_t s_captures, s_timeouts, s_early, s_wait_ms;
static uint32_t s_dma_ms, s_resume_ms, s_sdk_ms;
static bool s_timing_valid;
static int s_sdk_result;
/* Verified cspiControl commands in the hash-gated F6D_D base image. These
 * only clear CSPICTL.enable / set DMACTL.rxFifoFlush; neither stops or waits
 * for a DMA channel. Do not substitute CSPI_CTRL_TRANSABORT (command 1). */
#define FENCE_CSPI_START_STOP (1UL << 17)
#define FENCE_CSPI_FLUSH_RX_FIFO (1UL << 16)
static uint32_t s_rearm_prepares, s_rearm_stop_errors, s_rearm_flush_errors;
static int s_rearm_result;

static void fence_barrier(void)
{
#if defined(__arm__) || defined(__thumb__)
    __asm__ volatile("dmb" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

static void dma_completed(uint32_t event)
{
    /* ISR: no allocation, logging, task critical section, or blocking. The
     * audited liot release wrapper uses xQueueGiveFromISR in IRQ context. */
    if (s_vendor_callback) s_vendor_callback(event);
    if (event==1U && s_in_flight) {
        /* The audited time wrapper selects xTaskGetTickCountFromISR here.
         * Timestamp AFTER the vendor callback to preserve its ordering. This
         * observes the ISR, not the exact physical end of sensor exposure. */
        s_dma_completed_at=liot_rtos_get_running_time();
        s_dma_timing_ready=1U;
        s_completions++;
        fence_barrier();
        s_in_flight=0U;
        (void)liot_rtos_semaphore_release(s_done);
    } else {
        s_unexpected++;
    }
}

static int32_t proxy_init(camRecvCb callback)
{
    int32_t result;
    if (!s_vendor || !callback || s_ready || s_in_flight || s_init_failed)
        return TIRTC_CAMERA_FENCE_UNAVAILABLE;
    if (!tirtc_camera_fence_prepare()) {
        s_init_failed=true;
        return TIRTC_CAMERA_FENCE_UNAVAILABLE;
    }
    s_vendor_callback=callback;
    result=s_vendor->init(dma_completed);
    s_ready=result==0;
    if (!s_ready) { s_vendor_callback=NULL; s_init_failed=true; }
    return result;
}

static int32_t proxy_deinit(void)
{
    int32_t result;
    /* Callers must quiesce BEFORE Liot_CameraDeinit: the vendor deletes its
     * semaphore earlier than this function. This check is a last guard only. */
    if (s_in_flight) return TIRTC_CAMERA_FENCE_BUSY;
    if (!s_vendor) return TIRTC_CAMERA_FENCE_UNAVAILABLE;
    /* Liot_CspiInit ignores init's result. On the verified backend its only
     * failure is DMA_openChannel, before init-count increment/channel setup;
     * calling deInit then would decrement an unowned count/close a channel. */
    if (!s_ready) return 0;
    result=s_vendor->deInit();
    if (result==0) { s_ready=false; s_vendor_callback=NULL; }
    return result;
}

static int32_t proxy_recv(void)
{
    return s_ready ? s_vendor->recv() : TIRTC_CAMERA_FENCE_UNAVAILABLE;
}

static int32_t proxy_power(fence_power_t state)
{
    /* FULL must pass before init. After init failure even POWER_OFF is unsafe:
     * the verified SDK would stop an unowned/stale DMA channel. This is a
     * fail-closed state until reboot, not a claimed rollback of PAD/doze state. */
    return !s_init_failed ? s_vendor->powerCtrl(state) : TIRTC_CAMERA_FENCE_UNAVAILABLE;
}

static int32_t proxy_ctrl(uint32_t control, uint32_t argument)
{
    /* All vendor ctrl calls follow init. Do not start/configure an unowned
     * DMA channel when the vendor continues after an initialization error. */
    return s_ready ? s_vendor->ctrl(control,argument) : TIRTC_CAMERA_FENCE_UNAVAILABLE;
}

const fence_cspi_interface_t *__wrap_liot_cspi_get_intf(uint32_t port)
{
    const fence_cspi_interface_t *vendor=__real_liot_cspi_get_intf(port);
    if (port!=1U || !vendor) return vendor;
    if (!s_vendor) {
        s_vendor=vendor;
        s_proxy=*vendor;
        s_proxy.init=proxy_init;
        s_proxy.deInit=proxy_deinit;
        s_proxy.powerCtrl=proxy_power;
        s_proxy.recv=proxy_recv;
        s_proxy.ctrl=proxy_ctrl;
    }
    return vendor==s_vendor ? &s_proxy : NULL;
}

bool tirtc_camera_fence_prepare(void)
{
    if (s_ready || s_in_flight || s_init_failed) return false;
    if (s_done) return true;
    if (liot_rtos_semaphore_create(&s_done,0U)!=LIOT_OSI_SUCCESS) {
        s_done=NULL;
        return false;
    }
    return s_done!=NULL;
}

bool tirtc_camera_fence_ready(void) { return s_ready; }
bool tirtc_camera_fence_idle(void)
{
    bool idle=s_in_flight==0U;
    fence_barrier();
    return idle;
}

static bool wait_idle(uint32_t began, uint32_t timeout_ms)
{
    while (!tirtc_camera_fence_idle()) {
        uint32_t elapsed=liot_rtos_get_running_time()-began;
        if (elapsed>=timeout_ms || !s_done) return false;
        /* A token only wakes us. The DMA-owned state, not semaphore count,
         * proves completion, so an old token cannot bypass this fence. */
        LiotOSStatus_t result=liot_rtos_semaphore_wait(s_done,timeout_ms-elapsed);
        if (result!=LIOT_OSI_SUCCESS && !tirtc_camera_fence_idle()) return false;
    }
    return true;
}

bool tirtc_camera_quiesce(uint32_t timeout_ms)
{
    return wait_idle(liot_rtos_get_running_time(),timeout_ms);
}

int tirtc_camera_capture(liot_camera_handle_t camera, uint8_t *buffer,
                         uint32_t timeout_ms)
{
    uint32_t began, after_sdk, returned_at;
    int sdk_result;
    bool idle;
    if (!camera || !buffer || !timeout_ms || timeout_ms>60000U)
        return TIRTC_CAMERA_FENCE_ARGUMENT;
    if (!s_ready || !s_done) return TIRTC_CAMERA_FENCE_UNAVAILABLE;
    if (!tirtc_camera_fence_idle()) return TIRTC_CAMERA_FENCE_BUSY;
    while (liot_rtos_semaphore_wait(s_done,LIOT_NO_WAIT)==LIOT_OSI_SUCCESS) { }
    /* DMA_END proves that the old buffer is no longer being written, but a
     * later vendor frame-end IRQ can reopen FIFO output. Stop the receiver
     * and discard any queued pixels BEFORE the SDK resets/rearms it. Its
     * normal capture call then installs DMA before enabling reception again.
     * This path is never entered while a timed-out DMA still owns a buffer. */
    sdk_result=s_vendor->ctrl(FENCE_CSPI_START_STOP,0U);
    if (sdk_result!=0) {
        liot_rtos_enter_critical();
        s_rearm_stop_errors++; s_rearm_result=sdk_result; s_timing_valid=false;
        liot_rtos_exit_critical();
        return TIRTC_CAMERA_FENCE_REARM_STOP;
    }
    sdk_result=s_vendor->ctrl(FENCE_CSPI_FLUSH_RX_FIFO,0U);
    if (sdk_result!=0) {
        liot_rtos_enter_critical();
        s_rearm_flush_errors++; s_rearm_result=sdk_result; s_timing_valid=false;
        liot_rtos_exit_critical();
        return TIRTC_CAMERA_FENCE_REARM_FLUSH;
    }
    liot_rtos_enter_critical();
    s_rearm_prepares++; s_rearm_result=0;
    liot_rtos_exit_critical();
    /* Publish this flight's timing state before DMA can be marked active.
     * The previous flight must already be idle, including after a timeout. */
    began=liot_rtos_get_running_time();
    s_dma_timing_ready=0U;
    fence_barrier();
    s_in_flight=1U;
    fence_barrier();
    s_captures++;
    sdk_result=(int)Liot_CameraCaptureImage(camera,buffer,timeout_ms);
    after_sdk=liot_rtos_get_running_time();
    if (sdk_result==0 && !tirtc_camera_fence_idle()) s_early++;
    idle=wait_idle(began,timeout_ms);
    returned_at=liot_rtos_get_running_time();
    /* Only this owning task publishes completed-call diagnostics. The
     * acquire barrier in wait_idle makes the ISR timestamp visible. Keep the
     * last completed call while the next is active, and invalidate failures;
     * an ISR arriving after a timeout cannot revive an invalid measurement. */
    liot_rtos_enter_critical();
    s_sdk_result=sdk_result;
    s_wait_ms=returned_at-after_sdk;
    s_timing_valid=idle && sdk_result==0 && s_dma_timing_ready!=0U;
    if (s_timing_valid) {
        s_dma_ms=s_dma_completed_at-began;
        s_resume_ms=returned_at-s_dma_completed_at;
        s_sdk_ms=after_sdk-began;
    }
    liot_rtos_exit_critical();
    if (!idle) {
        s_timeouts++;
        /* Keep the DMA buffer leased. Do not reset s_in_flight or synthesize
         * success even if the vendor returned success from a stale token. */
        return TIRTC_CAMERA_FENCE_TIMEOUT;
    }
    return sdk_result;
}

void tirtc_camera_fence_get_stats(tirtc_camera_fence_stats_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical();
    out->captures=s_captures; out->completions=s_completions;
    out->timeouts=s_timeouts; out->sdk_early_returns=s_early;
    out->unexpected_events=s_unexpected; out->last_wait_ms=s_wait_ms;
    out->last_dma_ms=s_dma_ms; out->last_resume_ms=s_resume_ms;
    out->last_sdk_ms=s_sdk_ms; out->last_timing_valid=s_timing_valid;
    out->last_sdk_result=s_sdk_result; out->in_flight=s_in_flight!=0U;
    out->rearm_prepares=s_rearm_prepares;
    out->rearm_stop_errors=s_rearm_stop_errors;
    out->rearm_flush_errors=s_rearm_flush_errors;
    out->last_rearm_result=s_rearm_result;
    liot_rtos_exit_critical();
}
