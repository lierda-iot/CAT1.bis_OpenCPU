/*================================================================
  Copyright (c) 2021, Magic Wireless Solutions Co., Ltd. All rights reserved.
  Magic Wireless Solutions Proprietary and Confidential.
=================================================================*/

/**  @file
 *  demo_uart_sleep_gpio.c
 *  @brief UART, Sleep Mode, GPIO and Wakeup Pad Integration Demonstration
 *
 *  This file demonstrates a comprehensive low-power application combining:
 *  1. UART communication (L_UART2) for data transmission
 *  2. Sleep1 power management with automatic sleep entry
 *  3. GPIO output control (GPIO25) with state toggling
 *  4. Wakeup pad interrupt (WAKEUP_PAD_0) for system wake
 *  5. Event-driven architecture using message queue
 *
 *  Demo workflow:
 *  - Initialize UART, GPIO, and wakeup pad resources
 *  - Send data via UART, then allow system to enter Sleep1
 *  - When UART receives data, prepare for sleep entry
 *  - When wakeup pad is triggered, wake system and toggle GPIO
 *  - Use message queue to coordinate between callbacks and main task
 */

#include <stdint.h>
#include "liot_dev.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_power.h"
#include "liot_uart2.h"
#include "liot_log.h"

// UART configuration
#define UART_SLEEP_DEMO_PORT          L_UART2            // UART port used for demo
#define UART_SLEEP_DEMO_BAUDRATE      L_UART_BR_115200  // UART baud rate: 115200bps

// GPIO configuration
#define UART_SLEEP_DEMO_GPIO          L_GPIO_25          // GPIO pin for output control
#define UART_SLEEP_DEMO_GPIO_PIN      106                // Physical pin number for GPIO25

// Wakeup pad configuration
#define UART_SLEEP_DEMO_WAKEUP_PAD    L_WAKEUPAD_0       // Wakeup pad 0 for interrupt

// Message queue configuration
#define UART_SLEEP_DEMO_QUEUE_LENGTH  8                  // Queue can hold 8 events
#define UART_SLEEP_DEMO_QUEUE_WAIT    LIOT_WAIT_FOREVER  // Wait indefinitely for events

// UART transmission data
#define UART_SLEEP_DEMO_TX_DATA       "UART sleep demo ready\r\n"
#define UART_SLEEP_DEMO_TX_DATA_SIZE  (sizeof(UART_SLEEP_DEMO_TX_DATA) - 1)

/**
 * @brief Event types for inter-task communication
 */
typedef enum {
    UART_SLEEP_DEMO_EVENT_SEND = 1,        // Trigger UART data transmission
    UART_SLEEP_DEMO_EVENT_ENTER_SLEEP,     // Allow system to enter sleep mode
    UART_SLEEP_DEMO_EVENT_WAKE             // Handle wakeup event
} uart_sleep_demo_event_e;

// Global state variables - shared by callbacks and demo task
static liot_queue_t s_uartSleepDemoQueue = NULL;   // Message queue for event passing
static bool s_uartSleepDemoGpioHigh = true;        // Current GPIO output state

/**
 * @brief Post event to message queue from interrupt/callback context
 *
 * This is a helper function called from various callbacks to safely
 * send events to the main task via message queue.
 * Note: Uses LIOT_NO_WAIT to avoid blocking in callback context
 *
 * @param event Event type to post to queue
 */
static void post_event_from_callback(uart_sleep_demo_event_e event)
{
    uint8_t eventValue = (uint8_t)event;

    if (s_uartSleepDemoQueue != NULL &&
        liot_rtos_queue_release(s_uartSleepDemoQueue, sizeof(eventValue), &eventValue,
                                LIOT_NO_WAIT) != LIOT_SUCCESS) {
        liot_trace("UART sleep demo event queue full (%u)", eventValue);
    }
}

/**
 * @brief UART receive callback function
 *
 * Called when UART receives data. In this demo, any received data
 * triggers the system to prepare for sleep mode entry.
 *
 * @param port UART port that received data
 * @param data Pointer to received data buffer
 * @param size Number of bytes received
 * @param arg User argument (unused in this demo)
 */
static void uart_sleep_demo_receive_callback(liot_uart_e port, char *data, uint32_t size, void *arg)
{
    (void)arg;

    if (data == NULL || size == 0) {
        return;
    }

    liot_trace("UART %d received %u bytes", (int)port, (unsigned int)size);
    post_event_from_callback(UART_SLEEP_DEMO_EVENT_ENTER_SLEEP);
}

