/**
 * @file gx8006_protocol.c
 * @brief GX8006 protocol layer implementation
 * @details Handles frame TX/RX, command parsing, and event dispatch.
 *          Creates RX/TX/EVT tasks internally with message queue based async communication.
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#include <string.h>

#include "liot_os.h"
#include "liot_uart2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "gx8006.h"

#define GX_TRACE(fmt, ...) liot_trace("[GX8006] " fmt, ##__VA_ARGS__)

#ifndef GX8006_TASK_STACK_SIZE
#define GX8006_TASK_STACK_SIZE              (5 * 1024)
#endif

#ifndef GX8006_TASK_PRIORITY
#define GX8006_TASK_PRIORITY                APP_PRIORITY_NORMAL
#endif

#ifndef GX8006_QUEUE_DEPTH
#define GX8006_QUEUE_DEPTH                  32
#endif

#ifndef GX8006_CMD_TIMEOUT
#define GX8006_CMD_TIMEOUT                  2000
#endif

#ifndef GX8006_ACK_TIMEOUT
#define GX8006_ACK_TIMEOUT                  50
#endif

#ifndef GX8006_SPK_CMD_RETRY
#define GX8006_SPK_CMD_RETRY                3
#endif

#ifndef GX8006_SPK_FRAME_PACING_MS
#define GX8006_SPK_FRAME_PACING_MS          15
#endif

/* ========== Private api ========== */
extern void gx8006_hw_uart_send(uint8_t *data, uint16_t len);
extern void gx8006_spk_set_status(gx8006_spk_status_e s);
extern gx8006_spk_status_e gx8006_spk_get_status(void);

/* ========== Private data ========== */
static gx8006_evt_cb_t g_evt_cb   = NULL;
static liot_task_t  g_tx_task     = NULL;
static liot_task_t  g_evt_task    = NULL;
static liot_sem_t   g_tx_done_sem = NULL;
static liot_sem_t   g_tx_ack_sem  = NULL;
static liot_queue_t g_tx_q        = NULL;
static liot_queue_t g_evt_q       = NULL;

static uint8_t  g_tx_ack_result = 0;
static void    *g_tx_resp_arg   = NULL;

typedef struct {
    uint8_t   sync;
    uint8_t  *data;
    uint32_t  data_len;
} gx8006_tx_req_t;

typedef struct {
    uint8_t   evt;
    uint8_t  *data;
    uint32_t  len;
} gx8006_evt_msg_t;

#define EVT_MSG_MIC_RAW      0xFF
#define EVT_MSG_MCU_VERSION  0xFE

/* ========== Frame RX/TX interface ========== */

void gx8006_notify_ack_fail(void)
{
    if (g_tx_ack_sem) {
        g_tx_ack_result = 0x0;
        liot_rtos_semaphore_release(g_tx_ack_sem);
    }
}

static void evt_post(uint8_t evt, uint8_t *data, uint32_t len)
{
    gx8006_evt_msg_t msg = {.evt = evt, .data = data, .len = len};
    if (liot_rtos_queue_release(g_evt_q, sizeof(gx8006_evt_msg_t), (uint8_t *)&msg, LIOT_NO_WAIT) != LIOT_OSI_SUCCESS) {
        if (data) liot_rtos_free(data);
    }
}

uint32_t gx8006_get_tx_pending(void)
{
    uint32_t cnt = 0;
    if (g_tx_q)
        liot_rtos_queue_get_cnt(g_tx_q, &cnt);
    return cnt;
}

void gx8006_frame_recv(uint8_t *data, uint32_t data_len)
{
    if (data_len < PROTOCOL_HEAD_LEN + 1) return;

    uint8_t sum = 0;
    for (uint32_t i = 0; i < data_len - 1; i++)
        sum += data[i];
    if (sum != data[data_len - 1]) {
        GX_TRACE("checsum error expect=0x%02X, calc=0x%02X", sum, data[data_len - 1]);
        gx8006_notify_ack_fail();
        return;
    }

    if (data[3] != VOICE_CMD_WORD) return;

    uint8_t  *payload     = data + PROTOCOL_HEAD_LEN;
    uint16_t  payload_len = ((uint16_t)data[4] << 8) | data[5];
    uint8_t   sub         = payload[0];

    switch (sub) {
        case RECV_SPK_DATA_CMD:
            g_tx_ack_result = (payload_len > 1) ? payload[1] : 0x00;
            liot_rtos_semaphore_release(g_tx_ack_sem);
            break;
        case GET_MCU_VERSION_CMD:
            if (g_tx_resp_arg && payload_len > 4)
                memcpy(g_tx_resp_arg, payload + 4, payload_len - 4);
            liot_rtos_semaphore_release(g_tx_ack_sem);
            evt_post(EVT_MSG_MCU_VERSION, NULL, 0);
            break;
        case SEND_MIC_DATA_CMD: {
            uint8_t *buf = liot_rtos_malloc(payload_len);
            if (!buf) break;
            memcpy(buf, payload, payload_len);
            evt_post(EVT_MSG_MIC_RAW, buf, payload_len);
            break;
        }
        case SEND_OFFLINE_VOICE_AWAKE_CMD:
            evt_post(GX_EVT_AWAKEN, NULL, 0);
            break;
        case SEND_OFFLINE_VOICE_TIMEOUT_CMD:
            if (gx8006_get_chat_mode() == GX_Q_AND_A_CHAT_MODE ||
                gx8006_get_chat_mode() == GX_NATURAL_CHAT_MODE)
                evt_post(GX_EVT_AWAKEN_TIMEOUT, NULL, 0);
            break;
        default:
            break;
    }
}

