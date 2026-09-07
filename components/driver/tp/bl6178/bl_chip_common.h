#ifndef BL_CHIP_COMMON_H
#define BL_CHIP_COMMON_H
#include "bl_chip_custom.h"

#define	BL8XXX_60	0x01//bl8105,bl8265,bl8335,bl8495,bl8810,bl8818h,bl8825,bl8858c,bl8868c
#define	BL8XXX_61	0x02//bl8281,bl8331,bl8858s,bl8868s
#define	BL8XXX_63	0x03//bl8668,bl8678,bl8818,bl8858h,bl8868h,bl8878
#define	BL6XX0		0x04//bl6090.bl6130,bl6280,bl6360
#define	BL6XX1		0x05//bl6391
#define BL6XX3      0x06//bl6131z,bl6133u,blm13
#define	BL6XX6		0x07//bl7450
#define	BL7XX0		0x08//bl7450
#define BL7XX1      0x09
#define BL7XX3      0x0a
#define BL66X       0x0b
#define BLM18       0x0c
#define BL6XX8      0x0d
#define BLT7XX6     0x0e

#define BL_FLASH_I2C_ADDR		    (0x2c )//8bit addr
#define I2C_WRITE		0x00
#define	I2C_READ		0x01

#define SELF_CTP                    0x00
#define COMPATIBLE_CTP              0x01
#define SELF_INTERACTIVE_CTP        0x02

#define   INT_UPDATE_MODE           0x00
#define   I2C_UPDATE_MODE_OLD       0x01
#define   I2C_UPDATE_MODE_NEW       0x02

#define   FLASH_WSIZE			    128
#define   FLASH_RSIZE			    64
#define   PROJECT_INFO_LEN          8

#define ARGU_MARK  "chp_cfg_mark"

#define TX_NUM_MAX                  60
#define RX_NUM_MAX                  100

#if(TS_CHIP == BL8XXX_60)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	2
#define BTL_FLASH_ID	0x05
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	100

#define CTP_TYPE                COMPATIBLE_CTP
#define UPDATE_MODE             INT_UPDATE_MODE

enum BL8XXX_60_flash_cmd {

	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};

#elif(TS_CHIP == BL8XXX_61)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	5
#define BTL_FLASH_ID	0x06
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	100

#define CTP_TYPE                COMPATIBLE_CTP
#define UPDATE_MODE             INT_UPDATE_MODE

enum BL8XXX_61_flash_cmd {

	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};

#elif(TS_CHIP == BL8XXX_63)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	2
#define BTL_FLASH_ID	0x04
#define	MAX_FLASH_SIZE	0xc000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                COMPATIBLE_CTP
#define UPDATE_MODE             INT_UPDATE_MODE

enum BL8XXX_63_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL6XX0)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	2
#define BTL_FLASH_ID	0x20
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP
#define UPDATE_MODE             INT_UPDATE_MODE

enum BL6XX0_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL6XX1)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	2
#define BTL_FLASH_ID	0x21
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP
#define UPDATE_MODE             I2C_UPDATE_MODE_OLD

enum BL6XX1_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL6XX3)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	1
#define BTL_FLASH_ID	0x22
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP
#define UPDATE_MODE             INT_UPDATE_MODE

enum BL6XX3_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL6XX6)
#define BL_ARGUMENT_BASE_OFFSET	0xfc00
#define	MAX_POINT_NUM	2
#define BTL_FLASH_ID	0x42
#define	MAX_FLASH_SIZE	0x10000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x23
#define VERTIFY_END_OFFSET   0x24

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP
#define UPDATE_MODE             I2C_UPDATE_MODE_OLD

enum BL6XX6_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};

#elif(TS_CHIP == BL6XX8)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	1
#define BTL_FLASH_ID	0x23
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP
#define UPDATE_MODE             I2C_UPDATE_MODE_NEW

enum BL6XX8_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};

#elif(TS_CHIP == BL7XX0)
#define BL_ARGUMENT_BASE_OFFSET	0xbc00
#define	MAX_POINT_NUM	5
#define BTL_FLASH_ID	0x40
#define	MAX_FLASH_SIZE	0xc000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x23
#define VERTIFY_END_OFFSET   0x24

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE             SELF_INTERACTIVE_CTP
#define UPDATE_MODE          I2C_UPDATE_MODE_OLD

enum BL7XX0_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL7XX1)
#define BL_ARGUMENT_BASE_OFFSET	0xfc00
#define	MAX_POINT_NUM	10
#define BTL_FLASH_ID	0x41
#define	MAX_FLASH_SIZE	0x10000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x23
#define VERTIFY_END_OFFSET   0x24

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE             SELF_INTERACTIVE_CTP
#define UPDATE_MODE          I2C_UPDATE_MODE_OLD

enum BL7XX1_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL7XX3)
#define BL_ARGUMENT_BASE_OFFSET	0xbc00
#define	MAX_POINT_NUM	10
#define BTL_FLASH_ID	0x42
#define	MAX_FLASH_SIZE	0xc000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x23
#define VERTIFY_END_OFFSET   0x24

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE             SELF_INTERACTIVE_CTP
#define UPDATE_MODE          I2C_UPDATE_MODE_OLD

enum BL7XX3_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BL66X)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	2
#define BTL_FLASH_ID	0x05
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	100

#define CTP_TYPE                COMPATIBLE_CTP
#define UPDATE_MODE             INT_UPDATE_MODE

enum BL66X_flash_cmd {

	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BLM18)
#define BL_ARGUMENT_BASE_OFFSET	0x200
#define	MAX_POINT_NUM	1
#define BTL_FLASH_ID	0x60
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP
#define UPDATE_MODE             I2C_UPDATE_MODE_OLD

