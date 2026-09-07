#include "Tp_ts.h"
#include <string.h>

struct btl_chip_info baseInfo;
bl_tp_hw_config_t g_bl_tp_hw_cfg;

#ifdef BL_TOUCH_PAD_PROTOCOL_SUPPORT
struct btl_touch_pad_info touchPadInfo;
#endif


#ifdef GPIO_EINT
void bl_ts_set_intmode(char mode)
{
	if (g_bl_tp_hw_cfg.int_pin < 0) return;
	if (0 == mode) {
		Liot_GpioInitDirect(g_bl_tp_hw_cfg.int_pin, L_IO_OUTPUT, L_IO_LOW, NULL);
	} else if (1 == mode) {
		Liot_GpioInitDirect(g_bl_tp_hw_cfg.int_pin, L_IO_INPUT, L_IO_NONE, NULL);
	}
}

void bl_ts_set_intup(char level)
{
	if (g_bl_tp_hw_cfg.int_pin < 0) return;
	Liot_SetPinLevel(g_bl_tp_hw_cfg.int_pin, level ? L_IO_HIGH : L_IO_LOW);
}
#endif

#ifdef INT_PIN_WAKEUP
void bl_ts_int_wakeup(void)
{
	bl_ts_set_intmode(0);
	bl_ts_set_intup(1);
	osDelay(20);
	bl_ts_set_intup(0);
	osDelay(1);
	bl_ts_set_intup(1);
	osDelay(20);
	bl_ts_set_intmode(1);
}
#endif

#ifdef RESET_PIN_WAKEUP
void bl_ts_reset_wakeup(void)
{
	if (g_bl_tp_hw_cfg.rst_pin < 0) return;
	Liot_SetPinLevel(g_bl_tp_hw_cfg.rst_pin, L_IO_HIGH);
	osDelay(20);
	Liot_SetPinLevel(g_bl_tp_hw_cfg.rst_pin, L_IO_LOW);
	osDelay(20);
	Liot_SetPinLevel(g_bl_tp_hw_cfg.rst_pin, L_IO_HIGH);
	osDelay(20);
}
#endif

int CTP_FLASH_I2C_WRITE(u8 i2c_addr, u8 *value, u16 len)
{
	uint8_t slave_7bit = i2c_addr;
	if (len == 0) return 1;

	liot_errcode_i2c_e err = liot_I2cWrite(g_bl_tp_hw_cfg.i2c_ch,
	                                        slave_7bit, value[0],
	                                        (len > 1) ? &value[1] : NULL, len - 1);
	if (err != LIOT_I2C_SUCCESS) {
		bl_log_trace("CTP I2C write err: %d", err);
		return 0;
	}
	return 1;
}

int CTP_FLASH_I2C_READ(u8 i2c_addr, u8 *value, u16 len)
{
	uint8_t slave_7bit = i2c_addr;

	liot_errcode_i2c_e err = liot_I2cRead(g_bl_tp_hw_cfg.i2c_ch,
	                                       slave_7bit, 0, value, len);
	if (err != LIOT_I2C_SUCCESS) {
		bl_log_trace("CTP I2C read err: %d", err);
		return 0;
	}
	return (int)len;
}

static void bl_get_data(void)
{
    char buf[2 + 6 * MAX_POINT_NUM];
    unsigned char readPointCmd = TD_STAT_ADDR;

	#ifdef BL_DEBUG_DIFF
	bl_print_diff_data();
	return;
	#endif

    CTP_FLASH_I2C_WRITE(CTP_SLAVE_ADDR, &readPointCmd, 1);
    CTP_FLASH_I2C_READ(CTP_SLAVE_ADDR, buf, sizeof(buf));
	#ifdef BL_DEBUG_NOISE
	if(buf[7]==0x80)
	{
        bl_debug_for_touch(buf);
		return;
	}
	#endif
}

