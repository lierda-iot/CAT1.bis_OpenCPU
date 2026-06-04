#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_spi.h"

typedef struct
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
} RGBColor_TypeDef;

#define WS2812B_SPI_PORT    (0)
#define WS2812B_SPI_BIT_L  0xC0
#define WS2812B_SPI_BIT_H  0xFC
#define WS2812B_LED_NUM    3

#define RGBColor_Set(color, index, r, g, b)   do { color[index].R = r; color[index].G = g; color[index].B = b; } while(0)

RGBColor_TypeDef HSV_to_RGB888(uint16_t h, uint8_t s, uint8_t v)
{
    RGBColor_TypeDef rgb;

    uint8_t region, remainder, p, q, t;

    if (s == 0)
    {
        rgb.R = rgb.G = rgb.B = v;
        return rgb;
    }

    region    = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.R = v;
            rgb.G = t;
            rgb.B = p;
            break;
        case 1:
            rgb.R = q;
            rgb.G = v;
            rgb.B = p;
            break;
        case 2:
            rgb.R = p;
            rgb.G = v;
            rgb.B = t;
            break;
        case 3:
            rgb.R = p;
            rgb.G = q;
            rgb.B = v;
            break;
        case 4:
            rgb.R = t;
            rgb.G = p;
            rgb.B = v;
            break;
        default:
            rgb.R = v;
            rgb.G = p;
            rgb.B = q;
            break;
    }

    return rgb;
}

uint8_t ws2812b_spi_send_data(uint8_t *data, uint32_t len)
{
    return liot_spi_write(WS2812B_SPI_PORT, data, len);
}

void ws2812b_data_make(RGBColor_TypeDef Color, uint8_t *data)
{
    for(int i=0; i<=7; i++)
    {
		data[i]     = ( (Color.G & (1 << (7-i)) )? (WS2812B_SPI_BIT_H):WS2812B_SPI_BIT_L );
        data[8+i]   = ( (Color.R & (1 << (7-i)) )? (WS2812B_SPI_BIT_H):WS2812B_SPI_BIT_L );
        data[16+i]  = ( (Color.B & (1 << (7-i)) )? (WS2812B_SPI_BIT_H):WS2812B_SPI_BIT_L );
    }
}

void ws2812b_send_data(RGBColor_TypeDef *color, uint32_t num)
{
    uint8_t *data = liot_rtos_malloc(24 * num);

    for(int i=0;i<num;i++)
        ws2812b_data_make(color[i], &data[i*24]);

    ws2812b_spi_send_data(data, 24 * num);

    liot_rtos_free(data);
}


void ws2812b_init(void)
{
    liot_spi_init(WS2812B_SPI_PORT, LIOT_SPI_DMA_IRQ, 5000000U);
}

void liot_ws2812b_demo_thread(void *argv)
{
    RGBColor_TypeDef gRGBColor[WS2812B_LED_NUM] = {0};
    uint8_t R = 0x00, G = 0x00, B = 0x00;

    liot_rtos_task_sleep_ms(5000);

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    //LDO Ctrl
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);

    ws2812b_init();
    
    while(1)
    {
        for(int i = 0; i < WS2812B_LED_NUM; i++) {
            RGBColor_Set(gRGBColor, i, R, G, B);
        }
        ws2812b_send_data(gRGBColor, WS2812B_LED_NUM);
        R += 0x20;  if(R > 0xff) R = 0x00;
        G += 0x40;  if(G > 0xff) G = 0x00;
        B += 0x60;  if(B > 0xff) B = 0x00;
    
        liot_rtos_task_sleep_ms(500);
    }

    liot_rtos_task_delete(NULL);
}