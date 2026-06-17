#ifndef LED_MODULE_H
#define LED_MODULE_H

#include <stdint.h>

typedef enum {
    LED_PATTERN_OFF = 0,
    LED_PATTERN_STANDBY,
    LED_PATTERN_LISTENING,
    LED_PATTERN_THINKING,
    LED_PATTERN_SPEAKING,
    LED_PATTERN_ERROR,
    LED_PATTERN_POWEROFF,
} led_pattern_e;

typedef struct {
    uint8_t spi_port;
    uint8_t led_num;
    uint8_t brightness;
} led_config_t;

void ledInit(const led_config_t *cfg);
void ledSetPattern(led_pattern_e pattern);

#endif /* LED_MODULE_H */
