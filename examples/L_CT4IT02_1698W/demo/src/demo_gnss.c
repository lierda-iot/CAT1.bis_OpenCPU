/**
 * @file demo_gnss.c
 * @brief CC1161W UART/NMEA validation for L_CT4IT02_1698W.
 */

#include <stdio.h>
#include <string.h>

#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_uart2.h"

#include "demo_gnss.h"

#define GNSS_UART_PORT          L_UART2
#define GNSS_UART_BAUDRATE      L_UART_BR_115200
#define GNSS_LINE_BUFFER_SIZE   256
#define GNSS_RAW_SAMPLE_SIZE    32
#define GNSS_RX_MODEM_PIN       55
#define GNSS_TX_MODEM_PIN       56
#define GNSS_POWER_STABLE_MS    500
#define GNSS_AID_CHUNK_SIZE     1024U
#define GNSS_AID_SEND_RETRIES   20U
#define GNSS_INVALID_LOG_MS     5000U
#define GNSS_SAT_LOG_MS         5000U

static char s_nmea_line[GNSS_LINE_BUFFER_SIZE];
static uint16_t s_nmea_line_len;
static volatile uint32_t s_nmea_line_count;
static volatile uint32_t s_rx_callback_count;
static volatile uint32_t s_rx_byte_count;
static uint8_t s_raw_sample[GNSS_RAW_SAMPLE_SIZE];
static volatile uint8_t s_raw_sample_len;
static volatile uint32_t s_fix_sequence;
static volatile int32_t s_latitude_e7;
static volatile int32_t s_longitude_e7;
static volatile bool s_fix_valid;
static volatile bool s_uart_ready;
static volatile bool s_agnss_ttff_pending;
static volatile uint32_t s_agnss_injected_tick;
static uint32_t s_last_invalid_log_tick;
static uint32_t s_last_sat_log_tick;
static uint32_t s_last_gsv_log_tick;

bool demo_gnss_is_ready(void)
{
    return s_uart_ready;
}

int demo_gnss_query_assistance(void)
{
    static const unsigned char command[] = "$AIDINFO\r\n";
    uint32_t sent;

    if (!s_uart_ready) return -1;
    sent = Liot_UartSend(GNSS_UART_PORT, (unsigned char *)command,
                         sizeof(command) - 1U);
    liot_trace("[gnss-cc1161w] AIDINFO query sent=%lu/%lu",
               (unsigned long)sent,
               (unsigned long)(sizeof(command) - 1U));
    return sent == (sizeof(command) - 1U) ? 0 : -2;
}

int demo_gnss_inject_assistance(const uint8_t *data, size_t size)
{
    size_t offset = 0;

    if (data == NULL || size == 0U) return -1;
    if (!s_uart_ready) return -2;
    while (offset < size) {
        size_t chunk_size = size - offset;
        size_t chunk_sent = 0;
        uint32_t retries = 0;

        if (chunk_size > GNSS_AID_CHUNK_SIZE) chunk_size = GNSS_AID_CHUNK_SIZE;
        while (chunk_sent < chunk_size && retries < GNSS_AID_SEND_RETRIES) {
            uint32_t sent = Liot_UartSend(
                GNSS_UART_PORT, (unsigned char *)(data + offset + chunk_sent),
                (unsigned int)(chunk_size - chunk_sent));
            if (sent == 0U) {
                retries++;
                liot_rtos_task_sleep_ms(10);
                continue;
            }
            chunk_sent += sent;
        }
        if (chunk_sent != chunk_size) {
            liot_trace("[gnss-cc1161w] AGNSS UART failed offset=%lu sent=%lu/%lu",
                       (unsigned long)offset, (unsigned long)chunk_sent,
                       (unsigned long)chunk_size);
            return -3;
        }
        offset += chunk_size;
        liot_trace("[gnss-cc1161w] AGNSS UART injected=%lu/%lu",
                   (unsigned long)offset, (unsigned long)size);
        liot_rtos_task_sleep_ms(10);
    }
    s_agnss_injected_tick = liot_rtos_get_system_tick();
    s_agnss_ttff_pending = true;
    liot_trace("[gnss-cc1161w] AGNSS injection complete tick=%lu, waiting for valid fix",
               (unsigned long)s_agnss_injected_tick);
    return 0;
}

