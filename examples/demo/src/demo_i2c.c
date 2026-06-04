
#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_i2c.h"
#include "liot_os.h"

#define LIOT_I2C_SCL_BIT  67
#define LIOT_I2C_SCL_FUNC (2)

#define LIOT_I2C_SDA_BIT  66
#define LIOT_I2C_SDA_FUNC (2)


/** @brief 8-bit slave write address */
#define SalveAddr_w_8bit   (0x42 >> 1)
/** @brief 8-bit slave read address */
#define SalveAddr_r_8bit   (0x43 >> 1)

/** @brief 16-bit slave write address */
#define SalveAddr_w_16bit (0xa0 >> 1)
/** @brief 16-bit slave read address */
#define SalveAddr_r_16bit (0xa1 >> 1)

/** @brief Test mode selection: 1-8bit register address; 0-16bit register address */
#define demo_for_8bit_or_16bit  (0)

/**
 * @brief I2C function demonstration thread
 * @details Initialize I2C interface and perform read-write operations in a loop. Select 8-bit or 16-bit register address mode based on demo_for_8bit_or_16bit
 * @param[in] arvg Thread parameters (unused)
 */
void liot_i2c_demo_thread(void *arvg)
{
    int i2c_no = 0;
    int ret;
    int fastmode = 0;
    uint8_t read_data = 0;
#if demo_for_8bit_or_16bit
    uint8_t data = 0xaa;  /**< Data to be written in 8-bit mode */
#else
    uint8_t data = 0xab;  /**< Data to be written in 16-bit mode */
#endif

    liot_rtos_task_sleep_ms(200);

    liot_trace("I2C DEMO TEXT !!!");

    // Configure I2C pin functions
    Liot_SetPinFunc(LIOT_I2C_SCL_BIT, LIOT_I2C_SCL_FUNC);
    Liot_SetPinFunc(LIOT_I2C_SDA_BIT, LIOT_I2C_SDA_FUNC);

    // Initialize I2C interface
    ret = liot_I2cInit(i2c_no, fastmode);
    if (ret != LIOT_I2C_SUCCESS)
    {
        liot_trace("I2C INIT FAILED[%d]", ret);
    }

    while (1)
    {
#if demo_for_8bit_or_16bit
        // 8-bit register address mode: read operation
        liot_I2cRead(i2c_no, SalveAddr_r_8bit, 0xf0, &read_data, 1);
        liot_trace("< read i2c value=0x%x, ret=%d >\n", read_data, ret);

        // 8-bit register address mode: write operation
        liot_I2cWrite(i2c_no, SalveAddr_w_8bit, 0x55, &data, 1);
        liot_trace("< write i2c value=0x%x, ret=%d >\n", data, ret);
#else
        // 16-bit register address mode: read operation
        ret = liot_I2cRead_16bit_addr(i2c_no, SalveAddr_r_16bit, 0x00ff, &read_data, 1);
        liot_trace("< read i2c value=0x%x, ret=%d >\n", read_data, ret);

        // 16-bit register address mode: write operation
        ret = liot_I2cWrite_16bit_addr(i2c_no, SalveAddr_w_16bit, 0x00ff, &data, 1);
        liot_trace("< write i2c value=0x%x, ret=%d >\n", data, ret);
#endif
        read_data = 0;
        liot_rtos_task_sleep_ms(200);  /**< Delay 200ms after each operation */
    }

    liot_rtos_task_delete(NULL); // kill itsel
}