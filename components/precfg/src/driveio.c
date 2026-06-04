#include"liot_iocfg.h"

#define __L_IO_MAP_ __attribute__((section(".iomap")))

extern Uartdriveio uartIoResource[];
extern I2cdriveio i2cIoResource[];
extern Spidriveio spiIoResource[];
extern Cspidriveio cspiIoResource[];
extern I2sdriveio i2sIoResource[];

__L_IO_MAP_ DriveioMap hwioResource = 
{
    .uart = (Uartdriveio *)&uartIoResource,
    .i2c = (I2cdriveio *)&i2cIoResource,
    .spi = (Spidriveio *)&spiIoResource,
    .cspi = (Cspidriveio *)&cspiIoResource,
    .i2s = (I2sdriveio *)&i2sIoResource
};