static bool gnss_coordinate_to_e7(const char *value, char hemisphere,
                                  int32_t *coordinate_e7)
{
    const char *dot;
    uint32_t whole = 0;
    uint32_t fraction = 0;
    uint32_t fraction_scale = 1;
    uint32_t degrees;
    uint32_t minutes;
    uint64_t minute_e7;
    int32_t result;
    int digits = 0;

    if (value == NULL || coordinate_e7 == NULL || value[0] == '\0') return false;
    dot = strchr(value, '.');
    while (*value != '\0' && *value != '.') {
        if (*value < '0' || *value > '9') return false;
        whole = whole * 10U + (uint32_t)(*value - '0');
        value++;
    }
    if (dot != NULL) {
        value = dot + 1;
        while (*value >= '0' && *value <= '9' && digits < 7) {
            fraction = fraction * 10U + (uint32_t)(*value - '0');
            fraction_scale *= 10U;
            value++;
            digits++;
        }
    }
    degrees = whole / 100U;
    minutes = whole % 100U;
    if (minutes >= 60U) return false;
    minute_e7 = (uint64_t)minutes * 10000000ULL;
    if (fraction_scale > 1U) {
        minute_e7 += ((uint64_t)fraction * 10000000ULL) / fraction_scale;
    }
    result = (int32_t)((uint64_t)degrees * 10000000ULL + minute_e7 / 60ULL);
    if (hemisphere == 'S' || hemisphere == 'W') result = -result;
    else if (hemisphere != 'N' && hemisphere != 'E') return false;
    *coordinate_e7 = result;
    return true;
}

static void gnss_publish_fix(const char *latitude, char ns,
                             const char *longitude, char ew)
{
    int32_t lat_e7;
    int32_t lon_e7;

    if (!gnss_coordinate_to_e7(latitude, ns, &lat_e7) ||
        !gnss_coordinate_to_e7(longitude, ew, &lon_e7)) return;
    s_fix_sequence++;
    s_latitude_e7 = lat_e7;
    s_longitude_e7 = lon_e7;
    s_fix_valid = true;
    s_fix_sequence++;
}

bool demo_gnss_get_location(int32_t *latitude_e7, int32_t *longitude_e7)
{
    uint32_t before;
    uint32_t after;
    int32_t latitude;
    int32_t longitude;
    bool valid;

    if (latitude_e7 == NULL || longitude_e7 == NULL) return false;
    do {
        before = s_fix_sequence;
        if ((before & 1U) != 0U) continue;
        latitude = s_latitude_e7;
        longitude = s_longitude_e7;
        valid = s_fix_valid;
        after = s_fix_sequence;
    } while (before != after || (after & 1U) != 0U);
    if (!valid) return false;
    *latitude_e7 = latitude;
    *longitude_e7 = longitude;
    return true;
}

