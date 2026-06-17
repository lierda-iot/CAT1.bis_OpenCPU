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

/* ========== Private api ========== */
extern void gx8006_hw_uart_send(uint8_t *data, uint16_t len);
extern void gx8006_spk_set_status(gx8006_spk_status_e s);
extern gx8006_spk_status_e gx8006_spk_get_status(void);

/* ========== Private data ========== */
static gx8006_evt_cb_t g_evt_cb   = NULL;
static liot_task_t  g_rx_task     = NULL;
static liot_task_t  g_tx_task     = NULL;
static liot_task_t  g_evt_task    = NULL;
static liot_sem_t   g_tx_done_sem = NULL;
static liot_sem_t   g_tx_ack_sem  = NULL;
static liot_queue_t g_rx_q        = NULL;
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
    uint32_t len;
    uint8_t  data[];
} gx8006_rx_chunk_t;

/* ========== Frame RX/TX interface ========== */

void gx8006_notify_ack_fail(void)
{
    if (g_tx_ack_sem) {
        g_tx_ack_result = 0xFF;
        liot_rtos_semaphore_release(g_tx_ack_sem);
    }
}

/**
 * @brief Enqueue received frame
 * @details Called by UART parser, copies complete frame to heap and posts to RX queue.
 *          Drops the frame and frees memory if queue is full.
 * @param[in] data     Complete frame data pointer
 * @param[in] data_len Total frame length (including header and checksum)
 */
