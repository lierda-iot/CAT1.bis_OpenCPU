#ifndef _TP_TS_H_
#define _TP_TS_H_

#include "liot_i2c.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_tp.h"
#include "bl_chip_common.h"
#include "fw_update.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
// typedef unsigned int uint32_t;

struct btl_chip_info {
    unsigned char chipID;
	unsigned char rx_channel;
	unsigned char key_channel;
};

typedef struct {
    liot_i2c_channel_e i2c_ch;
    int8_t rst_pin;
    int8_t int_pin;
} bl_tp_hw_config_t;

extern bl_tp_hw_config_t g_bl_tp_hw_cfg;

#ifdef BL_FACTORY_SUPPORT
struct btl_factory_test {
    unsigned short rawDataMode0[RX_NUM_MAX*2];
	unsigned short rawDataMode0Min[RX_NUM_MAX*2];
	unsigned short rawDataMode0Max[RX_NUM_MAX*2];
    unsigned char CBMode0[RX_NUM_MAX];
	unsigned char CBMode0Min[RX_NUM_MAX];
	unsigned char CBMode0Max[RX_NUM_MAX];

	unsigned short rawDataMode1[RX_NUM_MAX*2];
	unsigned short rawDataMode1Min[RX_NUM_MAX*2];
	unsigned short rawDataMode1Max[RX_NUM_MAX*2];
	unsigned char CBMode1[RX_NUM_MAX];
	unsigned char CBMode1Min[RX_NUM_MAX];
	unsigned char CBMode1Max[RX_NUM_MAX];

	unsigned short rawDataMode2[RX_NUM_MAX*2];
	unsigned short rawDataMode2Min[RX_NUM_MAX*2];
	unsigned short rawDataMode2Max[RX_NUM_MAX*2];
	unsigned char CBMode2[RX_NUM_MAX];
	unsigned char CBMode2Min[RX_NUM_MAX];
	unsigned char CBMode2Max[RX_NUM_MAX];
};
#endif

#ifdef BL_TOUCH_PAD_PROTOCOL_SUPPORT
struct btl_touch_pad_info {
    unsigned char gestureCode;
	unsigned char leftKKey      :1;
	unsigned char rightKey      :1;
	unsigned char midKey		:1;
	unsigned char				:3;
	unsigned char horizonalFlag	:1;
	unsigned char verticalFlag	:1;
	char deltaX;
	char deltaY;
	char deltaZ;
};
#endif

#define CTP_WRITE		CTP_SLAVE_ADDR
#define CTP_READ		(CTP_SLAVE_ADDR+1)
#define CTP_I2C_DELAY			200

#define MDELAY(n)	    osDelay(n)

extern struct btl_chip_info baseInfo;

extern int CTP_FLASH_I2C_WRITE(u8 i2c_addr, u8 *value, u16 len);
extern int CTP_FLASH_I2C_READ(u8 i2c_addr, u8 *value, u16 len);
extern void bl_ts_set_intup(char level);
extern void bl_ts_set_intmode(char mode);

#if((UPDATE_MODE==I2C_UPDATE_MODE_NEW)||(UPDATE_MODE==I2C_UPDATE_MODE_OLD))
void bl_enter_update_with_i2c(void);
void bl_exit_update_with_i2c(void);
#endif
#if(UPDATE_MODE==INT_UPDATE_MODE)
void bl_enter_update_with_int(void);
void bl_exit_update_with_int(void);
#endif
#ifdef  RESET_PIN_WAKEUP
void bl_ts_reset_wakeup(void);
#endif

#if((UPDATE_MODE==I2C_UPDATE_MODE_NEW)||(UPDATE_MODE==I2C_UPDATE_MODE_OLD))
#define   SET_WAKEUP_HIGH    bl_exit_update_with_i2c()
#define   SET_WAKEUP_LOW	 bl_enter_update_with_i2c()
#endif

#if(UPDATE_MODE==INT_UPDATE_MODE)
#define   SET_WAKEUP_HIGH    bl_exit_update_with_int()
#define   SET_WAKEUP_LOW	 bl_enter_update_with_int()
#endif

#ifdef BL_DEBUG_DIFF
void bl_print_diff_data(void);
#endif
#if (CTP_TYPE == SELF_CTP)
void bl_get_rx_channel_num(unsigned char *rxNum);
void bl_get_key_channel_num(unsigned char *key_num);
#endif

int ctp_bl_ts_init(liot_tp_handle_t handle);
void ctp_enter_sleep(void);
void ctp_exit_sleep(void);

#endif