static void gnss_log_position(char *line)
{
    char copy[GNSS_LINE_BUFFER_SIZE];
    char *field[20];
    char *cursor;
    uint32_t now;
    int count = 0;

    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    cursor = copy;
    field[count++] = cursor;
    while (*cursor != '\0' && count < (int)(sizeof(field) / sizeof(field[0]))) {
        if (*cursor == ',') {
            *cursor = '\0';
            field[count++] = cursor + 1;
        }
        cursor++;
    }

    now = liot_rtos_get_system_tick();

    if ((strcmp(field[0], "$GNGGA") == 0 ||
         strcmp(field[0], "$GPGGA") == 0 ||
         strcmp(field[0], "$BDGGA") == 0) && count > 9) {
        if (field[6][0] != '0' ||
            (uint32_t)(now - s_last_invalid_log_tick) >= GNSS_INVALID_LOG_MS) {
            liot_trace("[gnss-cc1161w] GGA utc=%s lat=%s%s lon=%s%s fix=%s sats=%s hdop=%s alt=%s",
                       field[1], field[2], field[3], field[4], field[5],
                       field[6], field[7], field[8], field[9]);
            if (field[6][0] == '0') s_last_invalid_log_tick = now;
        }
    } else if ((strcmp(field[0], "$GNRMC") == 0 ||
                strcmp(field[0], "$GPRMC") == 0 ||
                strcmp(field[0], "$BDRMC") == 0) && count > 9) {
        if (field[2][0] == 'A') {
            gnss_publish_fix(field[3], field[4][0], field[5], field[6][0]);
            if (s_agnss_ttff_pending) {
                liot_trace("[gnss-cc1161w] AGNSS FIRST FIX TTFF=%lu ms utc=%s lat=%s%s lon=%s%s",
                           (unsigned long)(now - s_agnss_injected_tick),
                           field[1], field[3], field[4], field[5], field[6]);
                s_agnss_ttff_pending = false;
            }
            liot_trace("[gnss-cc1161w] RMC utc=%s valid=%s lat=%s%s lon=%s%s speed_kn=%s date=%s",
                       field[1], field[2], field[3], field[4], field[5],
                       field[6], field[7], field[9]);
        }
    } else if ((strcmp(field[0], "$GNGSA") == 0 ||
                strcmp(field[0], "$GPGSA") == 0 ||
                strcmp(field[0], "$BDGSA") == 0) && count > 17) {
        if ((uint32_t)(now - s_last_sat_log_tick) >= GNSS_SAT_LOG_MS) {
            int used = 0;
            int i;
            for (i = 3; i <= 14 && i < count; i++) {
                if (field[i][0] != '\0') used++;
            }
            liot_trace("[gnss-cc1161w] GSA fix_type=%s used_sats=%d pdop=%s hdop=%s vdop=%s",
                       field[2], used, field[15], field[16], field[17]);
            s_last_sat_log_tick = now;
        }
    } else if ((strcmp(field[0], "$GNGSV") == 0 ||
                strcmp(field[0], "$GPGSV") == 0 ||
                strcmp(field[0], "$BDGSV") == 0) && count > 3 &&
               strcmp(field[2], "1") == 0 &&
               (uint32_t)(now - s_last_gsv_log_tick) >= GNSS_SAT_LOG_MS) {
        liot_trace("[gnss-cc1161w] GSV talker=%s visible_sats=%s messages=%s",
                   field[0], field[3], field[1]);
        s_last_gsv_log_tick = now;
    } else if (strcmp(field[0], "$AIDINFO") == 0) {
        liot_trace("[gnss-cc1161w] AIDINFO response=%s", line);
    }
}

static void gnss_handle_complete_line(void)
{
    if (s_nmea_line_len == 0) {
        return;
    }

    s_nmea_line[s_nmea_line_len] = '\0';
    if (s_nmea_line[0] != '$') {
        s_nmea_line_len = 0;
        return;
    }
    s_nmea_line_count++;
    gnss_log_position(s_nmea_line);
    s_nmea_line_len = 0;
}

static void gnss_uart_callback(liot_uart_e port,
                               char *data,
                               uint32_t size,
                               void *argc)
{
    uint32_t i;

    (void)argc;
    if (port != GNSS_UART_PORT || data == NULL) {
        return;
    }

    s_rx_callback_count++;
    s_rx_byte_count += size;

    for (i = 0; i < size; i++) {
        char ch = data[i];

        if (s_raw_sample_len < GNSS_RAW_SAMPLE_SIZE) {
            s_raw_sample[s_raw_sample_len++] = (uint8_t)ch;
        }

        if (ch == '\r' || ch == '\n') {
            gnss_handle_complete_line();
            continue;
        }

        if (ch == '$') {
            s_nmea_line_len = 0;
        }

        if (s_nmea_line_len < GNSS_LINE_BUFFER_SIZE - 1) {
            s_nmea_line[s_nmea_line_len++] = ch;
        } else {
            liot_trace("[gnss-cc1161w] NMEA line overflow, drop");
            s_nmea_line_len = 0;
        }
    }
}

static liot_uart_err_e gnss_uart_start(liot_uart_baudrate_e baudrate)
{
    Liot_UartConfig_t uart_config;

    memset(&uart_config, 0, sizeof(uart_config));
    uart_config.baudrate = baudrate;
    uart_config.data_bit = L_UART_DATA_8;
    uart_config.stop_bit = L_UART_STOP_1;
    uart_config.parity_bit = L_UART_PARITY_NONE;
    uart_config.flow_ctrl = L_UART_FC_NONE;
    uart_config.tx_way = L_UART_TX_OPAQ;

    s_nmea_line_len = 0;
    s_nmea_line_count = 0;
    s_rx_callback_count = 0;
    s_rx_byte_count = 0;
    s_raw_sample_len = 0;

    return Liot_UartInit(GNSS_UART_PORT, &uart_config,
                         gnss_uart_callback, NULL);
}