enum BLM18_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == BLT7XX6)
#define BL_ARGUMENT_BASE_OFFSET	0xfc00
#define	MAX_POINT_NUM	10
#define BTL_FLASH_ID	0x43
#define	MAX_FLASH_SIZE	0x10000
#define	PJ_ID_OFFSET	0xcb

#define VERTIFY_START_OFFSET 0x23
#define VERTIFY_END_OFFSET   0x24

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE             SELF_INTERACTIVE_CTP
#define UPDATE_MODE          I2C_UPDATE_MODE_NEW

enum BLT7XX6_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#endif

enum fw_reg {

	WORK_MODE_REG		= 0x00,
    TS_DATA_REG         = 0x01,
	CHECKSUM_REG		= 0x3f,
	CHECKSUM_CAL_REG	= 0x8a,
	AC_REG				= 0x8b,
	RESOLUTION_REG		= 0x98,
	LPM_REG				= 0xa5,
	PROXIMITY_REG		= 0xb0,
	PROXIMITY_FLAG_REG	= 0xB1,
	CALL_REG			= 0xb2,
	BL_CHIP_ID_REG      = 0xb8,
	BL_FWVER_PJ_ID_REG  = 0xb6,
	BL_PRJ_INFO_REG     = 0xb4,
	CHIP_ID_REG         = 0xe7,
	BL_PRJ_ID_REG       = 0xb5,
	COB_ID_REG          = 0x33,
	BL_PROTECT_REG      = 0xee,
	BTL_ESD_REG         = 0xf9,
	#if(CTP_TYPE == SELF_CTP)
	BTL_CHANNEL_RX_REG   = 0x14,
	BTL_CHANNEL_KEY_REG  = 0x15,
	BTL_RAWDATA_REG      = 0x35,
	BTL_CB_REG           = 0x11,
	BTL_CB_CALI_REG      = 0x9c,
	BTL_SET_SCAN_MODE_REG = 0x20,
	BTL_RST_TEST_REG     = 0x88,
	BTL_DIFF_REG         = 0x73,
	BTL_DIFF_REG_PAGE   = 0xc0,
	#endif
};

enum checksum {

	CHECKSUM_READY		= 0x01,
	CHECKSUM_CAL		= 0xaa,
	CHECKSUM_ARG		= 0xba,
};

enum update_type{
	
	NONE_UPDATE		= 0x00,
	FW_ARG_UPDATE   = 0x01,
};

enum firmware_file_type{
	
	HEADER_FILE_UPDATE		= 0x00,
	BIN_FILE_UPDATE		= 0x01,
};

typedef enum 
{
    CTP_UP   = 0,
    CTP_DOWN,
} ctp_pen_state_enum;

#define  TD_STAT_ADDR		    0x1
#define  TD_STAT_NUMBER_TOUCH	0x07
#define  CTP_PATTERN	            0xAA

#define OUTPUT	1
#define INPUT	0
/*************Betterlife ic update***********/
#ifdef      BL_UPDATE_FIRMWARE_ENABLE
#define 	BL_FWVER_MAIN_OFFSET	(0x2a)
#define 	BL_FWVER_ARGU_OFFSET	(0x2b)
#define 	BL_PROJECT_ID_OFFSET	(0x2c)
#define     BL_PROJECT_INFO_OFFSET  (0x0f)
#define     BL_COB_ID_OFFSET        (0x34)
#define     BL_COB_ID_LEN           (12)
#define     BL_ARGUMENT_FLASH_SIZE  (1024)
#define     PRJ_INFO_LEN            (0x08)
#define     FLASH_PAGE_SIZE         (512)
//Update firmware through driver probe procedure with h and c file
#define 	BL_AUTO_UPDATE_FARMWARE
#ifdef		BL_AUTO_UPDATE_FARMWARE

#endif
#endif

/*************Betterlife ic touch pad support****/
#ifdef BL_TOUCH_PAD_PROTOCOL_SUPPORT
#define	BTL_ZOOM_OUT					0x1
#define BTL_ZOOM_IN						0x2
#define	BTL_SINGLE_FINGER_CLICK			0x3
#define BTL_DOUBLE_FINGER_CLICK			0x4
#define	BTL_TRIPLE_FINGER_CLICK			0x5
#define BTL_QUAD_FINGER_CLICK			0xa
#define	BTL_SINGLE_FINGERSWIPE_RIGHT	0x6
#define	BTL_SINGLE_FINGERSWIPE_LEFT		0x7
#define	BTL_SINGLE_FINGERSWIPE_DOWN	    0x8
#define	BTL_SINGLE_FINGERSWIPE_UP	    0x9
#define BTL_TRIPLE_FINGERSWIPE_RIGHT	0x10
#define	BTL_TRIPLE_FINGERSWIPE_LEFT		0x11
#define BTL_TRIPLE_FINGERSWIPE_DOWN		0x12
#define BTL_TRIPLE_FINGERSWIPE_UP		0x13
#define BTL_GESTURE_NONE				0x0
#endif

/*************Betterlife ic factory test support****/
#ifdef BL_FACTORY_SUPPORT
//#define     BL_KEY_SUPPORT
#define ENTER_WORK_FACTORY_RETRIES              3
#define FACTORY_SCAN_RETRIES                    10
#define FACTORY_CALI_RETRIES                    20
#define FACTORY_TEST_DELAY                      20
#endif
#include "liot_log.h"
#define bl_log_trace        liot_trace

#define BTL_DRIVER_VERSION    "betterlife_driver_version_v1.0.6"

#endif
