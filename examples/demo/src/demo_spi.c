/**
 * @file liot_spi_demo.c
 * @brief This file implements test examples for SPI master and slave modes.
 * @author L (email:ciot_iot_support@lierda.com)
 * @version 1.0
 * @date 2023-09-07
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

/**
 * This file implements test examples for SPI master and slave modes.
 * It includes the following steps:
 * 1. Initialize SPI master and slave modes
 * 2. Configure SPI pins and parameters
 * 3. Test SPI master mode:
 *    - Write data to SPI slave
 *    - Read data from SPI slave
 * 4. Test SPI slave mode:
 *    - Receive data from SPI master
 *    - Send data back to SPI master
 * 5. Print test results
 *    - Print sent data
 *    - Print received data
 *    - Print transfer status
 *    - Print transfer speed
 */

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_spi.h"
#include <string.h>

// Define SPI pin configurations for different chips
#define LIOT_CUR_SPI0_MOSI_PIN_MUN (85)
#define LIOT_CUR_SPI0_MISO_PIN_MUN (84)
#define LIOT_CUR_SPI0_CLK_PIN_MUN  (86)
#define LIOT_CUR_SPI0_CS0_PIN_MUN  (83)
#define LIOT_CUR_SPI_PIN_FUNC      (1)


/** @brief Switch for testing SPI master mode, 1 for enabled, 0 for disabled */
#define SPI_MASTER_DEMO 0

/** @brief Switch for testing SPI slave mode, 1 for enabled, 0 for disabled */
#define SPI_SLAVE_DEMO  1

/** @brief Clock speed for SPI testing */
#define SPI_DEMO_CLK_SPEED          8000000U

/** @brief Length of test data */
#define TEST_DATA_LEN              (128)

/** @brief Test data buffer for writing */
unsigned char demo_data_w[TEST_DATA_LEN] = {0};

/** @brief Test data buffer for reading */
unsigned char demo_data_r[TEST_DATA_LEN] = {0};

/**
 * @brief Transfer completion flag in SPI slave mode
 */
uint8_t isTransferDone = 0;

/**
 * @brief Interrupt callback function in SPI slave mode
 *        This function is called when the SPI transfer is completed or an error occurs.
 * @param event Event flag indicating the transfer status
 */
void liot_spi_demo_callback(uint32_t event)
{
    uint8_t i = 0;
    if(event & LIOT_SPI_EVENT_TRANSFER_COMPLETE)
    {
        isTransferDone = 1;
    }
    else
    {
        liot_trace("spi_demo_callback error %d", event);
        for(i = 0; i < TEST_DATA_LEN; i++)
        {
            liot_trace("[%d]Input:0x%x", i, demo_data_r[i]);
        }
    }
}

/**
 * @brief Thread function for SPI master mode testing
 *        This function initializes the SPI master mode and performs data write and read operations in a loop.
 * @param argv Thread parameters
 */