/**
 * @brief Sleep entry callback function
 *
 * Called by system just before entering Sleep1 mode.
 * Used for logging or cleanup before sleep.
 */
static void uart_sleep_demo_enter_sleep_callback(void)
{
    liot_trace("UART sleep demo entering Sleep1");
}

/**
 * @brief System wakeup callback function
 *
 * Called when system wakes from Sleep1 mode.
 * Posts wake event to main task for processing.
 * Note: This callback runs in ISR context, so we use ISR-safe queue release.
 */
static void uart_sleep_demo_wake_callback(void)
{
    uint8_t eventValue = (uint8_t)UART_SLEEP_DEMO_EVENT_WAKE;

    if (s_uartSleepDemoQueue != NULL) {
        liot_rtos_queue_release_isr(s_uartSleepDemoQueue, sizeof(eventValue), &eventValue);
    }
}

/**
 * @brief Initialize UART interface
 *
 * Configures UART with:
 * - Baud rate: 115200
 * - Data bits: 8
 * - Stop bits: 1
 * - Parity: None
 * - Flow control: None
 *
 * @return LIOT error code (L_UART_SUCCESS on success)
 */
static liot_uart_err_e initialize_uart(void)
{
    Liot_UartConfig_t uartConfig = {0};

    uartConfig.baudrate = UART_SLEEP_DEMO_BAUDRATE;
    uartConfig.data_bit = L_UART_DATA_8;
    uartConfig.flow_ctrl = L_UART_FC_NONE;
    uartConfig.stop_bit = L_UART_STOP_1;
    uartConfig.parity_bit = L_UART_PARITY_NONE;
    uartConfig.tx_way = L_UART_TX_DRIVER;

    return Liot_UartInit(UART_SLEEP_DEMO_PORT, &uartConfig,
                         uart_sleep_demo_receive_callback, NULL);
}

/**
 * @brief Initialize wakeup pad and GPIO
 *
 * Performs the following initialization:
 * 1. Enable AON (Always-On) power domain
 * 2. Set AON voltage to 1.8V
 * 3. Configure pin function for GPIO25
 * 4. Initialize GPIO25 as output (default high)
 * 5. Configure wakeup pad 0 with pull-up and falling edge trigger
 *
 * @return true if all initialization succeeded, false otherwise
 */
static bool initialize_wakeup_and_gpio(void)
{
    liot_wakeup_cfg_t wakeupConfig = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,  // Pull-up resistor enabled
        .wakeup_edge = L_INT_EDGE_FALL      // Trigger on falling edge
    };

    // Chain initialization calls - fail if any step fails
    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS ||
        Liot_SetVoltage(L_DOMAIN_AON, L_VOLT_1_80V) != L_GPIO_ERR_SUCCESS ||
        Liot_SetPinFunc(UART_SLEEP_DEMO_GPIO_PIN, L_PIN_FUNC_0) != L_GPIO_ERR_SUCCESS ||
        Liot_GpioInit(UART_SLEEP_DEMO_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL) != L_GPIO_ERR_SUCCESS ||
        Liot_WakeupIntInit(UART_SLEEP_DEMO_WAKEUP_PAD, wakeupConfig, NULL, NULL) != L_GPIO_ERR_SUCCESS) {
        liot_trace("UART sleep demo wakeup/GPIO setup failed");
        return false;
    }

    return true;
}

/**
 * @brief Clean up message queue resources
 *
 * Safely deletes the message queue and resets the handle.
 * Should be called during error handling or task cleanup.
 */
static void cleanup_queue(void)
{
    if (s_uartSleepDemoQueue != NULL) {
        liot_rtos_queue_delete(s_uartSleepDemoQueue);
        s_uartSleepDemoQueue = NULL;
    }
}

/**
 * @brief Handle wakeup event
 *
 * Called from main task when wakeup event is received.
 * Performs the following actions:
 * 1. Disable auto-sleep to prevent immediate re-entry to sleep
 * 2. Toggle GPIO25 output state
 * 3. Log the new GPIO state
 * 4. Trigger a UART transmission
 */
static void handle_wake_event(void)
{
    liot_autosleep_enable(LIOT_NOT_ALLOW_SLEEP);
    s_uartSleepDemoGpioHigh = !s_uartSleepDemoGpioHigh;
    Liot_GpioSetLevel(UART_SLEEP_DEMO_GPIO,
                      s_uartSleepDemoGpioHigh ? L_IO_HIGH : L_IO_LOW);
    liot_trace("Wakeup pad triggered; GPIO25 is %s",
               s_uartSleepDemoGpioHigh ? "high" : "low");
    post_event_from_callback(UART_SLEEP_DEMO_EVENT_SEND);
}

