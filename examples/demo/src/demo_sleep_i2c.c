/**
 * @file demo_sleep_i2c.c
 * @brief LIoT I2C Sleep Mode Demonstration Application
 * @details This file implements an I2C sensor (MPU6050) control example with sleep mode
 *          management for the LIoT platform, including sensor data acquisition and
 *          automatic sleep/wake control.
 * @version 1.0
 * @date 2024-11-05
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

/**
 * Demo workflow:
 * 1. Initialize I2C interface and MPU6050 sensor
 * 2. Register wake-up callback function
 * 3. Read sensor data periodically
 * 4. Enter sleep mode after sampling
 * 5. Wake up and repeat the cycle
 */

#include <stdint.h>
#include "lierda_app_main.h"
#include "liot_dev.h"
#include "liot_gpio2.h"
#include "liot_i2c.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "liot_type.h"

// Chip-specific I2C pin configuration
#if defined(CHIP_EC718)
#define LIOT_I2C_SCL_BIT        67
#define LIOT_I2C_SCL_FUNC       2
#define LIOT_I2C_SDA_BIT        66
#define LIOT_I2C_SDA_FUNC       2
#elif defined(CHIP_EC716)
#define LIOT_I2C_SCL_BIT        67
#define LIOT_I2C_SCL_FUNC       1
#define LIOT_I2C_SDA_BIT        66
#define LIOT_I2C_SDA_FUNC       1
#else
#error "Sleep I2C demo supports EC716 and EC718 only"
#endif

// MPU6050 sensor configuration
#define MPU6050_I2C_ADDR        0x68      // MPU6050 I2C device address
#define MPU6050_ACCEL_XOUT_H    0x3B      // Accelerometer data start register
#define MPU6050_PWR_MGMT_1      0x6B      // Power management register 1
#define MPU6050_I2C_NO          0         // I2C controller number
#define MPU6050_SAMPLE_COUNT    10        // Number of samples before entering sleep

// Sleep mode configuration
// Note: Using LOW mode to preserve RTOS state (RAM, queue, task context).
// Peripherals will lose power in LOW mode and need re-initialization after wake-up.
// Switching between NORMAL (active) and LOW (sleep) for power saving.
// Reference: demo_sleepex.c implementation pattern.
//#define LIOT_MAX_SLEEP_DEPTH    LIOT_SLEEP_MODE_NORMAL
#define LIOT_MAX_SLEEP_DEPTH    LIOT_SLEEP_MODE_LOW
//#define LIOT_MAX_SLEEP_DEPTH    LIOT_SLEEP_MODE_DEEP_LOW

#define LIOT_QUEUE_WAIT_TIMEOUT     10000 // Queue wait timeout in milliseconds
#define LIOT_QUEUE_DEPTH            10    // Message queue depth
#define MPU6050_SAMPLE_INTERVAL_MS  200   // Sampling interval in milliseconds

/**
 * @brief Sensor event message types
 */
typedef enum {
    DEMO_SENSOR_INIT,          // Initialize sensor
    DEMO_SENSOR_WAKEUP,        // Wake up from sleep
    DEMO_SENSOR_DATA,          // Trigger data acquisition cycle
    DEMO_MPU6050_INIT,         // Initialize MPU6050 device
    DEMO_MPU6050_GET,          // Read MPU6050 sensor data
    DEMO_SENSOR_SLEEP,         // Enter sleep mode
} demo_sleep_i2c_msg_t;

static liot_queue_t s_mpu6050MsgQueue = NULL;

/**
 * @brief Send event message to sensor task queue
 * @param event Event type to send
 * @return LIOT_SUCCESS on success, error code otherwise
 */
static liot_errcode_e sensor_send_event(demo_sleep_i2c_msg_t event)
{
    uint8_t msg = (uint8_t)event;
    liot_errcode_e ret = LIOT_SUCCESS;

    if (s_mpu6050MsgQueue == NULL) {
        return LIOT_INVALID_PARAM_ERR;
    }

    ret = liot_rtos_queue_release(s_mpu6050MsgQueue, sizeof(msg), &msg, 0);
    if (ret != LIOT_SUCCESS) {
        liot_trace("queue release failed: %d", ret);
    }

    return ret;
}