/**
 * @brief Send frame request
 * @details Copies data to heap and posts to TX queue. If sync=1, blocks waiting for
 *          module ACK response. arg receives sync command return data (e.g. version).
 * @param[in] data     Payload data to send
 * @param[in] data_len Payload length
 * @param[in] sync     1 = synchronous wait for ACK, 0 = asynchronous
 * @param[in] arg      Buffer for response data in sync mode, NULL for async
 * @return ACK result in sync mode (0 = success), 0 in async mode
 */
uint8_t gx8006_frame_send(uint8_t *data, uint32_t data_len, uint8_t sync, void *arg)
{
    gx8006_tx_req_t req = {0};
    req.sync     = sync;
    req.data_len = data_len;
    req.data     = liot_rtos_malloc(data_len);
    if (!req.data) return 0;

    memcpy(req.data, data, data_len);
    liot_rtos_queue_release(g_tx_q, sizeof(gx8006_tx_req_t), (uint8_t *)&req, LIOT_WAIT_FOREVER);

    if (sync) {
        g_tx_resp_arg = arg;
        liot_rtos_semaphore_wait(g_tx_done_sem, LIOT_WAIT_FOREVER);
        g_tx_resp_arg = NULL;
    }

    return g_tx_ack_result;
}

/**
 * @brief Parse MIC data sub-commands
 * @details Dispatches VAD events by sub-command type:
 *          0x00 = VAD start (stops SPK in natural chat mode),
 *          0x01 = VAD data, 0x02 = VAD end (notifies timeout in non-natural mode)
 * @param[in] data     Payload data (data[1] is sub-command)
 * @param[in] data_len Payload length
 */
static void gx8006_mic_parse(uint8_t *data, uint16_t data_len)
{
    switch (data[1]) {
        case 0x00:
            GX_TRACE("MIC recv start");
            if (gx8006_get_chat_mode() == GX_NATURAL_CHAT_MODE)
                gx8006_spk_set_status(GX_SPK_IDLE);
            if (g_evt_cb) g_evt_cb(GX_EVT_MIC_VAD_START, NULL, 0);
            break;
        case 0x01:
            if (g_evt_cb) g_evt_cb(GX_EVT_MIC_VAD_DATA, data + 2, data_len - 2);
            break;
        case 0x02:
            GX_TRACE("MIC recv end");
            if (gx8006_get_chat_mode() != GX_NATURAL_CHAT_MODE)
                gx8006_set_vad_is_timeout();
            if (g_evt_cb) g_evt_cb(GX_EVT_MIC_VAD_END, NULL, 0);
            break;
        default:
            break;
    }
}

/* ========== TX Task ========== */

/**
 * @brief Calculate checksum
 * @param[in] buf Data buffer
 * @param[in] len Data length
 * @return Sum of all bytes (truncated to uint8_t)
 */
static uint8_t calc_checksum(const uint8_t *buf, uint32_t len)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) sum += buf[i];
    return sum;
}

/**
 * @brief TX task function
 * @details Loops taking send requests from TX queue, assembles complete protocol frame
 *          (header + version + cmd + length + payload + checksum) and sends via UART.
 *          For sync requests, waits for ACK semaphore then releases done semaphore.
 */
