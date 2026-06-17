/**
 * @file gx8006.h
 * @brief GX8006 voice module driver interface
 * @details Provides initialization, MIC/VAD control, SPK playback,
 *          and chat mode switching for the GX8006 voice module.
 *          Communicates via UART and supports OPUS encoded audio streams.
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#ifndef _GX8006_H_
#define _GX8006_H_

#include <stdint.h>
#include <stddef.h>


/* ========== Protocol constants ========== */
#define GX8006_OPUS_MAX_FRAME_LENGTH        1152  /* Max bytes per OPUS frame (SPK single transfer max) */
#define GX8006_OPUS_FRAME_SIZE              80    /* OPUS frame data length */

#define FRAME_FIRST             0x55  /* Frame header first byte */
#define FRAME_SECOND            0xAA  /* Frame header second byte */

#define MCU_TX_VER              0x03  /* MCU TX protocol version */
#define MCU_RX_VER              0x00  /* MCU RX protocol version */

#define VOICE_CMD_WORD          0x92  /* Voice command word */
#define PROTOCOL_HEAD_LEN       6     /* Protocol frame header length */

/** @brief Communication command IDs */
typedef enum {
    GET_MCU_VERSION_CMD             = 0x01,
    SET_MIC_PARAM_CMD               = 0x02,
    SET_SPK_PARAM_CMD               = 0x03,
    SET_AWAKE_GPIO_CMD              = 0x04,
    SEND_MIC_DATA_CMD               = 0x05,
    RECV_SPK_DATA_CMD               = 0x06,
    SET_OFFLINE_VOICE_CMD           = 0x07,
    SEND_OFFLINE_VOICE_AWAKE_CMD    = 0x08,
    SEND_OFFLINE_VOICE_TIMEOUT_CMD  = 0x09,
    GET_OFFLINE_VOICE_STATUS_CMD    = 0x0A,
    FACTORY_TEST_CMD                = 0x0B,
    MCU_EVENT_CMD                   = 0x12,
} SMARTBOT_CMD;

/** @brief Configuration sub-command types */
typedef enum {
    TRANSPORT_CHANNEL_TYPE_SET = 0x00,
    MIC_SWITCH_CTRL            = 0x01,
    SPK_SWITCH_CTRL            = 0x02,
    WAKEUP_CTRL                = 0x03,
    VAD_PARAM_SET              = 0x04,
    MODULE_AWAKE_SET           = 0x05,
    MODULE_TIMEOUT_SET         = 0x06,
    AWAKEN_TIMEOUT_SET         = 0x07,
    PICKUP_TIMEOUT_SET         = 0x08,
} CONFIG_TYPE;

/** @brief Audio encoding format */
typedef enum {
    FORMAT_PCM   = 0x00,
    FORMAT_SPEEX = 0x01,
    FORMAT_OPUS  = 0x02,
    FORMAT_MP3   = 0x03,
} FORMAT_TYPE;

/* ========== Enums ========== */

/** @brief Chat mode */
typedef enum {
    GX_ONE_BY_ONE_QA_CHAT_MODE,  ///< One-shot Q&A mode
    GX_NATURAL_CHAT_MODE,        ///< Natural conversation mode
    GX_Q_AND_A_CHAT_MODE,        ///< Continuous Q&A mode
} gx8006_chat_mode_e;

/** @brief SPK playback status */
typedef enum {
    GX_SPK_IDLE,     ///< Idle
    GX_SPK_START,    ///< Playback starting
    GX_SPK_PLAYING,  ///< Playing
    GX_SPK_STOP,     ///< Stopped
} gx8006_spk_status_e;

/* ========== Events ========== */

/** @brief Event types */
typedef enum {
    GX_EVT_MIC_VAD_START = 0,  ///< VAD detected voice start
    GX_EVT_MIC_VAD_DATA,       ///< VAD voice data
    GX_EVT_MIC_VAD_END,        ///< VAD detected voice end
    GX_EVT_AWAKEN,             ///< Wakeup event
    GX_EVT_AWAKEN_TIMEOUT,     ///< Wakeup timeout
} gx8006_evt_e;

/**
 * @brief Event callback function type
 * @param[in] evt  Event type
 * @param[in] data Event data (may be NULL)
 * @param[in] len  Data length
 */
typedef void (*gx8006_evt_cb_t)(gx8006_evt_e evt, const uint8_t *data, uint32_t len);

/* ========== Init config ========== */

/** @brief Initialization configuration */
typedef struct {
    int8_t              rst_gpio;          ///< Reset GPIO (-1 = unused)
    int8_t              boot_gpio;         ///< BOOT GPIO (-1 = unused)
    int8_t              pa_mode_gpio;      ///< PA mode GPIO (-1 = unused)
    int8_t              uart_port;         ///< UART port number (-1 = unused)
    uint32_t            uart_baudrate;     ///< UART baud rate
    gx8006_chat_mode_e  default_chat_mode; ///< Default chat mode
    uint8_t             default_volume;    ///< Default volume (0~100)
    uint8_t             vad_timeout_time;  ///< VAD timeout (seconds)
    gx8006_evt_cb_t     evt_cb;           ///< Event callback function
} gx8006_config_t;

