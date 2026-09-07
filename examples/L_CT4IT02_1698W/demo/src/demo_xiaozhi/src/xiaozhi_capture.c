#include "xiaozhi_core.h"
#include <stdint.h>
#include <string.h>
#include "liot_audio2.h"
#include "liot_log.h"
#include "liot_os.h"

#define XZ_CAPTURE_BLOCK_BYTES 5120
#define XZ_CAPTURE_QUEUE_DEPTH 8

typedef struct { uint8_t pcm[XZ_CAPTURE_BLOCK_BYTES]; } xz_capture_block_t;
static liot_queue_t g_capture_queue;
static liot_task_t g_capture_task;
static volatile bool g_capture_active;
static volatile bool g_capture_stop;
static volatile bool g_capture_busy;
static unsigned int g_capture_dropped;

static void xz_capture_task(void *arg)
{
    xz_capture_block_t block;
    (void)arg;
    for (;;) {
        if (!g_capture_active) { liot_rtos_task_sleep_ms(10); continue; }
        g_capture_busy = true;
        if (Liot_AudioRecord(block.pcm, sizeof(block.pcm)) == L_AUD_ERR_SUCCESS) {
            if (g_capture_active &&
                liot_rtos_queue_release(g_capture_queue, sizeof(block),
                                         (uint8_t *)&block, LIOT_NO_WAIT) != LIOT_OSI_SUCCESS) {
                ++g_capture_dropped;
                if ((g_capture_dropped & 3u) == 1u)
                    liot_trace("[xiaozhi] capture queue full dropped=%u", g_capture_dropped);
            }
        }
        g_capture_busy = false;
        if (g_capture_stop) { g_capture_active = false; g_capture_stop = false; }
    }
}

int xiaozhi_capture_init(void)
{
    LiotOSStatus_t ret = liot_rtos_queue_create(&g_capture_queue,
                                                  XZ_CAPTURE_BLOCK_BYTES,
                                                  XZ_CAPTURE_QUEUE_DEPTH);
    if (ret != LIOT_OSI_SUCCESS) return -1;
    ret = liot_rtos_task_create(&g_capture_task, 16 * 1024,
                                LIOT_APP_TASK_PRIORITY, "xiaozhi_cap",
                                xz_capture_task, NULL);
    liot_trace("[xiaozhi] capture producer create ret=%d handle=%p", ret, g_capture_task);
    return ret == LIOT_OSI_SUCCESS ? 0 : -2;
}

void xiaozhi_capture_begin(void)
{
    g_capture_dropped = 0;
    g_capture_stop = false;
    liot_rtos_queue_reset(g_capture_queue);
    g_capture_active = true;
}

void xiaozhi_capture_end(void)
{
    unsigned int waited = 0;
    g_capture_stop = true;
    while ((g_capture_active || g_capture_busy) && waited < 300) {
        liot_rtos_task_sleep_ms(5); waited += 5;
    }
    g_capture_active = false;
    liot_trace("[xiaozhi] capture producer end waited=%u dropped=%u", waited, g_capture_dropped);
}

int xiaozhi_capture_read(uint8_t *pcm, int len, uint32_t timeout)
{
    if (pcm == NULL || len != XZ_CAPTURE_BLOCK_BYTES) return -1;
    return liot_rtos_queue_wait(g_capture_queue, pcm, len, timeout) == LIOT_OSI_SUCCESS ? 0 : -2;
}

int xiaozhi_capture_read_nowait(uint8_t *pcm, int len)
{
    return xiaozhi_capture_read(pcm, len, LIOT_NO_WAIT);
}