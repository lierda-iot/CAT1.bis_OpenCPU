#ifndef _PINMAP_H_
#define _PINMAP_H_

#include "liot_gpio2.h"

/* SSD1306 OLED display */
#define SSD1306_I2C_NUM         0

#define SSD1306_I2C_SDA_GPIO    L_GPIO_16
#define SSD1306_I2C_SDA_PIN     (38)    /* Physical pin number for I2C driver */
#define SSD1306_I2C_SDA_FUNC    (2)

#define SSD1306_I2C_SCL_GPIO    L_GPIO_17
#define SSD1306_I2C_SCL_PIN     (39)    /* Physical pin number for I2C driver */
#define SSD1306_I2C_SCL_FUNC    (2)

/* Codec I2C */
#define CODEC_I2C_NUM           1

#define CODEC_I2C_SDA_GPIO      L_GPIO_19
#define CODEC_I2C_SDA_PIN       (66)
#define CODEC_I2C_SDA_FUNC      (3)

#define CODEC_I2C_SCL_GPIO      L_GPIO_18
#define CODEC_I2C_SCL_PIN       (67)
#define CODEC_I2C_SCL_FUNC      (3)

/* WS2812B LED */
#define WS2812B_SPI_NUM         0
#define WS2812B_LED_NUM         3
#define WS2812B_GPIO     	    L_GPIO_9
#define WS2812B_PIN     		(85)
#define WS2812B_FUNC			(1)

/* Speak key */
#define SPEAK_KEY_GPIO          L_GPIO_22
#define SPEAK_KEY_PIN           (19)
#define SPEAK_KEY_FUNC          (0)

/* LDO 3.3V control */
#define LDO3V3_CTRL_GPIO     	L_GPIO_25
#define LDO3V3_CTRL_PIN     	(106)
#define LDO3V3_CTRL_FUNC		(0)

/* PA LDO control */
#define PALDO_CTRL_GPIO     	L_GPIO_27
#define PALDO_CTRL_PIN     	    (16)
#define PALDO_CTRL_FUNC		(0)

/* PA enable control */
#define PA_EN_CTRL_GPIO     	L_GPIO_8
#define PA_EN_CTRL_PIN     	    (83)
#define PA_EN_CTRL_FUNC		(0)

/* Charge state */
#define CHG_STATE_GPIO     	    L_GPIO_2
#define CHG_STATE_PIN     	    (23)
#define CHG_STATE_FUNC		(0)


#endif