/**
 * @brief Wake-up callback function executed after exiting sleep mode
 * @details This function runs in ISR context, only does ISR-safe operations
 */
static void sensor_post_sleep_process(void)
{
    // ISR-safe: use ISR-specific queue release, no logging or blocking calls
    uint8_t msg = (uint8_t)DEMO_SENSOR_WAKEUP;
    if (s_mpu6050MsgQueue != NULL) {
        liot_rtos_queue_release_isr(s_mpu6050MsgQueue, sizeof(msg), &msg);
    }
}

/**
 * @brief Initialize MPU6050 sensor
 * @param i2c_no I2C controller number
 * @return LIOT_I2C_SUCCESS on success, error code otherwise
 * @details Write 0 to power management register to wake up the sensor
 */
static int mpu6050_init(int i2c_no)
{
    uint8_t data = 0;
    int ret = liot_I2cWrite(i2c_no, MPU6050_I2C_ADDR, MPU6050_PWR_MGMT_1, &data, 1);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 power management write failed[%d]", ret);
    }
    return ret;
}

/**
 * @brief Read accelerometer data from MPU6050
 * @param i2c_no I2C controller number
 * @param ax Pointer to store X-axis acceleration value
 * @param ay Pointer to store Y-axis acceleration value
 * @param az Pointer to store Z-axis acceleration value
 * @return LIOT_I2C_SUCCESS on success, error code otherwise
 */
static int mpu6050_read_accel(int i2c_no, int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t data[6];
    int ret = LIOT_I2C_SUCCESS;

    // Check pointer validity
    if (ax == NULL || ay == NULL || az == NULL) {
        liot_trace("MPU6050 read accel: NULL pointer");
        return LIOT_I2C_INVALID_PARAM_ERR;
    }

    ret = liot_I2cRead(i2c_no, MPU6050_I2C_ADDR, MPU6050_ACCEL_XOUT_H, data, sizeof(data));
    if (ret != LIOT_I2C_SUCCESS) {
        return ret;
    }

    *ax = (int16_t)((data[0] << 8) | data[1]);
    *ay = (int16_t)((data[2] << 8) | data[3]);
    *az = (int16_t)((data[4] << 8) | data[5]);
    return LIOT_I2C_SUCCESS;
}

/**
 * @brief I2C sleep mode demo main thread
 * @param arg Thread parameter (unused)
 * @details Main thread that manages sensor initialization, data acquisition, and sleep mode control
 */
