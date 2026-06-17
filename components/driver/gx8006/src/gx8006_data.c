/**
 * @file gx8006_data.c
 * @brief GX8006 SPK data transfer implementation
 * @details Handles SPK audio data framing with start/data/stop three-phase control.
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#include <string.h>
#include <stdio.h>

#include "liot_os.h"
#include "liot_uart2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "gx8006.h"

#define GX_TRACE(fmt, ...) liot_trace("[GX8006] " fmt, ##__VA_ARGS__)

/* ========== Private api ========== */
extern uint8_t gx8006_frame_send(uint8_t *data, uint32_t data_len, uint8_t sync, void *arg);
extern void gx8006_int_to_big_endian(int32_t value, uint8_t *array);

#ifndef GX8006_SPK_CMD_RETRY
#define GX8006_SPK_CMD_RETRY                10
#endif

/* ========== SPK status ========== */
static uint32_t s_spk_count  = 0;
static uint8_t  g_spk_status = GX_SPK_IDLE;

/**
 * @brief Set SPK playback status
 * @param[in] s Target status
 */
void gx8006_spk_set_status(gx8006_spk_status_e s)
{
    g_spk_status = s;
}

/**
 * @brief Get SPK playback status
 * @return Current playback status enum value
 */
gx8006_spk_status_e gx8006_spk_get_status(void)
{
    return (gx8006_spk_status_e)g_spk_status;
}

/* ========== SPK data write ========== */
static int gx8006_data_write_spk_stop(void);

/**
 * @brief Send SPK playback start command
 * @details Sends stop first if currently playing. Sends start command and waits for ACK,
 *          retries up to GX8006_SPK_CMD_RETRY times on failure.
 * @return 0 on success, -1 if retry limit exceeded
 */
static int gx8006_data_write_spk_start(void)
{
    uint8_t cmd[6] = {RECV_SPK_DATA_CMD, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t retry = 0;
    uint8_t ack;

    if (gx8006_spk_get_status() == GX_SPK_PLAYING)
        gx8006_data_write_spk_stop();

    gx8006_spk_set_status(GX_SPK_START);
    s_spk_count = 0;

    do {
        ack = gx8006_frame_send(cmd, sizeof(cmd), 1, NULL);
        retry++;
    } while (ack && retry <= GX8006_SPK_CMD_RETRY);

    if (retry > GX8006_SPK_CMD_RETRY) {
        GX_TRACE("SPK start FAILED after %d retries, ack=0x%02X", retry, ack);
        return -1;
    }

    GX_TRACE("SPK play start");
    s_spk_count++;
    return 0;
}

/**
 * @brief Send SPK playback stop command
 * @details Carries the sent frame count, waits for ACK. Retries up to GX8006_SPK_CMD_RETRY times.
 * @return 0 on success, -1 if retry limit exceeded
 */
static int gx8006_data_write_spk_stop(void)
{
    uint8_t cmd[6] = {RECV_SPK_DATA_CMD, 0x02, 0x00, 0x00, 0x00, 0x00};
    uint8_t retry = 0;
    uint8_t ack;

    gx8006_spk_set_status(GX_SPK_STOP);
    gx8006_int_to_big_endian(s_spk_count, &cmd[2]);

    do {
        ack = gx8006_frame_send(cmd, sizeof(cmd), 1, NULL);
        retry++;
    } while (ack && retry <= GX8006_SPK_CMD_RETRY);

    gx8006_spk_set_status(GX_SPK_IDLE);
    GX_TRACE("SPK play stop, frames=%u retry=%u", s_spk_count, retry);
    s_spk_count = 0;
    if (retry > GX8006_SPK_CMD_RETRY) {
        GX_TRACE("SPK stop FAILED after %d retries, ack=0x%02X", retry, ack);
        return -1;
    }
    return 0;
}

/**
 * @brief Send one frame of SPK audio data
 * @details Dynamically allocates buffer to concatenate command header and audio data,
 *          sends synchronously and waits for ACK. Exits early if playback is stopped externally.
 * @param[in] buf Audio frame data
 * @param[in] len Data length
 * @return 0 on success, -1 on allocation failure or retry limit exceeded
 */
static int gx8006_data_write_spk(uint8_t *buf, uint32_t len)
{
    uint8_t cmd[6] = {RECV_SPK_DATA_CMD, 0x01, 0x00, 0x00, 0x00, 0x00};
    uint8_t retry = 0;
    uint8_t ack;

    uint8_t *pkt = liot_rtos_malloc(len + 6);
    if (!pkt)
        return -1;

    if (gx8006_spk_get_status() == GX_SPK_STOP) {
        liot_rtos_free(pkt);
        return 0;
    }

    gx8006_spk_set_status(GX_SPK_PLAYING);
    gx8006_int_to_big_endian(s_spk_count, &cmd[2]);
    memcpy(pkt, cmd, 6);
    memcpy(pkt + 6, buf, len);

    do {
        ack = gx8006_frame_send(pkt, len + 6, 1, NULL);
        if (ack) {
            liot_rtos_task_sleep_ms(10);
            retry++;
        }
    } while (ack && gx8006_spk_get_status() == GX_SPK_PLAYING && retry <= GX8006_SPK_CMD_RETRY);

    liot_rtos_free(pkt);
    if (retry > GX8006_SPK_CMD_RETRY)
        return -1;

    s_spk_count++;
    return 0;
}

/* ========== SPK play sound ========== */

/**
 * @brief Play OPUS audio data
 * @details Blocking playback: sends start command, then loops sending data from offset
 *          44+80 bytes in frame-sized chunks, finally sends stop command.
 *          Offset skips file header (44B) and first frame (80B).
 *          Terminates early if status is changed externally to non-playing state.
 * @param[in] txbuf Audio data buffer pointer
 * @param[in] txlen Total data length in bytes
 */
void gx8006_spk_play_sound(const uint8_t *txbuf, uint32_t txlen)
{
    if (!txbuf || txlen == 0)
        return;

    gx8006_data_write_spk_start();

    uint32_t offset = 44 + GX8006_OPUS_FRAME_SIZE;
    uint32_t frame_count = 0;
    while (offset < txlen &&
           (gx8006_spk_get_status() == GX_SPK_START ||
            gx8006_spk_get_status() == GX_SPK_PLAYING)) {
        uint32_t chunk = (txlen - offset > GX8006_OPUS_FRAME_SIZE)
                         ? GX8006_OPUS_FRAME_SIZE : (txlen - offset);
        gx8006_data_write_spk((uint8_t *)&txbuf[offset], chunk);
        offset += GX8006_OPUS_FRAME_SIZE;
        frame_count++;
    }

    if (gx8006_spk_get_status() == GX_SPK_PLAYING)
        gx8006_data_write_spk_stop();
}

/* ========== Stream playback API (for loopback) ========== */

int gx8006_spk_stream_start(void)
{
    return gx8006_data_write_spk_start();
}

int gx8006_spk_stream_write(const uint8_t *buf, uint32_t len)
{
    return gx8006_data_write_spk((uint8_t *)buf, len);
}

int gx8006_spk_stream_stop(void)
{
    return gx8006_data_write_spk_stop();
}