void gx8006_frame_recv(uint8_t *data, uint32_t data_len)
{

    gx8006_rx_chunk_t *chunk = liot_rtos_malloc(sizeof(gx8006_rx_chunk_t) + data_len);
    if (!chunk) return;

    chunk->len = data_len;
    memcpy(chunk->data, data, data_len);

    gx8006_rx_chunk_t *ptr = chunk;
    if (liot_rtos_queue_release(g_rx_q, sizeof(gx8006_rx_chunk_t *), (uint8_t *)&ptr, LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
        liot_rtos_free(chunk);
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
            GX_TRACE("MIC unknown sub=0x%02X, frame: %02X %02X %02X %02X",
                     data[1], data[0], data[1],
                     data_len > 2 ? data[2] : 0, data_len > 3 ? data[3] : 0);
            break;
    }
}

/* ========== RX Task ========== */

/**
 * @brief Handle voice command frame
 * @details Dispatches by first payload byte (sub-command ID):
 *          MIC data, MCU version response, wakeup event, SPK ACK, timeout event, etc.
 * @param[in] payload     Frame payload (excluding header)
 * @param[in] payload_len Payload length
 */
static void handle_voice_cmd(uint8_t *payload, uint16_t payload_len)
{
    uint8_t sub = payload[0];
    uint8_t evt;

    switch (sub) {
        case SEND_MIC_DATA_CMD:
            gx8006_mic_parse(payload, payload_len);
            break;
        case GET_MCU_VERSION_CMD:
            if (g_tx_resp_arg && payload_len > 4)
                memcpy(g_tx_resp_arg, payload + 4, payload_len - 4);
            liot_rtos_semaphore_release(g_tx_ack_sem);
            { uint8_t ack[2] = {0x01, 0x00}; gx8006_frame_send(ack, 2, 0, NULL); }
            break;
        case SEND_OFFLINE_VOICE_AWAKE_CMD:
            evt = GX_EVT_AWAKEN;
            liot_rtos_queue_release(g_evt_q, sizeof(uint8_t), &evt, LIOT_NO_WAIT);
            break;
        case RECV_SPK_DATA_CMD:
            g_tx_ack_result = (payload_len > 1) ? payload[1] : 0x00;
            liot_rtos_semaphore_release(g_tx_ack_sem);
            break;
        case SEND_OFFLINE_VOICE_TIMEOUT_CMD:
            if (gx8006_get_chat_mode() == GX_Q_AND_A_CHAT_MODE ||
                gx8006_get_chat_mode() == GX_NATURAL_CHAT_MODE) {
                evt = GX_EVT_AWAKEN_TIMEOUT;
                liot_rtos_queue_release(g_evt_q, sizeof(uint8_t), &evt, LIOT_NO_WAIT);
            }
            break;
        case MCU_EVENT_CMD:
            break;
        default:
            break;
    }
}

/**
 * @brief RX task function
 * @details Loops taking complete frames from RX queue, parses command word
 *          and dispatches to handler. Only processes VOICE_CMD_WORD frames.
 */
static void rx_task_fn(void *arg)
{
    (void)arg;
    gx8006_rx_chunk_t *chunk = NULL;

    while (1) {
        liot_rtos_queue_wait(g_rx_q, (uint8_t *)&chunk, sizeof(gx8006_rx_chunk_t *), LIOT_WAIT_FOREVER);
        if (!chunk || chunk->len < PROTOCOL_HEAD_LEN + 1) {
            liot_rtos_free(chunk);
            chunk = NULL;
            continue;
        }

        uint8_t sum = 0;
        for (uint32_t j = 0; j < chunk->len - 1; j++)
            sum += chunk->data[j];
        if (sum != chunk->data[chunk->len - 1]) {
            GX_TRACE("checsum error expect=0x%02X, calc=0x%02X", sum, chunk->data[chunk->len - 1]);
            gx8006_notify_ack_fail();
            liot_rtos_free(chunk);
            chunk = NULL;
            continue;
        }

        uint8_t   cmd      = chunk->data[3];
        uint16_t  data_len = ((uint16_t)chunk->data[4] << 8) | chunk->data[5];
        uint8_t  *payload  = chunk->data + PROTOCOL_HEAD_LEN;

        if (cmd == VOICE_CMD_WORD)
            handle_voice_cmd(payload, data_len);
        else
            GX_TRACE("RX unknown cmd=0x%02X len=%u", cmd, data_len);

        liot_rtos_free(chunk);
        chunk = NULL;
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
        gx8006_hw_uart_send(txbuf, PROTOCOL_HEAD_LEN + req.data_len + 1);

        liot_rtos_free(req.data);
        if (req.sync) {
            g_tx_ack_result = 0;
            liot_rtos_semaphore_wait(g_tx_ack_sem, GX8006_CMD_TIMEOUT);
            liot_rtos_semaphore_release(g_tx_done_sem);
        }
    }
}

/* ========== Event Task ========== */

/**
 * @brief Event dispatch task function
 * @details Loops taking event types from event queue and invokes user callback.
 *          Separate task avoids running time-consuming callbacks in RX task context.
 */
static void evt_task_fn(void *arg)
{
    (void)arg;
    uint8_t evt = 0;

    while (1) {
        liot_rtos_queue_wait(g_evt_q, &evt, sizeof(uint8_t), LIOT_WAIT_FOREVER);
        if (g_evt_cb)
            g_evt_cb((gx8006_evt_e)evt, NULL, 0);
    }
}

/* ========== Init / Deinit ========== */

/**
 * @brief Initialize protocol layer
 * @details Creates semaphores (TX done, TX ACK) and three message queues (RX/TX/EVT),
 *          starts RX, TX, and EVT tasks.
 * @param[in] evt_cb User event callback, may be NULL
 */
void gx8006_protocol_init(gx8006_evt_cb_t evt_cb)
{
    g_evt_cb = evt_cb;
    liot_rtos_semaphore_create(&g_tx_done_sem, 0);
    liot_rtos_semaphore_create(&g_tx_ack_sem, 0);
    liot_rtos_queue_create(&g_rx_q, sizeof(gx8006_rx_chunk_t *), GX8006_QUEUE_DEPTH);
    liot_rtos_queue_create(&g_tx_q, sizeof(gx8006_tx_req_t), GX8006_QUEUE_DEPTH);
    liot_rtos_queue_create(&g_evt_q, sizeof(uint8_t), GX8006_QUEUE_DEPTH);

    liot_rtos_task_create(&g_rx_task, GX8006_TASK_STACK_SIZE, GX8006_TASK_PRIORITY,
                          "gx_rx", rx_task_fn, NULL);
    liot_rtos_task_create(&g_tx_task, GX8006_TASK_STACK_SIZE, GX8006_TASK_PRIORITY,
                          "gx_tx", tx_task_fn, NULL);
    liot_rtos_task_create(&g_evt_task, GX8006_TASK_STACK_SIZE, GX8006_TASK_PRIORITY,
                          "gx_evt", evt_task_fn, NULL);
}

/**
 * @brief Release protocol layer resources
 * @details Deletes RX/TX/EVT tasks, releases semaphores and message queues
 */
void gx8006_protocol_deinit(void)
{
    if (g_rx_task)     liot_rtos_task_delete(g_rx_task);
    if (g_tx_task)     liot_rtos_task_delete(g_tx_task);
    if (g_evt_task)    liot_rtos_task_delete(g_evt_task);
    if (g_tx_done_sem) liot_rtos_semaphore_delete(g_tx_done_sem);
    if (g_tx_ack_sem)  liot_rtos_semaphore_delete(g_tx_ack_sem);
    if (g_rx_q)        liot_rtos_queue_delete(g_rx_q);
    if (g_tx_q)        liot_rtos_queue_delete(g_tx_q);
    if (g_evt_q)       liot_rtos_queue_delete(g_evt_q);
}
