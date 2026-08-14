/**
 * FOTA (Firmware Over-The-Air) Upgrade Demo
 *
 * Description:
 *   Receives firmware upgrade data via UART1, writes it to the file system,
 *   then performs upgrade verification. The device automatically reboots to
 *   complete the firmware update after successful verification.
 *
 * User Operation Steps:
 *   1. Device powers on and waits ~5 seconds, then UART1 outputs "fota download start:"
 *   2. User connects to device UART1 via serial tool (baud 115200, 8N1)
 *   3. User sends total byte count of upgrade file (ASCII string, e.g. "102400")
 *   4. Device outputs "Please send fota data:" after receiving the length
 *   5. User sends the complete firmware binary data (i.e. upgrade.bin file content)
 *   6. Device performs upgrade verification automatically after reception:
 *      If immediate reboot is disabled, logs will be output via serial port;
 *      if immediate reboot is enabled, serial output may not be available
 *      - Verification success: serial outputs "upgrade check ok!", device reboots
 *      - Verification failure: serial outputs "upgrade check failed!", upgrade aborted
 *
 * Serial Configuration:
 *   - Port: UART1
 *   - Baud rate: 115200
 *   - Data bits: 8
 *   - Stop bits: 1
 *   - Parity: None
 *   - Flow control: None
 *
 * Notes:
 *   - The first data sent must be the file length string (<32 bytes), no binary data
 *   - The upgrade file must be a valid firmware package, otherwise verification fails
 *   - Device reboots automatically after successful verification; do not power off during upgrade
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_fota.h"
#include "liot_fs_api.h"
#include "liot_os.h"
#include "liot_uart2.h"

#define LIOT_APP_TEST_FILENAME "upgrade.bin"
#define FOTA_UART_PORT         L_UART1

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define FOTA_QUEUE_MAX 30

typedef struct {
    uint8_t *data;
    uint32_t len;
} fota_msg_t;

static liot_queue_t g_fota_queue = NULL;
static bool g_fota_len_received = false;
static uint32_t g_fota_total_len = 0;
static uint32_t g_fota_recv_len = 0;
static LFILE g_fota_fd = -1;

static void fota_uart_rx_cb(liot_uart_e port, char *data, uint32_t size, void *argc)
{
    if (port != FOTA_UART_PORT || size == 0 || data == NULL)
        return;

    fota_msg_t msg;
    msg.data = (uint8_t *)liot_rtos_malloc(size);
    if (msg.data == NULL)
        return;
    memcpy(msg.data, data, size);
    msg.len = size;

    if (liot_rtos_queue_release(g_fota_queue, sizeof(fota_msg_t), (uint8 *)&msg, 0) != LIOT_SUCCESS) {
        liot_rtos_free(msg.data);
    }
}

static int fota_uart_init(void)
{
    Liot_UartConfig_t config = {0};
    config.baudrate   = L_UART_BR_115200;
    config.data_bit   = L_UART_DATA_8;
    config.stop_bit   = L_UART_STOP_1;
    config.parity_bit = L_UART_PARITY_NONE;
    config.flow_ctrl  = L_UART_FC_NONE;

    liot_uart_err_e ret = Liot_UartInit(FOTA_UART_PORT, &config, fota_uart_rx_cb, NULL);
    if (ret != L_UART_SUCCESS) {
        liot_trace("fota uart init failed, ret=%d", ret);
        return -1;
    }

    Liot_UartSend(FOTA_UART_PORT, (unsigned char *)"fota download start:", 20);
    return 0;
}

static void fota_cleanup(void)
{
    if (g_fota_fd >= 0) {
        liot_fclose(g_fota_fd);
        g_fota_fd = -1;
    }
    Liot_UartDeinit(FOTA_UART_PORT);
    if (g_fota_queue != NULL) {
        liot_rtos_queue_delete(g_fota_queue);
        g_fota_queue = NULL;
    }
}

void liot_fota_demo_thread(void *arg)
{
    int ret = 0;
    fota_msg_t msg;
    bool is_reboot = TRUE;

    liot_rtos_task_sleep_s(5);

    if (liot_rtos_queue_create(&g_fota_queue, sizeof(fota_msg_t), FOTA_QUEUE_MAX) != LIOT_SUCCESS) {
        liot_trace("fota queue create failed");
        goto exit;
    }

    if (fota_uart_init() != 0) {
        liot_trace("fota uart init failed");
        goto exit;
    }

    liot_trace("waiting for fota data length...");

    while (1) {
        memset(&msg, 0, sizeof(fota_msg_t));
        ret = liot_rtos_queue_wait(g_fota_queue, (uint8 *)&msg, sizeof(fota_msg_t), LIOT_WAIT_FOREVER);
        if (ret != LIOT_SUCCESS)
            continue;

        if (!g_fota_len_received) {
            if (msg.len > 0 && msg.len < 32) {
                char len_buf[32] = {0};
                memcpy(len_buf, msg.data, MIN(msg.len, 31));
                g_fota_total_len = atoi(len_buf);

                if (g_fota_total_len == 0) {
                    liot_trace("invalid fota length");
                    liot_rtos_free(msg.data);
                    continue;
                }

                g_fota_len_received = true;
                liot_trace("fota total len=%d", g_fota_total_len);

                liot_remove(LIOT_APP_TEST_FILENAME);
                g_fota_fd = liot_fopen(LIOT_APP_TEST_FILENAME, "wb+");
                if (g_fota_fd < 0) {
                    liot_trace("fopen %s failed", LIOT_APP_TEST_FILENAME);
                    liot_rtos_free(msg.data);
                    goto exit;
                }

                Liot_UartSend(FOTA_UART_PORT, (unsigned char *)"Please send fota data:", 22);
            }
            liot_rtos_free(msg.data);
            continue;
        }

        if (msg.len > 0 && msg.data != NULL) {
            ret = liot_fwrite(msg.data, 1, msg.len, g_fota_fd);
            if (ret <= 0) {
                liot_trace("fwrite failed, ret=%d", ret);
                liot_rtos_free(msg.data);
                goto exit;
            }
            g_fota_recv_len += msg.len;
        }

        liot_rtos_free(msg.data);

        if (g_fota_recv_len >= g_fota_total_len) {
            liot_trace("fota download complete, total=%d", g_fota_recv_len);
            break;
        }
    }

    liot_fclose(g_fota_fd);
    g_fota_fd = -1;

    liot_trace("starting upgrade check...");
    liot_fota_errcode_e result = Liot_FotaAppUpgradeCheck(LIOT_APP_TEST_FILENAME, is_reboot);
    if (result == LIOT_FOTA_UPGRADE_SUCCESS) {
        liot_trace("Upgrade check passed successfully.");
        Liot_UartSend(FOTA_UART_PORT, (unsigned char *)"upgrade check ok!", 17);
    } else {
        liot_trace("Upgrade check failed, error=%d", result);
        Liot_UartSend(FOTA_UART_PORT, (unsigned char *)"upgrade check failed!", 21);
    }

exit:
    fota_cleanup();
    liot_rtos_task_delete(NULL);
}
