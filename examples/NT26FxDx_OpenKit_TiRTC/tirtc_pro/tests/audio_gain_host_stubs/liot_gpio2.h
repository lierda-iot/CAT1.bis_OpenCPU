#ifndef PRO_AUDIO_GAIN_HOST_GPIO_H
#define PRO_AUDIO_GAIN_HOST_GPIO_H
typedef enum { L_GPIO_11 = 11 } liot_gpio_e;
typedef enum { L_IO_LOW, L_IO_HIGH, L_IO_NONE } liot_gpiolvl_e;
liot_gpiolvl_e Liot_GpioGetLevel(liot_gpio_e gpio);
#endif
