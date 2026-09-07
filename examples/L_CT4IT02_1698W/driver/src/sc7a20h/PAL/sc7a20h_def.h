#ifndef __SC7A20H_DEF_H__
#define __SC7A20H_DEF_H__
#pragma once

/* ================= 功能配置 ================= */
//I2C SPI
#define SL_SC7A20H_SPI_EN_I2C_DISABLE  0x00
#define SL_SC7A20H_RAWDATA_HPF_ENABLE  0x00
#define SL_SC7A20H_INT_DEFAULT_LEVEL   0x01
#define SL_SC7A20H_FIFO_ENABLE         0x00  //0x00-FIFO-DISABLE  0x01-FIFO ENABLE

//0x00: FIFO-12bit;0x01: FIFO-8bit
#define SL_SC7A20H_FIFO_MODE_ENABLE   0x00 


/***************SC7A20H IIC/SPI CONFIG***************/
/**SC7A20H SDO :0***************/
/**SC7A20H SDO :1***************/
#define SL_SC7A20H_SDO_VDD_GND            1

/**SC7A20H IIC_Device 7bits:  0****/
/**SC7A20H IIC_Device 8bits:  1****/
#define SL_SC7A20H_IIC_7BITS_8BITS        0

#if SL_SC7A20H_SDO_VDD_GND==0
#define SC7A20H_IIC_7BITS_ADDR        0x18
#define SC7A20H_IIC_8BITS_WRITE_ADDR  0x30
#define SC7A20H_IIC_8BITS_READ_ADDR   0x31
#else
#define SC7A20H_IIC_7BITS_ADDR        0x19
#define SC7A20H_IIC_8BITS_WRITE_ADDR  0x32
#define SC7A20H_IIC_8BITS_READ_ADDR   0x33
#endif

#if SL_SC7A20H_IIC_7BITS_8BITS==0
#define SC7A20H_IIC_ADDRESS        SC7A20H_IIC_7BITS_ADDR
#else
#define SC7A20H_IIC_WRITE_ADDRESS  SC7A20H_IIC_8BITS_WRITE_ADDR
#define SC7A20H_IIC_READ_ADDRESS   SC7A20H_IIC_8BITS_READ_ADDR
#endif


/* ================= 私有寄存器地址定义 ================= */
#define REG_WHO_AM_I        0x0F
#define REG_VERSION         0x70
#define REG_MODE_HR_CTRL    0x1f
#define REG_CTRL1           0x20
#define REG_CTRL2           0x21
#define REG_CTRL3           0x22
#define REG_CTRL4           0x23
#define REG_CTRL5           0x24
#define REG_INT1_CTRL       0x22
#define REG_INT2_CTRL       0x25
#define REG_FIFO_CTRL       0x2E
#define REG_FIFO_SRC        0x2F
#define REG_FIFO_DATA       0x69
#define REG_DRDY_STATUS     0x27

/* AOI1 (Shake) */
#define REG_AOI1_CFG        0x30
#define REG_AOI1_THS        0x32
#define REG_AOI1_DUR        0x33
#define REG_AOI1_STAT       0x31  //SHAKE中断源寄存器

/* Click */
#define REG_CLICK_CFG       0x38
#define REG_CLICK_SRC       0x39  //CLICK中断源寄存器(读取后清零)
#define REG_CLICK_COEFF1    0x3A
#define REG_CLICK_COEFF2    0x3B
#define REG_CLICK_COEFF3    0x3C
#define REG_CLICK_COEFF4    0x3D

/* Soft Reset */
#define REG_SOFT_RESET      0x68
#define SOFT_RESET_VALUE    0xA5

/* ================= 寄存器位掩码定义 ================= */
/* 0x22 INT1_CTRL */
#define BIT_INT1_CLICK      (1 << 7)
#define BIT_INT1_AOI1       (1 << 6)
#define BIT_INT1_AOI2       (1 << 5)
#define BIT_INT1_DRDY       (1 << 3)
#define MASK_INT1_CTRL      (BIT_INT1_CLICK | BIT_INT1_AOI1 | BIT_INT1_AOI2 | BIT_INT1_DRDY)

/* 0x25 INT2_CTRL */
#define BIT_INT2_CLICK      (1 << 7)
#define BIT_INT2_AOI1       (1 << 6)
#define BIT_INT2_AOI2       (1 << 5)
#define MASK_INT2_CTRL      (BIT_INT2_CLICK | BIT_INT2_AOI1 | BIT_INT2_AOI2)

/* 0x23 CTRL4 (Range) */
#define MASK_RANGE          (0x30)

/* 0x20 CTRL1 (ODR + Mode) */
#define MASK_ODR            (0xF0)
#define MASK_MODE           (0x08)

/* 0x21 CTRL2 (HPF) */
#define BIT_HPF_ENABLE      (1 << 7)
#define BIT_HPIS1           (1 << 6)
#define BIT_HPIS2           (1 << 5)

#endif