void liot_sleep_i2c_demo_thread(void *arg)
{
    (void)arg;
    uint8_t msg = 0;
    int ret = 0;
    LiotSleepModeCfg_t mode_cfg = {0};  // Initialize structure

    // Turn off radio frequency CFUN = 0
    liot_dev_set_modem_fun(0, false, 0);

    // Configure I2C pins
    Liot_SetPinFunc(LIOT_I2C_SCL_BIT, LIOT_I2C_SCL_FUNC);
    Liot_SetPinFunc(LIOT_I2C_SDA_BIT, LIOT_I2C_SDA_FUNC);

    // Initialize I2C controller
    ret = liot_I2cInit(MPU6050_I2C_NO, 0);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("I2C0 INIT FAILED[%d]", ret);
        return;
    }

    // Create message queue for sensor events
    ret = liot_rtos_queue_create(&s_mpu6050MsgQueue, sizeof(msg), LIOT_QUEUE_DEPTH);
    if (ret != LIOT_SUCCESS) {
        liot_trace("MPU6050 QUEUE CREATE FAILED[%d]", ret);
        return;
    }

    // Register wake-up callback function
    ret = liot_wakeup_register_cb(sensor_post_sleep_process);
    if (ret != LIOT_SLEEP_SUCCESS) {
        liot_trace("WAKEUP CALLBACK REGISTER FAILED[%d]", ret);
        // Cleanup: delete queue before return
        liot_rtos_queue_delete(s_mpu6050MsgQueue);
        s_mpu6050MsgQueue = NULL;
        return;
    }

    // Start initialization sequence
    sensor_send_event(DEMO_SENSOR_INIT);

    while (1) {
        // Wait for message queue event; timeout or error triggers sleep mode
        ret = liot_rtos_queue_wait(s_mpu6050MsgQueue, &msg, sizeof(msg), LIOT_QUEUE_WAIT_TIMEOUT);
        if (ret != LIOT_SUCCESS) {
            // Timeout or error: trigger sleep after idle period
            sensor_send_event(DEMO_SENSOR_SLEEP);
            continue;
        }

        switch ((demo_sleep_i2c_msg_t)msg) {
            case DEMO_SENSOR_INIT:
                sensor_send_event(DEMO_MPU6050_INIT);
                sensor_send_event(DEMO_SENSOR_DATA);
                break;

            case DEMO_SENSOR_WAKEUP:
                liot_trace("exit sleep");
                // Restore to normal mode after wake up
                mode_cfg.mode = LIOT_SLEEP_MODE_NORMAL;
                ret = Liot_SleepSetMode(&mode_cfg);
                if (ret != LIOT_SLEEP_SUCCESS) {
                    liot_trace("Restore normal mode failed[%d]", ret);
                }
                // Re-initialize peripherals (I2C pins and controller lost power in LOW mode)
                Liot_SetPinFunc(LIOT_I2C_SCL_BIT, LIOT_I2C_SCL_FUNC);
                Liot_SetPinFunc(LIOT_I2C_SDA_BIT, LIOT_I2C_SDA_FUNC);
                ret = liot_I2cInit(MPU6050_I2C_NO, 0);
                if (ret != LIOT_I2C_SUCCESS) {
                    liot_trace("I2C re-init failed[%d]", ret);
                }
                // Re-initialize MPU6050 sensor
                ret = mpu6050_init(MPU6050_I2C_NO);
                if (ret != LIOT_I2C_SUCCESS) {
                    liot_trace("MPU6050 re-init failed[%d]", ret);
                }
                // Send data event to start sampling
                sensor_send_event(DEMO_SENSOR_DATA);
                break;

            case DEMO_SENSOR_DATA: {
                static uint8_t s_sampleCount = 0;
                sensor_send_event(DEMO_MPU6050_GET);
                if (++s_sampleCount < MPU6050_SAMPLE_COUNT) {
                    sensor_send_event(DEMO_SENSOR_DATA);
                } else {
                    s_sampleCount = 0;
                    // Explicitly send sleep event after sampling cycle completes
                    sensor_send_event(DEMO_SENSOR_SLEEP);
                }
                break;
            }

            case DEMO_MPU6050_INIT:
                ret = mpu6050_init(MPU6050_I2C_NO);
                if (ret == LIOT_I2C_SUCCESS) {
                    liot_trace("MPU6050 INIT");
                } else {
                    liot_trace("MPU6050 INIT FAILED[%d]", ret);
                }
                break;

            case DEMO_MPU6050_GET: {
                int16_t ax = 0, ay = 0, az = 0;
                ret = mpu6050_read_accel(MPU6050_I2C_NO, &ax, &ay, &az);
                if (ret == LIOT_I2C_SUCCESS) {
                    liot_trace("MPU6050 Accel X:%d Y:%d Z:%d", ax, ay, az);
                } else {
                    liot_trace("MPU6050 READ FAILED[%d]", ret);
                }
                liot_rtos_task_sleep_ms(MPU6050_SAMPLE_INTERVAL_MS);
                break;
            }

            case DEMO_SENSOR_SLEEP:
                liot_trace("ENTER sleep");
                mode_cfg.mode = LIOT_MAX_SLEEP_DEPTH;
                ret = Liot_SleepSetMode(&mode_cfg);
                if (ret != LIOT_SLEEP_SUCCESS) {
                    liot_trace("Sleep mode set failed[%d]", ret);
                }
                break;

            default:
                break;
        }
    }
}