void bl_get_data_user(uint8_t *data)
{
    char buf[2 + 6 * MAX_POINT_NUM];
    unsigned char readPointCmd = TD_STAT_ADDR;

	#ifdef BL_DEBUG_DIFF
	bl_print_diff_data();
	return;
	#endif

int ret;
	extern bl_tp_hw_config_t g_bl_tp_hw_cfg;
	uint8_t slave_7bit = CTP_SLAVE_ADDR;

	liot_errcode_i2c_e err = liot_I2cRead(g_bl_tp_hw_cfg.i2c_ch,
	                                       slave_7bit, readPointCmd, buf, 2 + 6 * MAX_POINT_NUM);
	if (err != LIOT_I2C_SUCCESS) {
		bl_log_trace("bl_get_data_user I2C read err: %d", err);
		return ;
	}

	memcpy(data, buf, 2 + 6 * MAX_POINT_NUM);

	return ;

	#ifdef BL_DEBUG_NOISE
	if(buf[7]==0x80)
	{
        bl_debug_for_touch(buf);
		return;
	}
	#endif
}

#ifdef BL_TOUCH_PAD_PROTOCOL_SUPPORT
uint32 BTL_Read_TouchPad_Data(void)
{
    unsigned char readPointCmd = TD_STAT_ADDR;

	CTP_FLASH_I2C_WRITE(CTP_SLAVE_ADDR, &readPointCmd, 1);
    CTP_FLASH_I2C_READ(CTP_SLAVE_ADDR, (u8 *)&touchPadInfo, sizeof(touchPadInfo));

    if(touchPadInfo.gestureCode) {
        return 0;
    }
	if(touchPadInfo.horizonalFlag) {
        return 0;
    }
    if(touchPadInfo.verticalFlag) {
        return 0;
    }
    if(touchPadInfo.leftKKey) {
        return 0;
    }
    if(touchPadInfo.midKey) {
    	return 0;
    }
    if(touchPadInfo.rightKey) {
    	return 0;
    }
    if(touchPadInfo.deltaX | touchPadInfo.deltaY) {
        return 0;
    }
    return 0;
}
#endif

void ctp_enter_sleep(void)
{
	unsigned char sleepCmd[2] = {0xa5, 0x03};

	CTP_FLASH_I2C_WRITE(CTP_SLAVE_ADDR, sleepCmd, sizeof(sleepCmd));
	osDelay(100);
	bl_log_trace("ctp_enter_sleep");
}

void ctp_exit_sleep(void)
{
	#ifdef RESET_PIN_WAKEUP
	bl_ts_reset_wakeup();
	#endif
	#ifdef INT_PIN_WAKEUP
	bl_ts_int_wakeup();
	#endif

	bl_log_trace("ctp_exit_sleep");
}

int ctp_bl_ts_init(liot_tp_handle_t handle)
{
	char ret = 0;
	liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;
	#ifdef BTL_CHECK_CHIPID
	unsigned char chipID = 0x00;
	#endif

	g_bl_tp_hw_cfg.i2c_ch = 1;
	g_bl_tp_hw_cfg.rst_pin = 28;
	g_bl_tp_hw_cfg.int_pin = 19;

	#ifdef RESET_PIN_WAKEUP
	Liot_GpioInitDirect(g_bl_tp_hw_cfg.rst_pin, L_IO_OUTPUT, L_IO_HIGH, NULL);
	bl_ts_reset_wakeup();
	#endif
	#ifdef INT_PIN_WAKEUP
	bl_ts_int_wakeup();
	#endif

	osDelay(20);
	bl_log_trace("ctp_bl_ts_init");
#ifdef BTL_CHECK_CHIPID
	SET_WAKEUP_LOW;
	bl_log_trace("ctp_bl_ts_init:Read chipID");
	ret = bl_get_chip_id(&chipID);
	SET_WAKEUP_HIGH;
	if((ret < 0) || (chipID != BTL_FLASH_ID))
	{
        bl_log_trace("ctp_bl_ts_init:Read chipID Fail:chipID = %x", chipID);
	    return -1;
	}
    else
    {
         bl_log_trace("ctp_bl_ts_init:Read chipID success:chipID = %x", chipID);
    }
#endif

#ifdef BL_AUTO_UPDATE_FARMWARE
	ret = bl_auto_update_fw();
	if(ret < 0)
	{
		bl_log_trace("ctp_bl_ts_init:Update error ret=%x", ret);
		return ret;
	}
#endif

    bl_get_rx_channel_num(&baseInfo.rx_channel);

	return ret;
}