#if SPI_MASTER_DEMO == 1
void liot_spi_demo_thread(void *argv)
{
    unsigned char i = 1;

    // Configuration structure for SPI settings, used to initialize the SPI master mode.
    liot_spi_config_s cfg = {
        .input_mode = LIOT_SPI_INPUT_TRUE,
        .port = LIOT_SPI_PORT0,
        .framesize = 8,
        .spiclk = SPI_DEMO_CLK_SPEED,
        .cs_polarity0 = LIOT_SPI_CS_ACTIVE_LOW,
        .cs_polarity1 = LIOT_SPI_CS_ACTIVE_LOW,
        .cpol = LIOT_SPI_CPOL_HIGH,
        .cpha = LIOT_SPI_CPHA_2Edge,
        .input_sel = LIOT_SPI_DI_1,
        .transmode = LIOT_SPI_DIRECT_POLLING,
        .cs = LIOT_SPI_CS0,
        .clk_delay = LIOT_SPI_CLK_DELAY_0,
        .device_mode = LIOT_SPI_DEVICE_MODE_MASTER,
        .data_msb_lsb = LIOT_SPI_DATA_MSB_LSB, 
        .irq_callback = NULL,
    };

    Liot_SetPinFunc(LIOT_CUR_SPI0_MOSI_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    Liot_SetPinFunc(LIOT_CUR_SPI0_MISO_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    Liot_SetPinFunc(LIOT_CUR_SPI0_CLK_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    liot_spi_init_ext(cfg);

    while (1)
    {
        liot_rtos_task_sleep_ms(5000);
    
        liot_trace("spi demo master running...");
        memset(demo_data_w, i++, TEST_DATA_LEN);
        memset(demo_data_r, 0, TEST_DATA_LEN);

        liot_spi_write_read(LIOT_SPI_PORT0, demo_data_r, demo_data_w, TEST_DATA_LEN);

        liot_trace("Output:0x%x,0x%x,0x%x", demo_data_w[0], demo_data_w[TEST_DATA_LEN/2], demo_data_w[TEST_DATA_LEN-1]);
        liot_trace("Input :0x%x,0x%x,0x%x", demo_data_r[0], demo_data_r[TEST_DATA_LEN/2], demo_data_r[TEST_DATA_LEN-1]);
    }
    liot_rtos_task_delete(NULL);
}
#elif SPI_SLAVE_DEMO == 1

/**
 * @brief Thread function for SPI slave mode testing
 *        This function initializes the SPI slave mode and performs data read and write operations in a loop, 
 *        waiting for the transfer to complete.
 * 
 * @param argv Thread parameters
 */
void liot_spi_demo_thread(void *argv)
{
    unsigned char i = 1;
    uint32_t timeOut_ms = 5000;

    // Configuration structure for SPI settings, used to initialize the SPI slave mode.
    liot_spi_config_s cfg = {
        .input_mode = LIOT_SPI_INPUT_TRUE,
        .port = LIOT_SPI_PORT0,
        .framesize = 8,
        .spiclk = SPI_DEMO_CLK_SPEED,
        .cs_polarity0 = LIOT_SPI_CS_ACTIVE_LOW,
        .cs_polarity1 = LIOT_SPI_CS_ACTIVE_LOW,
        .cpol = LIOT_SPI_CPOL_HIGH,
        .cpha = LIOT_SPI_CPHA_2Edge,
        .input_sel = LIOT_SPI_DI_1,
        .transmode = LIOT_SPI_DMA_IRQ,
        .cs = LIOT_SPI_CS0,
        .clk_delay = LIOT_SPI_CLK_DELAY_0,
        .device_mode = LIOT_SPI_DEVICE_MODE_SLAVE,
        .data_msb_lsb = LIOT_SPI_DATA_MSB_LSB, 
        .irq_callback = liot_spi_demo_callback,
    };

    Liot_SetPinFunc(LIOT_CUR_SPI0_MOSI_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    Liot_SetPinFunc(LIOT_CUR_SPI0_MISO_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    Liot_SetPinFunc(LIOT_CUR_SPI0_CLK_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    Liot_SetPinFunc(LIOT_CUR_SPI0_CS0_PIN_MUN, LIOT_CUR_SPI_PIN_FUNC);
    liot_spi_init_ext(cfg);

    while (1)
    {
        liot_trace("spi demo slave running...");
        memset(demo_data_w, i++, TEST_DATA_LEN);
        memset(demo_data_r, 0, TEST_DATA_LEN);
        timeOut_ms = 5000;

        liot_spi_write_read(LIOT_SPI_PORT0, demo_data_r, demo_data_w, TEST_DATA_LEN);

        do {
            liot_rtos_task_sleep_ms(1);
        } while ((isTransferDone == false) && --timeOut_ms);
        
        if(timeOut_ms == 0)
            liot_trace("Slave receive failed for timeout\n");

        liot_trace("Output:0x%x,0x%x,0x%x", demo_data_w[0], demo_data_w[TEST_DATA_LEN/2], demo_data_w[TEST_DATA_LEN-1]);
        liot_trace("Input :0x%x,0x%x,0x%x", demo_data_r[0], demo_data_r[TEST_DATA_LEN/2], demo_data_r[TEST_DATA_LEN-1]);
    }
    liot_rtos_task_delete(NULL);
}
#endif