/* ========== API ========== */

/* ---------- Init & Deinit ---------- */

/**
 * @brief Initialize GX8006 module
 * @details Attempts to load config from NV first; if NV is invalid, uses the
 *          provided config and writes it to NV. Then initializes GPIO, UART,
 *          protocol layer, configures SPK parameters and chat mode.
 * @param[in] cfg Initialization config pointer, must not be NULL
 */
void gx8006_init(const gx8006_config_t *cfg);

/**
 * @brief Deinitialize GX8006 module
 * @details Stops protocol layer tasks, releases UART and GPIO resources
 */
void gx8006_deinit(void);

/* ---------- Hardware control ---------- */

/**
 * @brief Hardware reset the module
 * @details Pulls RST pin low for 100ms then high, waits for module restart
 */
void gx8006_hw_reset(void);

/**
 * @brief Enter BOOT mode
 * @details Pulls BOOT pin low, then resets the module for firmware upgrade
 */
void gx8006_hw_inboot(void);

/* ---------- Chat mode ---------- */

/**
 * @brief Set chat mode
 * @details Persists to NV and adjusts wakeup timeout accordingly:
 *          natural/continuous Q&A = 30s, one-shot Q&A = 5s
 * @param[in] mode Chat mode enum value
 */
void gx8006_chat_mode_set(gx8006_chat_mode_e mode);

/**
 * @brief Get current chat mode
 * @return Current chat mode enum value
 */
gx8006_chat_mode_e gx8006_get_chat_mode(void);

/* ---------- MIC / VAD control ---------- */

/**
 * @brief Open MIC
 * @details Enables the module's offline voice recognition MIC channel
 */
void gx8006_mic_open(void);

/**
 * @brief Close MIC
 * @details Disables the module's offline voice recognition MIC channel
 */
void gx8006_mic_close(void);

/**
 * @brief Set VAD wakeup enable
 * @param[in] enable 1 = enable, 0 = disable
 */
void gx8006_set_vad_awaken_enable(uint8_t enable);

/**
 * @brief Set VAD timeout
 * @details Timeout after no voice input detected by VAD
 * @param[in] time Timeout in seconds
 */
void gx8006_set_vad_timeout_time(uint8_t time);

/**
 * @brief Set wakeup wait timeout
 * @details Maximum time to wait for user speech after module wakeup
 * @param[in] time Timeout in seconds
 */
void gx8006_set_awaken_timeout_time(uint8_t time);

/**
 * @brief Set VAD sensitivity (0-100, higher = less sensitive)
 * @param[in] val Sensitivity value (default 45)
 */
void gx8006_set_vad_sensitivity(uint8_t val);

/**
 * @brief Set VAD noise reduction level
 * @param[in] nr Noise reduction level
 */
void gx8006_set_vad_noise_reduction(uint8_t nr);

/**
 * @brief Set ASR gain
 * @param[in] gain Gain value
 */
void gx8006_set_asr_gain(uint8_t gain);

/**
 * @brief Reset VAD state
 * @details Clears the module's internal VAD state machine
 */
void gx8006_set_vad_reset(void);

/**
 * @brief Notify module that VAD has timed out
 * @details In non-natural chat mode, notifies module to enter timeout state after VAD ends
 */
void gx8006_set_vad_is_timeout(void);

/* ---------- SPK control ---------- */

/**
 * @brief Get module MCU version
 * @details Synchronous command, blocks until module returns version string
 * @param[out] version Output buffer (at least 16 bytes)
 */
void gx8006_get_mcu_version(char *version);

/**
 * @brief Get current SPK playback status
 * @return Current SPK status (idle, start, playing, stop)
 */
gx8006_spk_status_e gx8006_spk_get_status(void);

/**
 * @brief Set SPK volume
 * @details Takes effect immediately, updates internal config (not persisted to NV)
 * @param[in] volume Volume value (0~100, clamped to 100 if exceeded)
 */
void gx8006_spk_set_volume(uint32_t volume);

/**
 * @brief Get current SPK volume
 * @return Current volume (0~100)
 */
uint32_t gx8006_spk_get_volume(void);

/**
 * @brief Play OPUS audio data
 * @details Blocking playback with automatic start/data/stop phases.
 *          Data starts at offset 44+80 bytes (skips file header and first frame),
 *          sent in GX8006_OPUS_FRAME_SIZE chunks.
 * @param[in] txbuf Audio data buffer pointer
 * @param[in] txlen Total data length in bytes
 */
void gx8006_spk_play_sound(const uint8_t *txbuf, uint32_t txlen);

/**
 * @brief Start SPK streaming playback
 * @return 0 on success, -1 on failure
 */
int gx8006_spk_stream_start(void);

/**
 * @brief Write audio frame data during streaming playback
 * @param[in] buf Audio frame data pointer
 * @param[in] len Data length in bytes
 * @return 0 on success, -1 on failure
 */
int gx8006_spk_stream_write(const uint8_t *buf, uint32_t len);

/**
 * @brief Stop SPK streaming playback
 * @return 0 on success, -1 on failure
 */
int gx8006_spk_stream_stop(void);

#endif