static void gnss_log_raw_sample(void)
{
    char hex[(GNSS_RAW_SAMPLE_SIZE * 3) + 1];
    uint8_t sample_len = s_raw_sample_len;
    uint8_t i;
    int offset = 0;

    if (sample_len > GNSS_RAW_SAMPLE_SIZE) {
        sample_len = GNSS_RAW_SAMPLE_SIZE;
    }
    for (i = 0; i < sample_len && offset < (int)sizeof(hex); i++) {
        offset += snprintf(hex + offset, sizeof(hex) - (size_t)offset,
                           "%02X ", s_raw_sample[i]);
    }
    hex[(offset < (int)sizeof(hex)) ? offset : (int)sizeof(hex) - 1] = '\0';
    liot_trace("[gnss-cc1161w] raw sample len=%u hex=%s", sample_len, hex);
}

void liot_gnss_demo_thread(void *argv)
{
    liot_uart_err_e ret;
    liot_gpioerr_e gpio_ret_rx;
    liot_gpioerr_e gpio_ret_tx;
    liot_gpioerr_e aon_ret;
    liot_gpioerr_e voltage_ret;
    liot_gpioerr_e power25_ret;
    liot_gpioerr_e power27_ret;

    (void)argv;
    s_nmea_line_len = 0;
    s_nmea_line_count = 0;

    aon_ret = Liot_AonPowerCtl(true);
    voltage_ret = Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    liot_trace("[gnss-cc1161w] AON ret=0x%x voltage 3.3V ret=0x%x",
               (unsigned int)aon_ret, (unsigned int)voltage_ret);

    liot_trace("[gnss-cc1161w] start UART2 115200 8N1");
    liot_trace("[gnss-cc1161w] RX=pin55(GPIO6/Func2)<-GNSS_TX");
    liot_trace("[gnss-cc1161w] TX=pin56(GPIO7/Func2)->GNSS_RX");

    gpio_ret_rx = Liot_SetPinFunc(GNSS_RX_MODEM_PIN, L_PIN_FUNC_2);
    gpio_ret_tx = Liot_SetPinFunc(GNSS_TX_MODEM_PIN, L_PIN_FUNC_2);
    liot_trace("[gnss-cc1161w] pinmux set RX ret=0x%x TX ret=0x%x",
               (unsigned int)gpio_ret_rx, (unsigned int)gpio_ret_tx);
    liot_trace("[gnss-cc1161w] pinmux read RX=%d TX=%d",
               (int)Liot_GetPinFunc(GNSS_RX_MODEM_PIN),
               (int)Liot_GetPinFunc(GNSS_TX_MODEM_PIN));

    ret = gnss_uart_start(GNSS_UART_BAUDRATE);
    liot_trace("[gnss-cc1161w] baud=%d init ret=%d",
               (int)GNSS_UART_BAUDRATE, (int)ret);
    if (ret != L_UART_SUCCESS) {
        liot_trace("[gnss-cc1161w] UART init failed");
        return;
    }
    s_uart_ready = true;

    liot_trace("[gnss-cc1161w] UART ready, enable peripheral power");
    power25_ret = Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_rtos_task_sleep_ms(10);
    power27_ret = Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_trace("[gnss-cc1161w] power GPIO25 ret=0x%x GPIO27 ret=0x%x",
               (unsigned int)power25_ret, (unsigned int)power27_ret);
    liot_rtos_task_sleep_ms(GNSS_POWER_STABLE_MS);

    liot_trace("[gnss-cc1161w] UART ready, waiting for boot log/NMEA");
    for (;;) {
        liot_rtos_task_sleep_ms(10000);
        liot_trace("[gnss-cc1161w] baud=%d callbacks=%lu bytes=%lu NMEA lines=%lu",
                   (int)GNSS_UART_BAUDRATE,
                   (unsigned long)s_rx_callback_count,
                   (unsigned long)s_rx_byte_count,
                   (unsigned long)s_nmea_line_count);
        if (s_rx_byte_count == 0) {
            gnss_log_raw_sample();
            liot_trace("[gnss-cc1161w] RX pin55 level=%d",
                       (int)Liot_GetPinLevel(GNSS_RX_MODEM_PIN));
        }
    }
}
