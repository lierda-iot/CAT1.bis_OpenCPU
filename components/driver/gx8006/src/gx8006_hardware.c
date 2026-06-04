/**
 * @file gx8006_hardware.c
 * @brief GX8006 hardware layer implementation
 * @details Handles UART communication, GPIO control, NV parameter persistence,
 *          and frame parsing.
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

/* ========== Private api ========== */
extern void gx8006_frame_recv(uint8_t *data, uint32_t data_len);
extern uint8_t gx8006_frame_send(uint8_t *data, uint32_t data_len, uint8_t sync, void *arg);
extern void gx8006_protocol_init(gx8006_evt_cb_t evt_cb);
extern void gx8006_protocol_deinit(void);

/* ========== Runtime config ========== */
static gx8006_config_t g_cfg;

/* ========== Endian utility ========== */

/**
 * @brief Convert big-endian to little-endian (32-bit)
 */
uint32_t gx8006_big_to_little_endian(uint32_t *data)
{
    uint32_t v = *data;
    return ((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8) |
           ((v & 0x0000FF00) << 8)  | ((v & 0x000000FF) << 24);
}

/**
 * @brief Convert integer to big-endian byte array
 */
void gx8006_int_to_big_endian(int32_t value, uint8_t *array)
{
    array[0] = (value >> 24) & 0xFF;
    array[1] = (value >> 16) & 0xFF;
    array[2] = (value >> 8)  & 0xFF;
    array[3] = value & 0xFF;
}

/* ========== Chat mode ========== */

/**
 * @brief Set chat mode and persist to NV
 * @details Also adjusts wakeup timeout: natural/continuous Q&A = 30s, one-shot = 5s
 */
void gx8006_chat_mode_set(gx8006_chat_mode_e mode)
{
    g_cfg.default_chat_mode = mode;

    uint8_t timeout = (mode == GX_NATURAL_CHAT_MODE || mode == GX_Q_AND_A_CHAT_MODE) ? 30 : 5;
    gx8006_set_awaken_timeout_time(timeout);
}

/**
 * @brief Get current chat mode
 */
gx8006_chat_mode_e gx8006_get_chat_mode(void)
{
    return g_cfg.default_chat_mode;
}

/* ========== SPK config ========== */


/**
 * @brief Set SPK volume and update internal config
 * @param[in] vol Volume value (0~100, clamped to 100)
 */
void gx8006_spk_set_volume(uint32_t vol)
{
    if (vol > 100) vol = 100;

    uint8_t cfg[12] = {0};
    cfg[0] = SET_SPK_PARAM_CMD;
    gx8006_int_to_big_endian(16000, &cfg[1]);
    cfg[5] = 16;
    cfg[6] = (uint8_t)vol;
    cfg[7] = FORMAT_OPUS;
    gx8006_int_to_big_endian(GX8006_OPUS_MAX_FRAME_LENGTH, &cfg[8]);
    gx8006_frame_send(cfg, sizeof(cfg), 0, NULL);
    g_cfg.default_volume = vol;
}

/**
 * @brief Get current SPK volume
 */
uint32_t gx8006_spk_get_volume(void)
{
    return g_cfg.default_volume;
}

/* ========== UART frame parsing ========== */
#define RX_PARSE_BUF_SIZE  (GX8006_OPUS_MAX_FRAME_LENGTH + PROTOCOL_HEAD_LEN + 1)

static uint8_t  s_parse_buf[RX_PARSE_BUF_SIZE * 2];
static uint32_t s_parse_len = 0;

/**
 * @brief UART RX callback: frame assembly, checksum validation, dispatch
 * @details Appends received data to parse buffer, searches for frame header (0x55AA),
 *          validates checksum and dispatches complete frames via gx8006_frame_recv
 */
static void gx8006_uart_rx_cb(liot_uart_e port, char *data, uint32_t size, void *argc)
{
    (void)port;
    (void)argc;

    if (s_parse_len + size > sizeof(s_parse_buf)) {
        GX_TRACE("UART RX: buffer overflow, reset parse_len=%u + size=%u > buf=%u",
                 s_parse_len, size, (uint32_t)sizeof(s_parse_buf));
        s_parse_len = 0;
    }

    memcpy(s_parse_buf + s_parse_len, data, size);
    s_parse_len += size;

    while (s_parse_len >= PROTOCOL_HEAD_LEN + 1) {
        uint32_t i = 0;
        while (i + 1 < s_parse_len) {
            if (s_parse_buf[i] == FRAME_FIRST && s_parse_buf[i + 1] == FRAME_SECOND)
                break;
            i++;
        }
        if (i > 0) {
            GX_TRACE("UART RX: skipped %u bytes before header", i);
            s_parse_len -= i;
            memmove(s_parse_buf, s_parse_buf + i, s_parse_len);
        }
        if (s_parse_len < PROTOCOL_HEAD_LEN + 1)
            break;

        uint16_t dlen = ((uint16_t)s_parse_buf[4] << 8) | s_parse_buf[5];
        uint32_t frame_total = PROTOCOL_HEAD_LEN + dlen + 1;
        if (s_parse_len < frame_total) {
            break;
        }

        uint8_t sum = 0;
        for (uint32_t j = 0; j < frame_total - 1; j++)
            sum += s_parse_buf[j];

        if (sum == s_parse_buf[frame_total - 1]) {
            gx8006_frame_recv(s_parse_buf, frame_total);
        } else {
            GX_TRACE("UART RX: checksum FAIL expect=0x%02X got=0x%02X, hex[0..7]: %02X %02X %02X %02X %02X %02X %02X %02X",
                     sum, s_parse_buf[frame_total - 1],
                     s_parse_buf[0], s_parse_buf[1], s_parse_buf[2], s_parse_buf[3],
                     frame_total > 4 ? s_parse_buf[4] : 0, frame_total > 5 ? s_parse_buf[5] : 0,
                     frame_total > 6 ? s_parse_buf[6] : 0, frame_total > 7 ? s_parse_buf[7] : 0);
        }

        s_parse_len -= frame_total;
        memmove(s_parse_buf, s_parse_buf + frame_total, s_parse_len);
    }
}

/**
 * @brief Send data to module via UART
 */
void gx8006_hw_uart_send(uint8_t *data, uint16_t len)
{
    if (g_cfg.uart_port < 0) {
        GX_TRACE("UART TX: port not configured!");
        return;
    }
    Liot_UartSend((liot_uart_e)g_cfg.uart_port, data, len);
}

/* ========== GPIO control ========== */

/**
 * @brief Hardware reset the module
 * @details Pulls RST low for 100ms then high, waits for module restart
 */
void gx8006_hw_reset(void)
{
    if (g_cfg.rst_gpio < 0)
        return;
    Liot_GpioSetLevel((liot_gpio_e)g_cfg.rst_gpio, L_IO_LOW);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel((liot_gpio_e)g_cfg.rst_gpio, L_IO_HIGH);
    liot_rtos_task_sleep_ms(100);
}

/**
 * @brief Enter BOOT mode
 * @details Pulls BOOT low, resets module, then releases BOOT for firmware upgrade
 */
void gx8006_hw_inboot(void)
{
    if (g_cfg.boot_gpio < 0 || g_cfg.rst_gpio < 0)
        return;
    Liot_GpioSetLevel((liot_gpio_e)g_cfg.boot_gpio, L_IO_LOW);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel((liot_gpio_e)g_cfg.rst_gpio, L_IO_LOW);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel((liot_gpio_e)g_cfg.rst_gpio, L_IO_HIGH);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel((liot_gpio_e)g_cfg.boot_gpio, L_IO_HIGH);
}

/* ========== Hardware init / deinit ========== */

/**
 * @brief Initialize hardware peripherals
 * @details Initializes RST/BOOT/PA_MODE GPIOs and UART port based on config.
 *          GPIO directions: RST=output high, BOOT=output high, PA_MODE=output low.
 *          UART configured as 8N1 no flow control, baud rate from g_cfg.
 */
static void gx8006_hw_init(void)
{
    if (g_cfg.uart_port >= 0) {
        Liot_UartConfig_t uart_cfg = {
            .baudrate   = g_cfg.uart_baudrate,
            .data_bit   = L_UART_DATA_8,
            .stop_bit   = L_UART_STOP_1,
            .parity_bit = L_UART_PARITY_NONE,
            .flow_ctrl  = L_UART_FC_NONE,
            .tx_way     = L_UART_TX_DRIVER,
        };
        Liot_UartInit((liot_uart_e)g_cfg.uart_port, &uart_cfg, gx8006_uart_rx_cb, NULL);
    }
    if (g_cfg.rst_gpio >= 0)
        Liot_GpioInit((liot_gpio_e)g_cfg.rst_gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (g_cfg.boot_gpio >= 0)
        Liot_GpioInit((liot_gpio_e)g_cfg.boot_gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (g_cfg.pa_mode_gpio >= 0)
        Liot_GpioInit((liot_gpio_e)g_cfg.pa_mode_gpio, L_IO_INPUT, L_IO_LOW, NULL);
}

/**
 * @brief Release hardware peripheral resources
 * @details Closes UART port, switches RST and BOOT GPIOs to input mode to reduce power
 */
static void gx8006_hw_deinit(void)
{
    if (g_cfg.uart_port >= 0)
        Liot_UartDeinit((liot_uart_e)g_cfg.uart_port);
    if (g_cfg.rst_gpio >= 0)
        Liot_GpioInit((liot_gpio_e)g_cfg.rst_gpio, L_IO_INPUT, L_IO_HIGH, NULL);
    if (g_cfg.boot_gpio >= 0)
        Liot_GpioInit((liot_gpio_e)g_cfg.boot_gpio, L_IO_INPUT, L_IO_HIGH, NULL);
}

/* ========== Top-level init ========== */

/**
 * @brief Initialize GX8006 module
 * @details Loads saved config from NV Flash first; if NV is invalid, uses the provided
 *          cfg and persists it. After hardware (GPIO/UART) and protocol layer init,
 *          configures SPK parameters, chat mode timeout, VAD timeout, closes MIC
 *          and disables VAD wakeup, leaving the module in standby state.
 * @param[in] cfg Init config pointer, used as default if NV is invalid, must not be NULL
 */
void gx8006_init(const gx8006_config_t *cfg)
{
    if (!cfg)
        return;

    g_cfg = *cfg;

    gx8006_hw_init();
    gx8006_protocol_init(g_cfg.evt_cb);

    liot_rtos_task_sleep_ms(100);
    gx8006_spk_set_volume(g_cfg.default_volume);
    liot_rtos_task_sleep_ms(50);

    gx8006_chat_mode_e mode = g_cfg.default_chat_mode;
    uint8_t timeout = (mode == GX_NATURAL_CHAT_MODE || mode == GX_Q_AND_A_CHAT_MODE) ? 30 : 5;
    gx8006_set_awaken_timeout_time(timeout);

    liot_rtos_task_sleep_ms(50);
    gx8006_set_vad_timeout_time(g_cfg.vad_timeout_time);
}

/**
 * @brief Deinitialize GX8006 module
 * @details Stops protocol layer tasks and releases UART and GPIO resources
 */
void gx8006_deinit(void)
{
    gx8006_protocol_deinit();
    gx8006_hw_deinit();
}
