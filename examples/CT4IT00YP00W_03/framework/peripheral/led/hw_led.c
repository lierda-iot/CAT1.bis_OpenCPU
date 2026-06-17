#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hw_led.h"
#include "ws2812b.h"
#include "frameworkTypes.h"
#include "liot_os.h"
#include "liot_log.h"
#include "FreeRTOS.h"
#include "queue.h"

#define LED_QUEUE_LENGTH    (8U)
#define LED_TASK_STACK      (1024U)
#define LED_TASK_PRIO       (8U)
#define LED_BLINK_INTERVAL  (300U)

static ws2812b_handle_t g_led_handle = NULL;
static liot_task_t g_led_task = NULL;
static QueueHandle_t g_led_queue = NULL;

typedef struct {
    led_pattern_e pattern;
} led_msg_t;

static volatile led_pattern_e g_current_pattern = LED_PATTERN_OFF;

static void led_apply_solid(led_pattern_e pattern)
{
    if (!g_led_handle)
        return;

    switch (pattern) {
    case LED_PATTERN_OFF:
        ws2812b_clear(g_led_handle);
        break;
    case LED_PATTERN_STANDBY:
        ws2812b_set_color(g_led_handle, -1, 0, 255, 0);
        break;
    case LED_PATTERN_ERROR:
        ws2812b_set_color(g_led_handle, -1, 255, 0, 0);
        break;
    default:
        break;
    }
}

static void led_task(void *arg)
{
    (void)arg;
    led_msg_t msg;
    uint8_t blink_on = 0;
    uint8_t poweroff_count = 0;

    while (1) {
        BaseType_t got = xQueueReceive(g_led_queue, &msg, pdMS_TO_TICKS(LED_BLINK_INTERVAL));

        if (got == pdPASS) {
            g_current_pattern = msg.pattern;
            blink_on = 0;
            poweroff_count = 0;
        }

        if (g_current_pattern == LED_PATTERN_LISTENING ||
            g_current_pattern == LED_PATTERN_THINKING ||
            g_current_pattern == LED_PATTERN_SPEAKING) {
            if (g_led_handle) {
                if (blink_on)
                    ws2812b_set_color(g_led_handle, -1, 0, 0, 255);
                else
                    ws2812b_clear(g_led_handle);
                blink_on = !blink_on;
            }
        } else if (g_current_pattern == LED_PATTERN_POWEROFF) {
            if (g_led_handle) {
                if (poweroff_count < 6) {
                    if (poweroff_count % 2 == 0)
                        ws2812b_set_color(g_led_handle, -1, 0, 255, 0);
                    else
                        ws2812b_clear(g_led_handle);
                    poweroff_count++;
                } else {
                    ws2812b_clear(g_led_handle);
                    g_current_pattern = LED_PATTERN_OFF;
                }
            }
        } else {
            led_apply_solid(g_current_pattern);
        }
    }
}

void ledInit(const led_config_t *cfg)
{
    ws2812b_config_t ws_cfg = {
        .spi_port   = cfg->spi_port,
        .spi_clk    = WS2812B_CLK_6_5MHZ,
        .led_num    = cfg->led_num,
        .brightness = cfg->brightness,
    };
    g_led_handle = ws2812b_init(&ws_cfg);

    g_led_queue = xQueueCreate(LED_QUEUE_LENGTH, sizeof(led_msg_t));

    liot_rtos_task_create(&g_led_task, LED_TASK_STACK, LED_TASK_PRIO,
                          "led_task", led_task, NULL);

    ledSetPattern(LED_PATTERN_STANDBY);
}

void ledSetPattern(led_pattern_e pattern)
{
    if (!g_led_queue)
        return;
    led_msg_t msg = {.pattern = pattern};
    xQueueSend(g_led_queue, &msg, 0);
}
