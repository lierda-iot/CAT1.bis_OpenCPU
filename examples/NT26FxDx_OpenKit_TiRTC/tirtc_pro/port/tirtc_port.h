#ifndef TIRTC_PORT_H
#define TIRTC_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Native panel/touch dimensions stay portrait; MADCTL addresses it landscape. */
#define TIRTC_PHYSICAL_WIDTH 240U
#define TIRTC_PHYSICAL_HEIGHT 320U
/* Same orientation since release 04; release 35 uses panel hardware rotation. */
#define TIRTC_SCREEN_WIDTH TIRTC_PHYSICAL_HEIGHT
#define TIRTC_SCREEN_HEIGHT TIRTC_PHYSICAL_WIDTH

/* Call once from user_main. Returns 0 only after LCD, touch and UI are ready. */
int tirtc_port_start(void);

/* Thread safe. The verified GPIO backlight supports 0=off, 1..100=on.
 * Keep this percentage API for a future PWM implementation. */
void tirtc_port_set_brightness(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