static void tx_task_fn(void *arg)
{
    (void)arg;
    gx8006_tx_req_t req;
    uint8_t txbuf[GX8006_OPUS_MAX_FRAME_LENGTH + PROTOCOL_HEAD_LEN + 1];

    while (1) {
        liot_rtos_queue_wait(g_tx_q, (uint8_t *)&req, sizeof(gx8006_tx_req_t), LIOT_WAIT_FOREVER);

        txbuf[0] = FRAME_FIRST;
        txbuf[1] = FRAME_SECOND;
        txbuf[2] = MCU_RX_VER;
        txbuf[3] = VOICE_CMD_WORD;
        txbuf[4] = (req.data_len >> 8) & 0xFF;
        txbuf[5] = req.data_len & 0xFF;
        if (req.data_len > 0)
            memcpy(&txbuf[PROTOCOL_HEAD_LEN], req.data, req.data_len);
        txbuf[PROTOCOL_HEAD_LEN + req.data_len] = calc_checksum(txbuf, PROTOCOL_HEAD_LEN + req.data_len);

        uint8_t retry = 0;
        uint32_t ack_timeout = req.sync ? GX8006_CMD_TIMEOUT : GX8006_ACK_TIMEOUT;
        do {
            gx8006_hw_uart_send(txbuf, PROTOCOL_HEAD_LEN + req.data_len + 1);
            g_tx_ack_result = 0;
            liot_rtos_semaphore_wait(g_tx_ack_sem, ack_timeout);
            if (g_tx_ack_result) retry++;
        } while (g_tx_ack_result && retry <= GX8006_SPK_CMD_RETRY);

        liot_rtos_free(req.data);
        if (req.sync)
            liot_rtos_semaphore_release(g_tx_done_sem);
        else
            liot_rtos_task_sleep_ms(GX8006_SPK_FRAME_PACING_MS);
    }
}

/* ========== Event Task ========== */

/**
 * @brief Event dispatch task function
 * @details Loops taking event messages from event queue and invokes user callback.
 *          Separate task avoids running time-consuming callbacks in UART RX context.
 */
static void evt_task_fn(void *arg)
{
    (void)arg;
    gx8006_evt_msg_t msg;

    while (1) {
        liot_rtos_queue_wait(g_evt_q, (uint8_t *)&msg, sizeof(gx8006_evt_msg_t), LIOT_WAIT_FOREVER);

        switch (msg.evt) {
        case EVT_MSG_MIC_RAW:
            gx8006_mic_parse(msg.data, msg.len);
            break;
        case EVT_MSG_MCU_VERSION: {
            uint8_t ack[2] = {0x01, 0x00};
            gx8006_frame_send(ack, 2, 0, NULL);
            break;
        }
        default:
            if (g_evt_cb)
                g_evt_cb((gx8006_evt_e)msg.evt, msg.data, msg.len);
            break;
        }

        if (msg.data)
            liot_rtos_free(msg.data);
    }
}

/* ========== Init / Deinit ========== */

/**
 * @brief Initialize protocol layer
 * @details Creates semaphores (TX done, TX ACK) and queues (TX/EVT),
 *          starts TX and EVT tasks. RX is handled inline in UART callback.
 * @param[in] evt_cb User event callback, may be NULL
 */
void gx8006_protocol_init(gx8006_evt_cb_t evt_cb)
{
    g_evt_cb = evt_cb;
    liot_rtos_semaphore_create(&g_tx_done_sem, 0);
    liot_rtos_semaphore_create(&g_tx_ack_sem, 0);
    liot_rtos_queue_create(&g_tx_q, sizeof(gx8006_tx_req_t), GX8006_QUEUE_DEPTH);
    liot_rtos_queue_create(&g_evt_q, sizeof(gx8006_evt_msg_t), GX8006_QUEUE_DEPTH);

    liot_rtos_task_create(&g_tx_task, GX8006_TASK_STACK_SIZE, GX8006_TASK_PRIORITY,
                          "gx_tx", tx_task_fn, NULL);
    liot_rtos_task_create(&g_evt_task, GX8006_TASK_STACK_SIZE, GX8006_TASK_PRIORITY,
                          "gx_evt", evt_task_fn, NULL);
}

/**
 * @brief Release protocol layer resources
 * @details Deletes TX/EVT tasks, releases semaphores and message queues
 */
void gx8006_protocol_deinit(void)
{
    if (g_tx_task)     liot_rtos_task_delete(g_tx_task);
    if (g_evt_task)    liot_rtos_task_delete(g_evt_task);
    if (g_tx_done_sem) liot_rtos_semaphore_delete(g_tx_done_sem);
    if (g_tx_ack_sem)  liot_rtos_semaphore_delete(g_tx_ack_sem);
    if (g_tx_q)        liot_rtos_queue_delete(g_tx_q);
    if (g_evt_q)       liot_rtos_queue_delete(g_evt_q);
}
