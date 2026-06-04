/**
 * @file gx8006_mic.c
 * @brief GX8006 MIC/VAD control command implementation
 * @details Provides MIC on/off, VAD parameter settings, and wakeup timeout commands.
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

/* ========== Private api ========== */
extern uint8_t gx8006_frame_send(uint8_t *data, uint32_t data_len, uint8_t sync, void *arg);

/* ========== MCU version ========== */

/**
 * @brief Get module MCU version
 * @details Sends synchronous command, blocks until module returns version string.
 *          If timeout occurs, version is filled with "ERROR"
 */
void gx8006_get_mcu_version(char *version)
{
    uint8_t data[1] = {GET_MCU_VERSION_CMD};
    memset(version, 0, 16);
    gx8006_frame_send(data, 1, 1, version);
    if (version[0] == 0)
        sprintf(version, "ERROR");
}

/* ========== MIC / VAD commands ========== */

/**
 * @brief Open MIC channel
 */
void gx8006_mic_open(void)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, MIC_SWITCH_CTRL, 0x01};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Close MIC channel
 */
void gx8006_mic_close(void)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, MIC_SWITCH_CTRL, 0x00};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Set VAD wakeup enable
 * @param[in] enable 1 = enable, 0 = disable
 */
void gx8006_set_vad_awaken_enable(uint8_t enable)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, WAKEUP_CTRL, enable ? 1 : 0};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Set VAD timeout
 * @param[in] time Timeout in seconds
 */
void gx8006_set_vad_timeout_time(uint8_t time)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, VAD_PARAM_SET, time};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Set wakeup wait timeout
 * @param[in] time Timeout in seconds
 */
void gx8006_set_awaken_timeout_time(uint8_t time)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, AWAKEN_TIMEOUT_SET, time};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Set VAD sensitivity
 * @param[in] active  Active mode sensitivity
 * @param[in] passive Passive mode sensitivity
 */
void gx8006_set_vad_sensitivity(uint8_t active, uint8_t passive)
{
    uint8_t cmd[4] = {SET_OFFLINE_VOICE_CMD, 0x09, active, passive};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Set VAD noise reduction level
 * @param[in] nr Noise reduction level
 */
void gx8006_set_vad_noise_reduction(uint8_t nr)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, 0x0A, nr};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Set ASR gain
 * @param[in] gain Gain value
 */
void gx8006_set_asr_gain(uint8_t gain)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, 0x0B, gain};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Reset VAD state machine
 */
void gx8006_set_vad_reset(void)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, 0x0C, 0x01};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}

/**
 * @brief Notify module that VAD has timed out
 * @details In non-natural chat mode, notifies module to enter timeout state after VAD ends
 */
void gx8006_set_vad_is_timeout(void)
{
    uint8_t cmd[3] = {SET_OFFLINE_VOICE_CMD, MODULE_TIMEOUT_SET, 0x01};
    gx8006_frame_send(cmd, sizeof(cmd), 0, NULL);
}
