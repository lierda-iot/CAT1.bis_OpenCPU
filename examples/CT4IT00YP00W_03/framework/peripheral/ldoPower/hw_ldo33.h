#ifndef PERIPHERAL_POWER_H
#define PERIPHERAL_POWER_H

#include <stdint.h>

/**
 * @brief Initialize and power on all lod33 devices
 * @details Powers on LDO33 (AON domain), GX8006 module, and PA amplifier
 * @return 0 on success, -1 on failure
 */
int lod33_power_init(void);

/**
 * @brief Power off all lod33 devices
 */
void lod33_power_deinit(void);

#endif /* PERIPHERAL_POWER_H */
