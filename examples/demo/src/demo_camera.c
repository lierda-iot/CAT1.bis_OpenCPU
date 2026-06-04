/**
 * @file demo_camera.c
 * @brief Camera demonstration program
 * @details This file implements a camera demo that captures images and performs QR code decoding.
 *          It demonstrates camera initialization, image capture, and decoder integration.
 * 
 * @author Lierda
 * @version 1.0
 * @date 2026-03-26
*/

#include <stdio.h>
#include <string.h>
#include "liot_type.h"
#include "liot_uart2.h"
#include "liot_sleep.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_api_common.h"
#include "liot_gpio2.h"
#include "liot_camera.h"
#include "liot_decode.h"

#define LIOT_CAMERA_DEMO_TEST_GC032A    1   

#define DEMO_CAMERA_CSPI_PORT   LIOT_CSPI_PORT1     
#define DEMO_CAMERA_I2C_NUM     0                   
#define DEMO_CAMERA_SDA_PIN		58                  
#define DEMO_CAMERA_SCL_PIN		57                  
#define DEMO_DECODER_TASK_STACK_SIZE    (250 * 1024)

const Liot_Framesize_t g_FramesizeList[FRAMESIZE_MAX] = {
    {96, 96},
    {160, 120},
    {176, 144},
    {240, 160},
    {240, 240},
    {320, 240},
    {352, 288},
    {480, 320},
    {640, 480},
};

#define SENSOR_WIDTH(Liot_Framesize_e)    g_FramesizeList[Liot_Framesize_e].width
#define SENSOR_HEIGHT(Liot_Framesize_e)    g_FramesizeList[Liot_Framesize_e].height

LIOT_ADD_CAMERA(liot_gc032a_2ddr); 

uint8_t *camdemo_task_stack = NULL; 
uint8_t *img = NULL;  

static uint8_t txbuf[1024] = {0};  
static uint8_t decodeResult[1024] = {0};  

static liot_task_t camdemo_taskRef = NULL;
static liot_StaticTask_t camdemo_task_mem;

#define IMG_WIDTH   SENSOR_WIDTH(FRAMESIZE_VGA)  
#define IMG_HEIGHT  SENSOR_HEIGHT(FRAMESIZE_VGA) 

void liot_uart2_notify_cb(liot_uart_e port, char *data, uint32_t size, void *argc)
{
    liot_trace("UART port %d receive size:%d, data=%s", port, size, data);
}

void camdemo_task(void *argv)
{
    int ret;

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);
    
    liot_camera_config_t cfg = {  
#if LIOT_CAMERA_DEMO_TEST_GC032A == 1
        .sensor = &liot_gc032a_2ddr,   
# endif
        .cspi = {  
            .num = DEMO_CAMERA_CSPI_PORT,  
            .speed = LIOT_CAM_25_5_M,  
        },
        .i2c = {  
            .num = DEMO_CAMERA_I2C_NUM,  
            .scl = DEMO_CAMERA_SCL_PIN,  
            .sda = DEMO_CAMERA_SDA_PIN,  
        },
        .info = {  
            .format = LIOT_CAMERA_OUTPUT_GRAY,  
            .resolution = {  
#if LIOT_CAMERA_DEMO_TEST_GC032A == 1
                .width.offset = 0,  
                .width.scale = 0,  
                .width.size = IMG_WIDTH,  
                .height.offset = 0,  
                .height.scale = 0,  
                .height.size = IMG_HEIGHT,  
# endif
            }
        }
    };

    liot_camera_handle_t cam = Liot_CameraInit(&cfg);  

    liot_decoder_init(LIOT_DECODER_TYPE_QY);  

    Liot_UartConfig_t usart_config = {0};
    usart_config.baudrate   = L_UART_BR_115200;
    usart_config.data_bit   = L_UART_DATA_8;
    usart_config.flow_ctrl  = L_UART_FC_NONE;
    usart_config.stop_bit   = L_UART_STOP_1;
    usart_config.parity_bit = L_UART_PARITY_NONE;
    ret = Liot_UartInit(L_UART1, &usart_config, liot_uart2_notify_cb, NULL);

    img = liot_rtos_malloc(IMG_WIDTH*IMG_HEIGHT*2);  
    
    while(1)  
    {
        Liot_CameraCaptureImage(cam, img, 1000);

        if(liot_image_decoder(img, IMG_WIDTH, IMG_HEIGHT) == LIOT_DECODER_SUCCESS)  
        {
            Liot_GpioSetLevel(27, L_IO_LOW);
            memset(decodeResult, 0, sizeof(decodeResult));  
            liot_get_decoder_result(decodeResult);  
            liot_trace("result: %s", decodeResult);  
            snprintf(txbuf, sizeof(txbuf), "result: %s", decodeResult);  
            Liot_UartSend(L_UART1, (unsigned char *)(uint8_t*)txbuf, strlen(txbuf));  
            Liot_GpioSetLevel(27, L_IO_HIGH);  
        }
    }
}

void Decoder_task_start(void)
{
    camdemo_task_stack = liot_rtos_malloc(DEMO_DECODER_TASK_STACK_SIZE);
    // 解码需要的栈空间大，使用static接口创建
    LiotOSStatus_t result = liot_rtos_task_create_static(
                                &camdemo_taskRef,
                                DEMO_DECODER_TASK_STACK_SIZE,
                                APP_PRIORITY_NORMAL,
                                "camdemo_task",
                                camdemo_task,
                                camdemo_task_stack,
                                &camdemo_task_mem,
                                NULL);
    if(result == 0)
    {
        liot_trace("camera demo task create success %d", result);
    }
    else
    {
        liot_trace("camera demo task create fail %d", result);
    }   
}

void liot_camera_demo_thread(void *argv)
{
    /**< Start decoder task */
    Decoder_task_start();
    while(1)
    {
        osDelay(1000);
    }
}