/**
 * @brief UART sleep GPIO demonstration thread main function
 *
 * This is the main entry point for the demo. It performs the following:
 *
 * Initialization phase:
 * 1. Set modem to minimum functionality mode (power saving)
 * 2. Initialize GPIO and wakeup pad hardware
 * 3. Create message queue for event handling
 * 4. Initialize UART interface
 * 5. Register sleep and wakeup callbacks
 * 6. Configure system for Sleep1 mode
 *
 * Runtime phase:
 * - Event-driven loop processing three event types:
 *   * SEND: Transmit data via UART
 *   * ENTER_SLEEP: Allow system to enter sleep mode
 *   * WAKE: Handle system wakeup and toggle GPIO
 *
 * Error handling:
 * - All initialization failures result in early return
 * - Resources are properly cleaned up on failure
 *
 * @param argv Thread parameter (unused)
 */
void liot_uart_sleep_gpio_demo_thread(void *argv)
{
    uint8_t eventValue = 0;
    liot_uart_err_e uartResult = L_UART_SUCCESS;

    (void)argv;

    /* Step 1: Set modem to minimum functionality to save power */
    liot_dev_set_modem_fun(LIOT_DEV_CFUN_MIN, false, 0);

    /* Step 2: Initialize GPIO and wakeup pad */
    if (!initialize_wakeup_and_gpio()) {
        return;  // Initialization failed, exit thread
    }

    /* Step 3: Create message queue for event communication */
    if (liot_rtos_queue_create(&s_uartSleepDemoQueue, sizeof(eventValue),
                               UART_SLEEP_DEMO_QUEUE_LENGTH) != LIOT_SUCCESS) {
        liot_trace("UART sleep demo queue creation failed");
        Liot_WakeupIntDeinit(UART_SLEEP_DEMO_WAKEUP_PAD);
        return;
    }

    /* Step 4: Initialize UART interface */
    uartResult = initialize_uart();
    if (uartResult != L_UART_SUCCESS) {
        liot_trace("UART sleep demo UART init failed (%d)", (int)uartResult);
        cleanup_queue();
        Liot_WakeupIntDeinit(UART_SLEEP_DEMO_WAKEUP_PAD);
        return;
    }

    /* Step 5: Register sleep and wakeup callbacks */
    if (liot_sleep_register_cb(uart_sleep_demo_enter_sleep_callback) != LIOT_SLEEP_SUCCESS ||
        liot_wakeup_register_cb(uart_sleep_demo_wake_callback) != LIOT_SLEEP_SUCCESS) {
        liot_trace("UART sleep demo callback registration failed");
        liot_sleep_register_cb(NULL);
        liot_wakeup_register_cb(NULL);
        Liot_UartDeinit(UART_SLEEP_DEMO_PORT);
        cleanup_queue();
        Liot_WakeupIntDeinit(UART_SLEEP_DEMO_WAKEUP_PAD);
        return;
    }

    /* Step 6: Configure system for Sleep1 mode */
    liot_set_sleep_depth(LIOT_SLP_SLP1_STATE);

    /* Trigger initial UART transmission */
    post_event_from_callback(UART_SLEEP_DEMO_EVENT_SEND);

    /* Main event processing loop */
    while (1) {
        // Wait for event from queue (blocks until event arrives)
        if (liot_rtos_queue_wait(s_uartSleepDemoQueue, &eventValue,
                                 sizeof(eventValue), UART_SLEEP_DEMO_QUEUE_WAIT) != LIOT_SUCCESS) {
            continue;
        }

        // Process event based on type
        switch ((uart_sleep_demo_event_e)eventValue) {
            case UART_SLEEP_DEMO_EVENT_SEND:
                // Send demo message via UART
                Liot_UartSend(UART_SLEEP_DEMO_PORT,
                              (unsigned char *)UART_SLEEP_DEMO_TX_DATA,
                              UART_SLEEP_DEMO_TX_DATA_SIZE);
                break;

            case UART_SLEEP_DEMO_EVENT_ENTER_SLEEP:
                // Allow system to enter Sleep1 mode
                liot_autosleep_enable(LIOT_ALLOW_SLEEP);
                break;

            case UART_SLEEP_DEMO_EVENT_WAKE:
                // Handle wakeup: toggle GPIO and send data
                handle_wake_event();
                break;

            default:
                liot_trace("UART sleep demo ignored event (%u)", eventValue);
                break;
        }
    }
}
