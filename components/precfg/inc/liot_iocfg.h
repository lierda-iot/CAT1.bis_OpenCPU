#ifndef __HW_DRIVER_IO_REOURCE_H_
#define __HW_DRIVER_IO_REOURCE_H_

typedef struct _ioPin {
  unsigned char    pinNum;      //chip pin addr
  unsigned char    funcNum;     //chip func
} ioPin;

typedef struct _Uartdriveio
{
    ioPin   *pin_tx;    //  TX Pin identifier
    ioPin   *pin_rx;    //  RX Pin identifier
    ioPin   *pin_cts;   //  CTS Pin identifier
    ioPin   *pin_rts;   //  RTS Pin identifier
} Uartdriveio;

typedef struct _I2cdriveio
{
    ioPin   *pin_scl; //  SCL Pin identifier
    ioPin   *pin_sda; //  SDA Pin identifier
} I2cdriveio;

typedef struct _Spidriveio
{
    ioPin   *pin_sclk;  //  SCLK Pin identifier
    ioPin   *pin_ssn;   //  SSn Pin identifier
    ioPin   *pin_mosi;  //  MOSI Pin identifier
    ioPin   *pin_miso;  //  MISO Pin identifier
} Spidriveio;

typedef struct _Cspidriveio
{
    ioPin   *pin_mclk;   ///< Main clk Pin identifier
    ioPin   *pin_pclk;   ///< Pixel clk Pin identifier
    ioPin   *pin_cs;     ///< Cs Pin identifier
    ioPin   *pin_sdo0;   ///< Din Pin identifier
    ioPin   *pin_sdo1;   ///< Dout Pin identifier
} Cspidriveio;

typedef struct _I2sdriveio
{
    ioPin   *pin_mclk;  //  main clk Pin identifier
    ioPin   *pin_bclk;  //  pixel clk Pin identifier
    ioPin   *pin_lrck;  //  cs Pin identifier
    ioPin   *pin_din;   //  din Pin identifier
    ioPin   *pin_dout;  //  dout Pin identifier
} I2sdriveio;

typedef struct _Candriveio
{
    ioPin   *pin_tx;
    ioPin   *pin_rx;
    ioPin   *pin_stb;
} Candriveio;

typedef struct _DriveioMap
{
    Uartdriveio *uart;
    I2cdriveio  *i2c;
    Spidriveio  *spi;
    Cspidriveio *cspi;
    I2sdriveio  *i2s;
    //Candriveio can;
} DriveioMap;

int hwDriveIOinit(DriveioMap *hwmap);

#endif