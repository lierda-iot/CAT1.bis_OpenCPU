#include "Tp_ts.h"
#include "bl_chip_custom.h"
#include "bl_fw.h"

#ifdef BL_FACTORY_SUPPORT
static struct btl_factory_test factoryTest;
#endif

static int bl_i2c_transfer(unsigned char i2c_addr, unsigned char *buf, int len, unsigned char rw)
{
	int ret = 0;
    switch(rw)
    {
        case I2C_WRITE:
			ret = CTP_FLASH_I2C_WRITE(i2c_addr, buf, len);
			break;
		case I2C_READ:
			ret = CTP_FLASH_I2C_READ(i2c_addr, buf, len);
			break;
    }
	if(!ret){
		bl_log_trace("bl_i2c_transfer:i2c transfer error___\n");
		return -1;
	}

	return 0;
}

static int bl_read_fw(unsigned char i2c_addr, unsigned char reg_addr, unsigned char *buf, int len)
{
	int ret;
	extern bl_tp_hw_config_t g_bl_tp_hw_cfg;
	uint8_t slave_7bit = i2c_addr;

	liot_errcode_i2c_e err = liot_I2cRead(g_bl_tp_hw_cfg.i2c_ch,
	                                       slave_7bit, reg_addr, buf, len);
	if (err != LIOT_I2C_SUCCESS) {
		bl_log_trace("bl_read_fw I2C read err: %d", err);
		return -1;
	}
	return 0;

	// ret = CTP_FLASH_I2C_WRITE(i2c_addr, &reg_addr, 1);
	// osDelay(1);
	// if(!ret)
	// {
	// 	goto IIC_COMM_ERROR;
	// }
	// ret = CTP_FLASH_I2C_READ(i2c_addr, buf, len);
	// if(!ret)
	// {
	// 	goto IIC_COMM_ERROR;
	// }

// IIC_COMM_ERROR:
// 	if(!ret){
// 		bl_log_trace("bl_read_fw:i2c transfer error___\n");
// 		return -1;
// 	}
// 	return 0;
}

#ifdef BL_DEBUG_NOISE
void bl_debug_for_touch(unsigned char* debugInfo)
{
	unsigned int i = 0;
	short freq =0x00;
	short maxNoise = 0x00;
	short minNoise = 0x00;
	//bl_log_trace("debugData start:\n");
	freq = (debugInfo[0]<<8)|debugInfo[1];
	maxNoise = (debugInfo[2]<<8)|debugInfo[3];
	minNoise = (debugInfo[4]<<8)|debugInfo[5];
	bl_log_trace("freq = %d maxNoise = %d minNoise = %d\n",freq,maxNoise,minNoise);
	//bl_log_trace("debugData end\n");
}
#endif

#ifdef BL_DEBUG_DIFF
void bl_print_diff_data(void)
{
    int i = 0;
    unsigned char data[2 * RX_NUM_MAX] = {0x00};
	
    bl_read_fw(CTP_SLAVE_ADDR, BTL_DIFF_REG, data, 2 * baseInfo.rx_channel);

	for(i = 0; i < (2 * baseInfo.rx_channel);)
    {
		bl_log_trace("%d", (short)(data[i+1]<<8) | data[i]);
		i = i + 2;
    }
	bl_log_trace("\n");
}

#endif

#ifdef BL_ESD_PROTECT
void bl_ts_esd_recovery(void)
{
    #ifdef BL_POWER_CONTROL_SUPPORT
	    bl_ts_reset_powerup();
	#else
	    #ifdef RESET_PIN_WAKEUP
		    bl_ts_reset_wakeup();
		#endif
        #ifdef INT_PIN_WAKEUP
		    bl_ts_int_wakeup();
        #endif
	#endif
}
void bl_esd_check_handler(void)
{
    int ret = 0;
    unsigned char buf[4] = {0x00};
	unsigned char curBuf[4] = {0x00};
    
	bl_log_trace("bl_esd_check_handler start\n");
    
    ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_ESD_REG, buf,sizeof(buf));
    if(ret < 0)
    {
    	bl_log_trace("i2c module abnormal need recovery!\n");
    	bl_ts_esd_recovery();
    }
    else
    {
        bl_log_trace("esd buf value %x %x %x %x!\n",buf[0], buf[1], buf[2], buf[3]);
		MDELAY(50);
        ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_ESD_REG, curBuf,sizeof(curBuf));
        if(ret < 0)
        {
            bl_log_trace("i2c module abnormal need recovery!\n");
            bl_ts_esd_recovery();
        }
		else
        {
            bl_log_trace("esd curBuf value %x %x %x %x!\n",curBuf[0], curBuf[1], curBuf[2], curBuf[3]);
            if(memcmp(curBuf, buf, sizeof(buf)) == 0)
            {
            	bl_log_trace("IC abnormal need recovery!\n");
            	bl_ts_esd_recovery();		
            }
        }
    }

	bl_log_trace("bl_esd_check_handler end\n");
}
#endif

int bl_soft_reset_switch_int_wakemode(void)
{
    unsigned char cmd[4];
	int ret = 0x00;

	cmd[0] = RW_REGISTER_CMD;
	cmd[1] = ~cmd[0];
	cmd[2] = CHIP_ID_REG;
    cmd[3] = 0xe8;
	
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd,4,I2C_WRITE);
	if(ret < 0){
		bl_log_trace("bl_soft_reset_switch_int_wakemode failed:i2c write flash error___\n");
	}

    return ret;
}

int bl_get_chip_id(unsigned char *buf)
{

	unsigned char cmd[3];
	int ret = 0x00;
	bl_log_trace("bl_get_chip_id\n");

    cmd[0] = RW_REGISTER_CMD;
    cmd[1] = ~cmd[0];
    cmd[2] = CHIP_ID_REG;
    
    ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR, cmd,3,I2C_WRITE);
    if(ret < 0){
    	bl_log_trace("bl_get_chip_id:i2c write flash error___\n");
    	goto GET_CHIP_ID_ERROR;
    }
    
    ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR, buf,1,I2C_READ); 
    if(ret < 0){
    	bl_log_trace("bl_get_chip_id:i2c read flash error___\n");
    	goto GET_CHIP_ID_ERROR;
    }
    
    bl_log_trace("bl_get_chip_id:buf = %x\n",*buf);

GET_CHIP_ID_ERROR:
	return ret;
}

static void bl_switch_protocol(void)
{
	int ret;
    unsigned char cmd[] = {'U', 'F', 'O'};	
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd,sizeof(cmd),I2C_WRITE);
	if(ret < 0){
		bl_log_trace("failed\n");
	}	
	MDELAY(50);	
}

#if((UPDATE_MODE == I2C_UPDATE_MODE_OLD) || (UPDATE_MODE == I2C_UPDATE_MODE_NEW))
#if(UPDATE_MODE == I2C_UPDATE_MODE_NEW)
void bl_enter_update_with_i2c(void)
{
    unsigned char buf[4] = {0x63, 0x75, 0x69, 0x33};
	unsigned char cmd[16] = {0};
	int i = 4;
	int ret = 0;

	for(i = 0; i < 4; i++) 
    {
        cmd[4 * i] = buf[0];
		cmd[4 * i + 1] = buf[1];
		cmd[4 * i + 2] = buf[2];
		cmd[4 * i + 3] = buf[3];
    }

	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd, sizeof(cmd), I2C_WRITE);
	if(ret < 0)
    {
        bl_log_trace("bl_enter_update_with_i2c failed:send i2c cmd error___\r\n");
        goto error;
    }
	MDELAY(50);

error:
	return ;
}
#endif

#if(UPDATE_MODE == I2C_UPDATE_MODE_OLD)
void bl_enter_update_with_i2c(void)
{
    unsigned char cmd[200] = {0x00};
	int i = 0;
	int ret = 0;

	for(i = 0; i < sizeof(cmd); i += 2)
    {
        cmd[i] = 0x5a;
		cmd[i + 1] = 0xa5;
    }

	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd, sizeof(cmd), I2C_WRITE);
	if(ret < 0)
    {
        bl_log_trace("bl_enter_update_with_i2c failed:send 5a a5 error___\n");
        goto error;
    }
	MDELAY(50);

error:
	return ;
}
#endif

void bl_exit_update_with_i2c(void)
{
    int ret = 0;
	unsigned char cmd[2] = {0x5a, 0xa5};
    MDELAY(20);
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd, sizeof(cmd), I2C_WRITE);
	if(ret < 0)
    {
        bl_log_trace("bl_exit_update_with_i2c failed:send 5a a5 error___\n");
    }

	MDELAY(20);
	#if defined(RESET_PIN_WAKEUP)
	bl_ts_reset_wakeup();
	MDELAY(30);
	#endif
	return;
}
#endif

#if(UPDATE_MODE == INT_UPDATE_MODE)
void bl_enter_update_with_int(void)
{
    bl_ts_set_intmode(0);
    bl_ts_set_intup(0);
	#if defined(RESET_PIN_WAKEUP)
	bl_ts_reset_wakeup();
    #endif
    bl_soft_reset_switch_int_wakemode();
	MDELAY(50);
}

void bl_exit_update_with_int(void)
{
    MDELAY(20);
	bl_ts_set_intup(1);
	MDELAY(20);
	bl_ts_set_intmode(1);
    #if defined(RESET_PIN_WAKEUP)
	bl_ts_reset_wakeup();
    #endif
}
#endif

#ifdef BL_FACTORY_SUPPORT
static int bl_enter_factory_mode(void)
{
    int ret = 0;
    unsigned char mode = 0;
	unsigned cmdWrite[2] = {0};
    int i = 0;
    int j = 0;

	cmdWrite[0] = WORK_MODE_REG;
	cmdWrite[1] = 0x40;

    ret = bl_read_fw(CTP_SLAVE_ADDR, WORK_MODE_REG, &mode, 1);
    if ((ret == 0) && (0x40 == (mode & 0x7f)))
        return 0;

    for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
        ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmdWrite, sizeof(cmdWrite), I2C_WRITE);
        if (ret >= 0) {
            MDELAY(FACTORY_TEST_DELAY);
            for (j = 0; j < 20; j++) {
                ret = bl_read_fw(CTP_SLAVE_ADDR, WORK_MODE_REG, &mode, 1);
                if ((ret == 0) && (0x40 == (mode & 0x7f))) {
                    bl_log_trace("enter factory mode success");
                    MDELAY(50);
                    return 0;
                } else
                    MDELAY(FACTORY_TEST_DELAY);
            }
        }

        MDELAY(50);
    }

    if (i >= ENTER_WORK_FACTORY_RETRIES) {
        bl_log_trace("Enter factory mode fail");
        return -1;
    }

    return 0;

}

static int bl_start_scan(void)
{
    int ret = 0;
    unsigned char cmdWrite[2] = {0};
	unsigned char val = 0;
    int times = 0;
	int j = 0;

	cmdWrite[0] = WORK_MODE_REG;
	cmdWrite[1] = 0xc0;

    while (times++ < FACTORY_SCAN_RETRIES) {
        ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmdWrite, sizeof(cmdWrite), I2C_WRITE); 
        if (ret == 0) {
            MDELAY(FACTORY_TEST_DELAY);
            for (j = 0; j < 20; j++) {
 				ret = bl_read_fw(CTP_SLAVE_ADDR, WORK_MODE_REG, &val, 1);
                if ((ret == 0) && ((val & 0x80)== 0x00)) {
                    bl_log_trace("scan success");
                    MDELAY(50);
                    return 0;
                } else
                    MDELAY(FACTORY_TEST_DELAY);
            }
        }

        MDELAY(50);
    }

    if (times >= FACTORY_SCAN_RETRIES) {
        bl_log_trace("scan timeout\n");
        return -1;
    }

    return 0;
}

static int bl_cali_cb(unsigned char mode)
{
    int ret = 0;
    int i = 0;
    int j = 0;
	unsigned char cmdWrite[2] = {0};

    cmdWrite[0] = BTL_CB_CALI_REG;
	cmdWrite[1] = mode;
	
    for (i = 0; i < FACTORY_CALI_RETRIES; i++) {
		ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmdWrite, sizeof(cmdWrite), I2C_WRITE);
        if (ret == 0) {
            MDELAY(FACTORY_TEST_DELAY);
            for (j = 0; j < 20; j++) {
 				ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_CB_CALI_REG, &mode, 1);
                if ((ret == 0) && (0x00 == mode)) {
                    bl_log_trace("calibrate success");
                    MDELAY(50);
                    return 0;
                } else
                    MDELAY(FACTORY_TEST_DELAY);
            }
        }

        MDELAY(50);
    }

    if (i >= FACTORY_CALI_RETRIES) {
        bl_log_trace("Enter factory mode fail");
        return -1;
    }

    return 0;
}

static int bl_select_scan_mode(unsigned char mode)
{
    int ret = 0;
    unsigned char cmdWrite[2] = {0};
	unsigned char val = 0;
    int times = 0;
	int j = 0;

	cmdWrite[0] = BTL_SET_SCAN_MODE_REG;
	cmdWrite[1] = mode;

    while (times++ < FACTORY_SCAN_RETRIES) {
        ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmdWrite, sizeof(cmdWrite), I2C_WRITE); 
        if (ret == 0) {
            MDELAY(FACTORY_TEST_DELAY);
            for (j = 0; j < 20; j++) {
 				ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_SET_SCAN_MODE_REG, &val, 1);
                if ((ret == 0) && (val == mode)) {
                    bl_log_trace("select scan mode success");
                    MDELAY(50);
                    return 0;
                } else
                    MDELAY(FACTORY_TEST_DELAY);
            }
        }

        MDELAY(50);
    }

    if (times >= FACTORY_SCAN_RETRIES) {
        bl_log_trace("select scan mode timeout\n");
        return -1;
    }

    return 0;
}


#ifdef RESET_PIN_WAKEUP
static int bl_factory_rst_test(void)
{
    int ret = 0;
	unsigned char originValue = 0;
	unsigned char cmdWrite[2] = {0};
	unsigned char afterValue = 0;
	
	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_RST_TEST_REG, &originValue, 1);
	if(ret < 0)
    {
        bl_log_trace("read rst reg error");
    	return -1;
    }

	cmdWrite[0] = BTL_RST_TEST_REG;
	cmdWrite[1] = originValue + 1;

	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmdWrite, sizeof(cmdWrite), I2C_WRITE);
	if(ret < 0)
    {
        bl_log_trace("read rst reg error");
    	return -1;
    }	

	bl_ts_reset_wakeup();
	MDELAY(100);
	
    ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_RST_TEST_REG, &afterValue, 1);
    if((ret < 0) || (originValue != afterValue))
    {
    	bl_log_trace("read rst reg error");
    	return -1;
    }
    return 0;
}
#endif

static int bl_factory_test_mode0(void)
{
    int ret = 0;
    int i = 0;
	unsigned char channel_num = baseInfo.rx_channel + baseInfo.key_channel;
	unsigned char rawData[2 * RX_NUM_MAX] = {0};
	
	ret = bl_enter_factory_mode();
	if(ret < 0)
    {
        return -1;
    }

    for(i = 0; i < 5; i++)
    {
        ret = bl_start_scan();
        if(ret < 0)
        {
            return -1;
        }
    }
	
	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_RAWDATA_REG, rawData, 2 * channel_num);
	if(ret < 0)
    {
        return -1;
    }

	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_CB_REG, factoryTest.CBMode0, channel_num);
	if(ret < 0)
    {
        return -1;
    }

    for(i = 0; i < (channel_num * 2); i = i + 2)
    {
		factoryTest.rawDataMode0[i >> 1] = (rawData[i+1] << 8) + rawData[i];
    }

	for (i = 0; i < channel_num; i++) {
		if ((factoryTest.rawDataMode0[i] < factoryTest.rawDataMode0Min[i]) || (factoryTest.rawDataMode0[i] > factoryTest.rawDataMode0Max[i])) {
			bl_log_trace("test fail,Rx%d=%5d,range=(%5d,%5d)\n", i + 1, factoryTest.rawDataMode0[i], factoryTest.rawDataMode0Min[i], factoryTest.rawDataMode0Max[i]);
			return -1;
		}
	}

    for(i = 0; i < channel_num; i++)
    {
		 if ((factoryTest.CBMode0[i] < factoryTest.CBMode0Min[i]) || (factoryTest.CBMode0[i][i] > factoryTest.CBMode0Max[i])) {
			bl_log_trace("test fail,%d=%5d,range=(%5d,%5d)\n", i + 1, factoryTest.CBMode0, factoryTest.CBMode0Min[i], factoryTest.CBMode0Max[i]);
			return -1;
		}      
    }
	return 0;
}

static int bl_factory_test_mode1(void)
{
    int ret = 0;
    int i = 0;
	unsigned char channel_num = baseInfo.rx_channel + baseInfo.key_channel;
	unsigned char rawData[2 * RX_NUM_MAX] = {0};
	
	ret = bl_enter_factory_mode();
	if(ret < 0)
    {
        return -1;
    }

    ret = bl_select_scan_mode(0x11);
	if(ret < 0)
    {
        return -1;
    }

    ret = bl_cali_cb(0x1);
	if(ret < 0)
    {
        return -1;
    }

    for(i = 0; i < 5; i++)
    {
        ret = bl_start_scan();
        if(ret < 0)
        {
            return -1;
        }
    }
	
	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_RAWDATA_REG, rawData, 2 * channel_num);
	if(ret < 0)
    {
        return -1;
    }

	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_CB_REG, factoryTest.CBMode1, channel_num);
	if(ret < 0)
    {
        return -1;
    }

    for(i = 0; i < (channel_num * 2); i = i + 2)
    {
		factoryTest.rawDataMode0[i >> 1] = (rawData[i+1] << 8) + rawData[i];
    }

	for (i = 0; i < channel_num; i++) {
		if ((factoryTest.rawDataMode1[i] < factoryTest.rawDataMode1Min[i]) || (factoryTest.rawDataMode1[i] > factoryTest.rawDataMode1Max[i])) {
			bl_log_trace("test fail,Rx%d=%5d,range=(%5d,%5d)\n", i + 1, factoryTest.rawDataMode1[i], factoryTest.rawDataMode1Min[i], factoryTest.rawDataMode1Max[i]);
			return -1;
		}
	}

    for(i = 0; i < channel_num; i++)
    {
		 if ((factoryTest.CBMode0[i] < factoryTest.CBMode0Min[i]) || (factoryTest.CBMode0[i][i] > factoryTest.CBMode0Max[i])) {
			bl_log_trace("test fail,%d=%5d,range=(%5d,%5d)\n", i + 1, factoryTest.CBMode0, factoryTest.CBMode0Min[i], factoryTest.CBMode0Max[i]);
			return -1;
		}      
    }
	return 0;

}

static int bl_factory_test_mode2(void)
{
    int ret = 0;
    int i = 0;
	unsigned char channel_num = baseInfo.rx_channel + baseInfo.key_channel;
	unsigned char rawData[2 * RX_NUM_MAX] = {0};
	
	ret = bl_enter_factory_mode();
	if(ret < 0)
    {
        return -1;
    }

    ret = bl_select_scan_mode(0x12);
	if(ret < 0)
    {
        return -1;
    }

    ret = bl_cali_cb(0x1);
	if(ret < 0)
    {
        return -1;
    }

    for(i = 0; i < 5; i++)
    {
        ret = bl_start_scan();
        if(ret < 0)
        {
            return -1;
        }
    }
	
	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_RAWDATA_REG, rawData, 2 * channel_num);
	if(ret < 0)
    {
        return -1;
    }

	ret = bl_read_fw(CTP_SLAVE_ADDR, BTL_CB_REG, factoryTest.CBMode2, channel_num);
	if(ret < 0)
    {
        return -1;
    }

    for(i = 0; i < (channel_num * 2); i = i + 2)
    {
		factoryTest.rawDataMode0[i >> 1] = (rawData[i+1] << 8) + rawData[i];
    }

	for (i = 0; i < channel_num; i++) {
		if ((factoryTest.rawDataMode2[i] < factoryTest.rawDataMode2Min[i]) || (factoryTest.rawDataMode2[i] > factoryTest.rawDataMode2Max[i])) {
			bl_log_trace("test fail,Rx%d=%5d,range=(%5d,%5d)\n", i + 1, factoryTest.rawDataMode2[i], factoryTest.rawDataMode2Min[i], factoryTest.rawDataMode2Max[i]);
			return -1;
		}
	}

    for(i = 0; i < channel_num; i++)
    {
		 if ((factoryTest.CBMode2[i] < factoryTest.CBMode2Min[i]) || (factoryTest.CBMode2[i][i] > factoryTest.CBMode2Max[i])) {
			bl_log_trace("test fail,%d=%5d,range=(%5d,%5d)\n", i + 1, factoryTest.CBMode2, factoryTest.CBMode2Min[i], factoryTest.CBMode2Max[i]);
			return -1;
		}      
    }
	return 0;
}



static bl_factory_thresh_hold_value_init(void)
{
    factoryTest.rawDataMode0Min[] = {/*��?D??����?3?2��?����y?Y*/};
    factoryTest.rawDataMode0Max[] = {/*��?D??����?3?2��?����y?Y*/};
	factoryTest.CBMode0Min[] = {/*��?D??����?3?2��?����y?Y*/};
	factoryTest.CBMode0Max[] = {/*��?D??����?3?2��?����y?Y*/};

    factoryTest.rawDataMode1Min[] = {/*��?D??����?3?2��?����y?Y*/};
    factoryTest.rawDataMode1Max[] = {/*��?D??����?3?2��?����y?Y*/};
	factoryTest.CBMode1Min[] = {/*��?D??����?3?2��?����y?Y*/};
	factoryTest.CBMode1Max[] = {/*��?D??����?3?2��?����y?Y*/};

	factoryTest.rawDataMode2Min[] = {/*��?D??����?3?2��?����y?Y*/};
    factoryTest.rawDataMode2Max[] = {/*��?D??����?3?2��?����y?Y*/};
	factoryTest.CBMode2Min[] = {/*��?D??����?3?2��?����y?Y*/};
	factoryTest.CBMode2Max[] = {/*��?D??����?3?2��?����y?Y*/};
}

static int bl_factory_test(void)
{
    int ret = 0;

	bl_log_trace("bl_factory_test start\n");
    bl_factory_thresh_hold_value_init();
	#ifdef RESET_PIN_WAKEUP
	bl_ts_reset_wakeup();
	ret = bl_factory_rst_test();
	if(ret < 0)
    {
        bl_log_trace("bl_factory_test:rst test failed\n");
        return -1;
    }
	#endif

	ret = bl_factory_test_mode0();
	if(ret < 0)
    {
        bl_log_trace("bl_factory_test:test mode0 test failed\n");
        return -1;
    }

    ret = bl_factory_test_mode1();
    if(ret < 0)
    {
        bl_log_trace("bl_factory_test:test mode1 test failed\n");
        return -1;
    }
    
    ret = bl_factory_test_mode2();
    if(ret < 0)
    {
        bl_log_trace("bl_factory_test:test mode2 test failed\n");
        return -1;
    }

    return 0;
}
#endif

int bl_get_prj_id(unsigned char *buf)
{
	bl_log_trace("bl_get_prj_id\n");
	return bl_read_fw(CTP_SLAVE_ADDR,BL_PRJ_ID_REG, buf, 1);    
}

int bl_get_fwArgPrj_id(unsigned char *buf)
{
	bl_log_trace("bl_get_fwArgPrj_id\n");
	return bl_read_fw(CTP_SLAVE_ADDR,BL_FWVER_PJ_ID_REG, buf, 3);
}

int bl_get_cob_id(unsigned char *buf)
{
	bl_log_trace("bl_get_cob_id\n");
	return bl_read_fw(CTP_SLAVE_ADDR,COB_ID_REG, buf, 6);
}

#if (CTP_TYPE == SELF_CTP)
void bl_get_rx_channel_num(unsigned char *rxNum)
{
    bl_read_fw(CTP_SLAVE_ADDR, BTL_CHANNEL_RX_REG, rxNum, 1);
}

void bl_get_key_channel_num(unsigned char *key_num)
{
	bl_read_fw(CTP_SLAVE_ADDR, BTL_CHANNEL_KEY_REG, key_num, 1);
}
#endif

#ifdef BL_UPDATE_FIRMWARE_ENABLE
static int bl_get_protect_flag(void)
{
    unsigned char ret = 0;
    unsigned char protectFlag = 0x00;
	bl_log_trace("bl_get_protect_flag\n");
	ret = bl_read_fw(CTP_SLAVE_ADDR,BL_PROTECT_REG, &protectFlag, 1);
	if(ret < 0)
    {
    	bl_log_trace("bl_get_protect_flag failed,ret = %x\n",ret);
		return 0;
    }
	if(protectFlag == 0x55)
    {
        bl_log_trace("bl_get_protect_flag:protectFlag = %x\n",protectFlag);
		return 1;
    }
	return 0;
}

static int bl_get_specific_argument_for_self_ctp(unsigned int *arguOffset, unsigned char *cobID, unsigned char* fw_data, unsigned int fw_size, unsigned char arguCount)
{
    unsigned char convertCobId[12] = {0x00};
    unsigned char i = 0;
    unsigned int cobArguAddr = fw_size - arguCount * BL_ARGUMENT_FLASH_SIZE;
    bl_log_trace("fw_size is %x\n", fw_size);
    bl_log_trace("arguCount is %d\n", arguCount);
    bl_log_trace("cobArguAddr is %x\n",cobArguAddr);
	
    for(i = 0; i < sizeof(convertCobId); i++)
    {		
        if(i%2)
        {
            convertCobId[i] = cobID[i/2] & 0x0f;
        }		
        else
        {
            convertCobId[i] = (cobID[i/2] & 0xf0) >> 4;
        }
        bl_log_trace("before convert:convertCobId[%d] is %x\n",i,convertCobId[i]);
        if(convertCobId[i] < 10)
        {
            convertCobId[i] = '0' + convertCobId[i];
        }
        else
        {
            convertCobId[i] = 'a' + convertCobId[i] - 10;
        }
        bl_log_trace("after convert:convertCobId[%d] is %x\n",i,convertCobId[i]);
    }
	
    bl_log_trace("convertCobId is:\n");
    for(i= 0; i < 12; i++)
    {
        bl_log_trace("%x  ", convertCobId[i]);
    }
    bl_log_trace("\n");
	
    for(i = 0; i < arguCount; i++)
    {
        if(memcmp(convertCobId, fw_data + cobArguAddr + i * BL_ARGUMENT_FLASH_SIZE + BL_COB_ID_OFFSET, 12))
        {
            bl_log_trace("This argu is not the specific argu\n");
        }
        else
        {
            *arguOffset = cobArguAddr + i * BL_ARGUMENT_FLASH_SIZE;
            bl_log_trace("This argu is the specific argu, and arguOffset is %x\n",*arguOffset);
            break;
        }
    }

	if(i == arguCount)
    {
        *arguOffset = BL_ARGUMENT_BASE_OFFSET;
        return -1;
    }
	else
    {
        return 0;
    }
}

static int bl_get_specific_argument_for_self_interactive_ctp(unsigned int *arguOffset, unsigned char prjID, unsigned char* fw_data, unsigned int fw_size, unsigned char arguCount)
{
    int i = 0;
	unsigned char binPrjID = 0x00;

	bl_log_trace("prjID = %d, fw_size = %x, arguCount = %d\n", prjID, fw_size, arguCount);
	for(i = 0; i < arguCount; i++)
    {
        binPrjID = fw_data[BL_ARGUMENT_BASE_OFFSET + i * MAX_FLASH_SIZE + BL_PROJECT_ID_OFFSET];
		bl_log_trace("i = %d, binPrjID = %d\n", i, binPrjID);
        if(prjID == binPrjID)
        {
            *arguOffset = i * MAX_FLASH_SIZE;
            break;
        }
        else
        {
            continue;
        }
    }

    if(i >= arguCount)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}


static unsigned char bl_get_argument_count_for_self_ctp(unsigned char* fw_data, unsigned int fw_size)
{
    unsigned char i = 0;
	unsigned addr = 0;
	addr = fw_size;
	bl_log_trace("addr is %x\n", addr);
	while(addr > (BL_ARGUMENT_BASE_OFFSET + BL_ARGUMENT_FLASH_SIZE))
    {
        addr = addr - BL_ARGUMENT_FLASH_SIZE;
        if(memcmp(fw_data+addr, ARGU_MARK, sizeof(ARGU_MARK) - 1))
        {
            bl_log_trace("arguMark found flow complete");
            break;		
        }
        else
        {
            i++;
            bl_log_trace("arguMark founded\n");
        }
    }
    bl_log_trace("The argument count is %d\n",i);
    return i;
}

static unsigned int bl_get_cob_project_down_size_arguCnt_for_self_ctp(unsigned char* fw_data,unsigned int fw_size, unsigned char *arguCnt)
{
    unsigned int downSize = 0;

	*arguCnt = bl_get_argument_count_for_self_ctp(fw_data,fw_size);
	downSize = fw_size - (*arguCnt) * BL_ARGUMENT_FLASH_SIZE - FLASH_PAGE_SIZE;
	return downSize;
}

static unsigned char bl_get_argument_count_for_self_interactive_ctp(unsigned char* fw_data, unsigned int fw_size)
{
    unsigned char i = 0;
    i = fw_size / MAX_FLASH_SIZE;
    bl_log_trace("The argument count is %d\n",i);
    return i;
}

static unsigned int bl_get_cob_project_down_size_arguCnt_for_interactive_ctp(unsigned char* fw_data,unsigned int fw_size, unsigned char *arguCnt)
{
    unsigned int downSize = 0;
    *arguCnt = bl_get_argument_count_for_self_interactive_ctp(fw_data,fw_size);
	downSize = MAX_FLASH_SIZE;
	return downSize;
}

static unsigned char bl_is_cob_project_for_self(unsigned char* fw_data, int fw_size)
{
    unsigned char arguKey[4] = {0xaa,0x55,0x09,0x09};
	unsigned char* pfw;

	pfw = fw_data+fw_size-4;
	
    if(fw_size%FLASH_PAGE_SIZE)
    {
        return 0;
    }
	else
    {
	    if(memcmp(arguKey,pfw,4))
	    {
	        return 0;
	    }
        else
        {
            return 1;
        }
    }
}

static unsigned char bl_is_cob_project_for_self_interactive(unsigned char* fw_data, int fw_size)
{
    if((fw_size > MAX_FLASH_SIZE) && ((fw_size % MAX_FLASH_SIZE)==0))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int bl_get_fw_checksum(unsigned short *fw_checksum)
{
	unsigned char buf[3];
	unsigned char checksum_ready = 0;
	int retry = 5;
	int ret = 0x00;

	bl_log_trace("bl_get_fw_checksum\n");

	buf[0] = CHECKSUM_CAL_REG;
	buf[1] = CHECKSUM_CAL;
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, buf,2,I2C_WRITE);
	if(ret < 0){
		bl_log_trace("bl_get_fw_checksum:write checksum cmd error___\n");
		return -1;
	}
	MDELAY(FW_CHECKSUM_DELAY_TIME);
	
	ret = bl_read_fw(CTP_SLAVE_ADDR,CHECKSUM_REG, buf, 3);
	if(ret < 0){
		bl_log_trace("bl_get_fw_checksum:read checksum error___\n");
		return -1;
	}

	checksum_ready = buf[0];

	while((retry--) && (checksum_ready != CHECKSUM_READY)){

		MDELAY(50);
		ret = bl_read_fw(CTP_SLAVE_ADDR,CHECKSUM_REG, buf, 3);
		if(ret < 0){
			bl_log_trace("bl_get_fw_checksum:read checksum error___\n");
			return -1;
		}

		checksum_ready = buf[0];
	}
	
	if(checksum_ready != CHECKSUM_READY){
		bl_log_trace("bl_get_fw_checksum:read checksum fail___\n");
		return -1;
	}
	*fw_checksum = (buf[1]<<8)+buf[2];

	return 0;
}

static void bl_get_fw_bin_checksum_for_self_ctp(unsigned char *fw_data,unsigned short *fw_bin_checksum, int fw_size, int specifyArgAddr)
{
	int i = 0;
	int temp_checksum = 0x0;

    for(i = 0; i < BL_ARGUMENT_BASE_OFFSET; i++)
    {
        temp_checksum += fw_data[i];
    }
    for(i = specifyArgAddr; i < specifyArgAddr + VERTIFY_START_OFFSET; i++)
    {
    	temp_checksum += fw_data[i];
    }
    for(i = specifyArgAddr + VERTIFY_START_OFFSET; i < specifyArgAddr + VERTIFY_START_OFFSET + 4; i++)
    {
    	temp_checksum += fw_data[i];
    }
    for(i = BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET + 4; i < fw_size; i++)
    {
    	temp_checksum += fw_data[i];
    }

    for(i = fw_size; i < MAX_FLASH_SIZE; i++)
    {
    	temp_checksum += 0xff;
    }
    
    *fw_bin_checksum = temp_checksum & 0xffff;
}

static void bl_get_fw_bin_checksum_for_self_interactive_ctp(unsigned char *fw_data,unsigned short *fw_bin_checksum, int fw_size, int specifyArgAddr)
{
	int i = 0;
	int temp_checksum = 0x0;

    for(i = specifyArgAddr; i < (specifyArgAddr + fw_size); i++)
    {
    	temp_checksum += fw_data[i];
    }    

	for(i = fw_size; i < MAX_FLASH_SIZE; i++)
    {
        temp_checksum += 0xff;
    }
    *fw_bin_checksum = temp_checksum & 0xffff;
}

static void bl_get_fw_bin_checksum_for_compatible_ctp(unsigned char *fw_data,unsigned short *fw_bin_checksum, int fw_size)
{
	int i = 0;
	int temp_checksum = 0x0;

    for(i = 0; i < fw_size; i++)
    {
    	temp_checksum += fw_data[i];
    }
    for(i = fw_size; i < MAX_FLASH_SIZE; i++)
    {
    	temp_checksum += 0xff;
    }
    
    *fw_bin_checksum = temp_checksum & 0xffff;
}

static int bl_erase_flash(void)
{
	unsigned char cmd[2];
	
	bl_log_trace("bl_erase_flash\n");
	
	cmd[0] = ERASE_ALL_MAIN_CMD; 
	cmd[1] = ~cmd[0];

	return bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd, 0x02,I2C_WRITE);	
}

static int bl_write_flash_no_blm18(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
	unsigned char cmd_buf[6+FLASH_WSIZE];
	unsigned int flash_end_addr;
	int ret;
		
	bl_log_trace("bl_write_flash_no_blm18\n");
	
	if(!len){
		bl_log_trace("___write flash len is 0x00,return___\n");
		return -1;	
	}

	flash_end_addr = flash_start_addr + len -1;

	if(flash_end_addr >= MAX_FLASH_SIZE){
		bl_log_trace("___write flash end addr is overflow,return___\n");
		return -1;	
	}

	cmd_buf[0] = cmd;
	cmd_buf[1] = ~cmd;
	cmd_buf[2] = flash_start_addr >> 0x08;
	cmd_buf[3] = flash_start_addr & 0xff;
	cmd_buf[4] = flash_end_addr >> 0x08;
	cmd_buf[5] = flash_end_addr & 0xff;

	memcpy(&cmd_buf[6],buf,len);

	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd_buf, len+6,I2C_WRITE);	
	if(ret < 0){
		bl_log_trace("i2c transfer error___\n");
		return -1;
	}

	return 0;
}

static int bl_write_flash_blm18(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
	unsigned char cmd_buf[8+FLASH_WSIZE];
	unsigned int flash_end_addr;
	int ret;
		
	bl_log_trace("bl_write_flash_blm18\n");
	
	if(!len){
		bl_log_trace("___write flash len is 0x00,return___\n");
		return -1;	
	}

	flash_end_addr = flash_start_addr + len -1;

	if(flash_end_addr >= MAX_FLASH_SIZE){
		bl_log_trace("___write flash end addr is overflow,return___\n");
		return -1;	
	}

	cmd_buf[0] = cmd;
	cmd_buf[1] = ~cmd;
	cmd_buf[2] = flash_start_addr >> 16;
	cmd_buf[3] = flash_start_addr >> 8;
	cmd_buf[4] = flash_start_addr & 0xff;
	cmd_buf[5] = flash_end_addr >> 16;
	cmd_buf[6] = flash_end_addr >> 8;
	cmd_buf[7] = flash_end_addr & 0xff;

	memcpy(&cmd_buf[8],buf,len);

	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd_buf, len+8,I2C_WRITE);	
	if(ret < 0){
		bl_log_trace("i2c transfer error___\n");
		return -1;
	}

	return 0;
}

static int bl_write_flash(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
    #if(TS_CHIP==BLM18)
		return bl_write_flash_blm18(cmd, flash_start_addr, buf, len);
	#else
	    return bl_write_flash_no_blm18(cmd, flash_start_addr, buf, len);
	#endif
}

static int bl_read_flash_no_blm18(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
	char ret =0;
	unsigned char cmd_buf[6];
	unsigned int flash_end_addr;

	flash_end_addr = flash_start_addr + len -1;
	cmd_buf[0] = cmd;
	cmd_buf[1] = ~cmd;
	cmd_buf[2] = flash_start_addr >> 0x08;
	cmd_buf[3] = flash_start_addr & 0xff;
	cmd_buf[4] = flash_end_addr >> 0x08;
	cmd_buf[5] = flash_end_addr & 0xff;
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd_buf,6,I2C_WRITE);
	if(ret < 0)
	{
	    bl_log_trace("bl_read_flash_no_blm18:i2c transfer write error\n");
		return -1;
	}
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,buf,len,I2C_READ);
	if(ret < 0)
	{
	    bl_log_trace("bl_read_flash_no_blm18:i2c transfer read error\n");
		return -1;
	}

	return 0;
}

static int bl_read_flash_blm18(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
	char ret =0;
	unsigned char cmd_buf[6];
	unsigned int flash_end_addr;

	flash_end_addr = flash_start_addr + len -1;
	cmd_buf[0] = cmd;
	cmd_buf[1] = ~cmd;
	cmd_buf[2] = flash_start_addr >> 16;
	cmd_buf[3] = flash_start_addr >> 8;
	cmd_buf[4] = flash_start_addr & 0xff;
	cmd_buf[5] = flash_end_addr >> 16;
	cmd_buf[6] = flash_end_addr >> 8;
	cmd_buf[7] = flash_end_addr & 0xff;
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd_buf,8,I2C_WRITE);
	if(ret < 0)
	{
	    bl_log_trace("bl_read_flash_blm18:i2c transfer write error\n");
		return -1;
	}
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,buf,len,I2C_READ);
	if(ret < 0)
	{
	    bl_log_trace("bl_read_flash_blm18:i2c transfer read error\n");
		return -1;
	}

	return 0;
}

static int bl_read_flash(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
    #if(TS_CHIP==BLM18)
	    return bl_read_flash_blm18(cmd, flash_start_addr, buf, len);
	#else
	    return bl_read_flash_no_blm18(cmd, flash_start_addr, buf, len);
	#endif
}
static int bl_download_fw_for_self_ctp(unsigned char *pfwbin,int specificArgAddr, int fwsize)
{
	unsigned int i;
	unsigned int size,len;
	unsigned int addr;
    unsigned char verifyBuf[4] = {0xff, 0xff, 0xff, 0xff};
	bl_log_trace("bl_download_fw_for_self_ctp\n");
	
	verifyBuf[2] = pfwbin[BL_ARGUMENT_BASE_OFFSET+VERTIFY_START_OFFSET+2];
	verifyBuf[3] = pfwbin[BL_ARGUMENT_BASE_OFFSET+VERTIFY_START_OFFSET+3];	
	bl_log_trace("bl_download_fw:verifyBuf = %x %x %x %x\n",verifyBuf[0],verifyBuf[1],verifyBuf[2],verifyBuf[3]);
	if(bl_erase_flash()){
		bl_log_trace("___erase flash fail___\n");
		return -1;
	}

	MDELAY(50);

    //Write data before BL_ARGUMENT_BASE_OFFSET
	for(i=0;i< BL_ARGUMENT_BASE_OFFSET;)
	{
		size = BL_ARGUMENT_BASE_OFFSET - i;
		if(size > FLASH_WSIZE){
			len = FLASH_WSIZE;
		}else{
			len = size;
		}

		addr = i;
	
		if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i],len)){
			return -1;
		}
		i += len;
		MDELAY(5);
	}

    //Write the data from BL_ARGUMENT_BASE_OFFSET to VERTIFY_START_OFFSET
    for(i=BL_ARGUMENT_BASE_OFFSET;i< (VERTIFY_START_OFFSET+BL_ARGUMENT_BASE_OFFSET);)
    {
    	size = VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i+specificArgAddr-BL_ARGUMENT_BASE_OFFSET],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

    //Write the four bytes verifyBuf from VERTIFY_START_OFFSET
    for(i=(VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET);i< (VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET + sizeof(verifyBuf));)
    {
    	size = VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET + sizeof(verifyBuf) - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &verifyBuf[i-VERTIFY_START_OFFSET-BL_ARGUMENT_BASE_OFFSET],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

    //Write data after verityBuf from VERTIFY_START_OFFSET + 4
    for(i=(BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET + 4);i< fwsize;)
    {
    	size = fwsize - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

	return 0;	
}

static int bl_download_fw_for_self_interactive_ctp(unsigned char *pfwbin,int specificArgAddr, int fwsize)
{
	unsigned int i;
	unsigned int size,len;
	unsigned int addr;
    unsigned char verifyBuf[4] = {0xff, 0xff, 0xff, 0xff};
	bl_log_trace("bl_download_fw_for_self_interactive_ctp\n");
	
	verifyBuf[2] = pfwbin[specificArgAddr + BL_ARGUMENT_BASE_OFFSET+VERTIFY_START_OFFSET+2];
	verifyBuf[3] = pfwbin[specificArgAddr + BL_ARGUMENT_BASE_OFFSET+VERTIFY_START_OFFSET+3];	
	bl_log_trace("bl_download_fw_for_self_ctp:verifyBuf = %x %x %x %x\n",verifyBuf[0],verifyBuf[1],verifyBuf[2],verifyBuf[3]);
	if(bl_erase_flash()){
		bl_log_trace("___erase flash fail___\n");
		return -1;
	}

	MDELAY(50);

    //Write data before BL_ARGUMENT_BASE_OFFSET
	for(i=0;i< (VERTIFY_START_OFFSET+BL_ARGUMENT_BASE_OFFSET);)
	{
		size = BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET - i;
		if(size > FLASH_WSIZE){
			len = FLASH_WSIZE;
		}else{
			len = size;
		}

		addr = i;
	
		if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i+specificArgAddr],len)){
			return -1;
		}
		i += len;
		MDELAY(5);
	}

    //Write the four bytes verifyBuf from VERTIFY_START_OFFSET
    for(i=(VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET);i< (VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET + sizeof(verifyBuf));)
    {
    	size = VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET + sizeof(verifyBuf) - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &verifyBuf[i-VERTIFY_START_OFFSET-BL_ARGUMENT_BASE_OFFSET],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

    //Write data after verityBuf from VERTIFY_START_OFFSET + 4
    for(i=(BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET + 4);i < fwsize;)
    {
    	size = fwsize - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i+specificArgAddr],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

	return 0;	
}

static int bl_download_fw_for_compatible_ctp(unsigned char *pfwbin, int fwsize)
{
	unsigned int i;
	unsigned int size,len;
	unsigned int addr;
	bl_log_trace("bl_download_fw_for_compatible_ctp\n");
	
	if(bl_erase_flash()){
		bl_log_trace("___erase flash fail___\n");
		return -1;
	}

	MDELAY(50);

    for(i = 0;i < fwsize;)
    {
    	size = fwsize - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

	return 0;	
}

static int bl_read_flash_vertify(unsigned char *pfwbin)
{
	unsigned char cnt = 0;
	int ret = 0;
	unsigned char vertify[2] = {0};
	unsigned char vertify1[2] = {0};
	
	memcpy(vertify,&pfwbin[BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET],sizeof(vertify));
	bl_log_trace("bl_read_flash_vertify: vertify:%x %x\n",vertify[0],vertify[1]);

	SET_WAKEUP_LOW;
	while(cnt < 3)
	{
	    cnt++;
        ret = bl_read_flash(READ_MAIN_CMD, BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET, vertify1, sizeof(vertify1));
		if(ret < 0)
		{
			bl_log_trace("bl_write_flash_vertify: read fail\n");
			continue;
		}

		if(memcmp(vertify, vertify1, sizeof(vertify)) == 0)
		{
			ret = 0;
			break;
		}
		else
        {
            ret = -1;
        }
	}
	SET_WAKEUP_HIGH;
	return ret;
}

static int bl_write_flash_vertify(unsigned char *pfwbin)
{
	unsigned char cnt = 0;
	int ret = 0;
	unsigned char vertify[2] = {0};
	unsigned char vertify1[2] = {0};

	memcpy(vertify,&pfwbin[BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET],sizeof(vertify));
	bl_log_trace("bl_write_flash_vertify: vertify:%x %x\n",vertify[0],vertify[1]);

	SET_WAKEUP_LOW;
	while(cnt < 3)
	{
	    cnt++;
	    ret = bl_write_flash(WRITE_MAIN_CMD, BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET, vertify, sizeof(vertify));
		if(ret < 0)
		{
			bl_log_trace("bl_write_flash_vertify: write fail\n");
			continue;
		}
		
		MDELAY(10);

        ret = bl_read_flash(READ_MAIN_CMD, BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET, vertify1, sizeof(vertify1));
		if(ret < 0)
		{
			bl_log_trace("bl_write_flash_vertify: read fail\n");
			continue;
		}

		if(memcmp(vertify, vertify1, sizeof(vertify)) == 0)
		{
			ret = 0;
			break;
		}
		else
        {
            ret = -1;
        }
	}
	SET_WAKEUP_HIGH;
	return ret;
}

static int bl_update_flash_for_self_ctp(unsigned char update_type, unsigned char *pfwbin,int fwsize, int specificArgAddr)
{
	int retry = 0;
	int ret = 0;
	unsigned short fw_checksum = 0x0;
	unsigned short fw_bin_checksum = 0x0;
	retry =3;
	while(retry--)
	{
		SET_WAKEUP_LOW;

	    ret = bl_download_fw_for_self_ctp(pfwbin,specificArgAddr,fwsize);

		if(ret<0)
		{
			bl_log_trace("bl_update_flash_for_self_ctp:bl_download_fw_for_self_ctp error retry=%d\n",retry);
			continue;
		}
		
		MDELAY(50);

		SET_WAKEUP_HIGH;
	
	    bl_get_fw_bin_checksum_for_self_ctp(pfwbin,&fw_bin_checksum, fwsize,specificArgAddr);
	    ret = bl_get_fw_checksum(&fw_checksum);
		fw_checksum -= 0xff;
		bl_log_trace("bl_download_fw_for_self_ctp:fw checksum = 0x%x,fw_bin_checksum =0x%x\n",fw_checksum, fw_bin_checksum);

		
		if((ret < 0) || ((update_type == FW_ARG_UPDATE)&&(fw_checksum != fw_bin_checksum)))
		{
			bl_log_trace("bl_download_fw_for_self_ctp:bl_get_fw_checksum error");
			continue;
		}

		if((update_type == FW_ARG_UPDATE)&&(fw_checksum == fw_bin_checksum))
		{
            ret = bl_write_flash_vertify(pfwbin);
			if(ret < 0)
				continue;
		}
		break;
	}

	if(retry < 0)
	{
		bl_log_trace("bl_download_fw_for_self_ctp error\n");
		return -1;
	}

	bl_log_trace("bl_download_fw_for_self_ctp success___\n");	

	return 0;
}

static int bl_update_flash_for_self_interactive_ctp(unsigned char update_type, unsigned char *pfwbin,int fwsize, int specificArgAddr)
{
	int retry = 0;
	int ret = 0;
	unsigned short fw_checksum = 0x0;
	unsigned short fw_bin_checksum = 0x0;
	retry =3;
	while(retry--)
	{
		SET_WAKEUP_LOW;

	    ret = bl_download_fw_for_self_interactive_ctp(pfwbin,specificArgAddr,fwsize);

		if(ret<0)
		{
			bl_log_trace("bl_update_flash_for_self_interactive_ctp:bl_download_fw_for_self_interactive_ctp error retry=%d\n",retry);
			continue;
		}
		
		MDELAY(50);

		SET_WAKEUP_HIGH;
	
	    bl_get_fw_bin_checksum_for_self_interactive_ctp(pfwbin,&fw_bin_checksum, fwsize,specificArgAddr);
	    ret = bl_get_fw_checksum(&fw_checksum);
		fw_checksum -= 0xff;
		bl_log_trace("bl_update_flash_for_self_interactive_ctp:fw checksum = 0x%x,fw_bin_checksum =0x%x\n",fw_checksum, fw_bin_checksum);

		
		if((ret < 0) || ((update_type == FW_ARG_UPDATE)&&(fw_checksum != fw_bin_checksum)))
		{
			bl_log_trace("bl_update_flash_for_self_interactive_ctp:bl_get_fw_checksum error");
			continue;
		}

		if((update_type == FW_ARG_UPDATE)&&(fw_checksum == fw_bin_checksum))
		{
            ret = bl_write_flash_vertify(pfwbin);
			if(ret < 0)
				continue;
		}
		break;
	}

	if(retry < 0)
	{
		bl_log_trace("bl_update_flash_for_self_interactive_ctp error\n");
		return -1;
	}

	bl_log_trace("bl_update_flash_for_self_interactive_ctp success___\n");	

	return 0;
}

static int bl_update_flash_for_copatible_ctp(unsigned char update_type, unsigned char *pfwbin,int fwsize)
{
	int retry = 0;
	int ret = 0;
	unsigned short fw_checksum = 0x0;
	unsigned short fw_bin_checksum = 0x0;
	retry =3;
	while(retry--)
	{
		SET_WAKEUP_LOW;

	    ret = bl_download_fw_for_compatible_ctp(pfwbin, fwsize);

		if(ret<0)
		{
			bl_log_trace("bl fw update start bl_download_fw error retry=%d\n",retry);
			continue;
		}
		
		MDELAY(50);

		SET_WAKEUP_HIGH;		

	    bl_get_fw_bin_checksum_for_compatible_ctp(pfwbin,&fw_bin_checksum, fwsize);
	    ret = bl_get_fw_checksum(&fw_checksum);
		bl_log_trace("bl fw update end,fw checksum = 0x%x,fw_bin_checksum =0x%x\n",fw_checksum, fw_bin_checksum);
		
		if((ret < 0) || ((update_type == FW_ARG_UPDATE)&&(fw_checksum != fw_bin_checksum)))
		{
			bl_log_trace("bl fw update start bl_download_fw bl_get_fw_checksum error");
			continue;
		}

		break;
	}

	if(retry < 0)
	{
		bl_log_trace("bl fw update error\n");
		return -1;
	}

	bl_log_trace("bl fw update success___\n");	

	return 0;
}

static unsigned char choose_update_type_for_self_ctp(unsigned char isBlank, unsigned char* fw_data, unsigned char fwVer, unsigned char arguVer, unsigned short fwChecksum, unsigned short fwBinChecksum,int specifyArgAddr)
{
    unsigned char update_type = NONE_UPDATE;
    if(isBlank)
	{
		update_type = FW_ARG_UPDATE;
		bl_log_trace("Update case 0:FW_ARG_UPDATE\n");
	}
	else
	{
        if((fwVer != fw_data[specifyArgAddr + BL_FWVER_MAIN_OFFSET])
		    ||(arguVer != fw_data[specifyArgAddr + BL_FWVER_ARGU_OFFSET])
		    ||(fwChecksum != fwBinChecksum))
	    {
            update_type = FW_ARG_UPDATE;
		    bl_log_trace("Update case 1:FW_ARG_UPDATE\n");
	    }
	    else
	    {
            update_type = NONE_UPDATE;
            bl_log_trace("Update case 4:NONE_UPDATE\n");
	    }
	}
    return update_type;
}

static unsigned char choose_update_type_for_self_interactive_ctp(unsigned char isBlank, unsigned char* fw_data, unsigned char fwVer, unsigned char arguVer, unsigned short fwChecksum, unsigned short fwBinChecksum,int specifyArgAddr)
{
    unsigned char update_type = NONE_UPDATE;
    if(isBlank)
	{
		update_type = FW_ARG_UPDATE;
		bl_log_trace("Update case 0:FW_ARG_UPDATE\n");
	}
	else
	{
        if((fwVer != fw_data[specifyArgAddr + BL_ARGUMENT_BASE_OFFSET + BL_FWVER_MAIN_OFFSET])
		    ||(arguVer != fw_data[specifyArgAddr + BL_ARGUMENT_BASE_OFFSET + BL_FWVER_ARGU_OFFSET])
		    ||(fwChecksum != fwBinChecksum))
	    {
            update_type = FW_ARG_UPDATE;
		    bl_log_trace("Update case 1:FW_ARG_UPDATE\n");
	    }
	    else
	    {
            update_type = NONE_UPDATE;
            bl_log_trace("Update case 4:NONE_UPDATE\n");
	    }
	}
    return update_type;
}

static unsigned char choose_update_type_for_compatible_ctp(unsigned isBlank, unsigned char* fw_data, unsigned char prjID, unsigned short checksum)
{
    unsigned char update_type = NONE_UPDATE;
    if(isBlank)
	{
		update_type = FW_ARG_UPDATE;
		bl_log_trace("Update case 0:FW_ARG_UPDATE\n");
	}
	else
	{
        if((prjID != fw_data[PJ_ID_OFFSET])||(checksum == 0))
	    {
            update_type = FW_ARG_UPDATE;
		    bl_log_trace("Update case 1:FW_ARG_UPDATE\n");
	    }
	    else
	    {
            update_type = NONE_UPDATE;
            bl_log_trace("Update case 4:NONE_UPDATE\n");
	    }
	}
    return update_type;
}

static int bl_update_fw_for_self_ctp(unsigned char fileType,unsigned char* fw_data, int fw_size)
{
	unsigned char fwArgPrjID[3];    //firmware version/argument version/project identification
	int ret = 0x00;
	unsigned char isBlank = 0x0;    //Indicate the IC have any firmware
	unsigned short fw_checksum = 0x0;  //The checksum for firmware in IC
	unsigned short fw_bin_checksum = 0x0;  //The checksum for firmware in file
	unsigned char update_type = NONE_UPDATE;  
	unsigned int downSize = 0x0;       //The available size of firmware data in file 
	unsigned char cobID[6] = {0};           //The identification for COB project
	unsigned int specificArguAddr = BL_ARGUMENT_BASE_OFFSET;   //The specific argument base address in firmware date with cobID 
	unsigned char arguCount = 0x0;      //The argument count for COB firmware
	unsigned char IsCobPrj = 0;         //Judge the project type depend firmware file
	unsigned char projectFlag = 0;      //protect flag
    bl_log_trace("bl_update_fw_for_self_ctp start\n");	

//check protect flag
    projectFlag = bl_get_protect_flag();
    if(bl_get_protect_flag() && (fileType == HEADER_FILE_UPDATE))
    {
        bl_log_trace("bl_update_fw_for_self_ctp:projectFlag = %x, fileType = %x", projectFlag, fileType);
		return 0;
    }
	
//Step 1:Obtain project type
    IsCobPrj = bl_is_cob_project_for_self(fw_data, fw_size);
    bl_log_trace("bl_update_fw_for_self_ctp:IsCobPrj = %x", IsCobPrj);

//Step 2:Obtain IC version number
	MDELAY(5);
	ret = bl_get_fwArgPrj_id(fwArgPrjID);
	if((ret < 0)
		|| (fileType == BIN_FILE_UPDATE)
		|| ((ret == 0) && (fwArgPrjID[0] == 0))
		|| ((ret == 0) && (fwArgPrjID[0] == 0xff)) 
		|| ((ret == 0) && (fwArgPrjID[0] == BL_FWVER_PJ_ID_REG))
		|| (bl_read_flash_vertify(fw_data) < 0))
	{
	    isBlank = 1;
		bl_log_trace("bl_update_fw_for_self_ctp:This is blank IC ret = %x fwArgPrjID[0]=%x fwArgPrjID[1]=%x fwArgPrjID[2]=%x\n",ret,fwArgPrjID[0],fwArgPrjID[1], fwArgPrjID[2]);
    }
	else
    {
        isBlank = 0;
		bl_log_trace("bl_update_fw_for_self_ctp:ret=%x fwID=%x argID=%x prjID=%x\n",ret,fwArgPrjID[0],fwArgPrjID[1], fwArgPrjID[2]);
    }
    bl_log_trace("bl_update_fw_for_self_ctp:isBlank = %x\n",isBlank);
	
//Step 3:Specify download size
    if(IsCobPrj)
    {
    	downSize = bl_get_cob_project_down_size_arguCnt_for_self_ctp(fw_data,fw_size,&arguCount);
    	bl_log_trace("bl_update_fw_for_self_ctp:downSize = %x,arguCount = %x\n",downSize,arguCount);
    }
    else
    {
    	downSize = fw_size;
    	bl_log_trace("bl_update_fw_for_self_ctp:downSize = %x\n",downSize);
    }

UPDATE_SECOND_FOR_COB:
//Step 4:Update the fwArgPrjID
    if(!isBlank)
    {
        bl_get_fwArgPrj_id(fwArgPrjID);
    }
	
//Step 5:Specify the argument data for cob project
    if(IsCobPrj && !isBlank)
    {
        MDELAY(50);
        ret = bl_get_cob_id(cobID);
		if(ret < 0)
        {
            bl_log_trace("bl_update_fw_for_self_ctp:bl_get_cob_id error\n");
			ret = -1;
			goto UPDATE_ERROR;
        }
		else
		{
		    bl_log_trace("bl_update_fw_for_self_ctp:cobID = %x %x %x %x %x %x\n",cobID[0],cobID[1],cobID[2],cobID[3],cobID[4],cobID[5]);
		}
		ret = bl_get_specific_argument_for_self_ctp(&specificArguAddr,cobID,fw_data,fw_size,arguCount);
		if(ret < 0)
		{
            bl_log_trace("Can't found argument for CTP module,use default argu:\n");
		}
		bl_log_trace("bl_update_fw_for_self_ctp:specificArguAddr = %x\n",specificArguAddr);
       }
	
	bl_log_trace("fw_data[] = %x  fw_data[] = %x", fw_data[BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET],fw_data[BL_ARGUMENT_BASE_OFFSET + VERTIFY_END_OFFSET]);
	
//Step 6:Specify whether switch the touch
    bl_log_trace("isBlank = %d  ver1 = %d ver2 = %d binVer1 = %d binVer2 = %d specificAddr = %x", isBlank,fwArgPrjID[0],fwArgPrjID[1],fw_data[specificArguAddr+BL_FWVER_MAIN_OFFSET],fw_data[specificArguAddr + BL_FWVER_ARGU_OFFSET], specificArguAddr);
    if(!isBlank)
	{
		bl_get_fw_bin_checksum_for_self_ctp(fw_data, &fw_bin_checksum, downSize, specificArguAddr);
		ret = bl_get_fw_checksum(&fw_checksum);
		if((ret < 0) || (fw_checksum != fw_bin_checksum)){
			bl_log_trace("bl_update_fw_for_self_ctp:Read checksum fail fw_checksum = %x\n",fw_checksum);
			fw_checksum = 0x00;
		}
		bl_log_trace("bl_update_fw_for_self_ctp:fw_checksum = 0x%x,fw_bin_checksum = 0x%x___\n",fw_checksum, fw_bin_checksum);
    }

//Step 7:Select update fw+arg or only update arg
    update_type = choose_update_type_for_self_ctp(isBlank, fw_data, fwArgPrjID[0], fwArgPrjID[1], fw_checksum, fw_bin_checksum,specificArguAddr);

//Step 8:Start Update depend condition
    if(update_type != NONE_UPDATE)
    {
        ret = bl_update_flash_for_self_ctp(update_type, fw_data, downSize,specificArguAddr);
        if(ret < 0)
        {
            bl_log_trace("bl_update_fw_for_self_ctp:bl_update_flash failed\n");
			goto UPDATE_ERROR;
        }
    }
	 
//Step 9:Execute second update flow when project firmware is cob and last update_type is FW_ARG_UPDATE
    if((ret == 0)&&(IsCobPrj)&&(isBlank))
    {
        isBlank = 0;
        bl_log_trace("bl_update_fw_for_self_ctp:bl_update_flash for COB project need second update with blank IC:isBlank = %d\n",isBlank);
        goto UPDATE_SECOND_FOR_COB;
    }
	bl_log_trace("bl_update_fw_for_self_ctp exit\n");

UPDATE_ERROR:
	return ret;
}

static int bl_update_fw_for_self_interactive_ctp(unsigned char fileType,unsigned char* fw_data, int fw_size)
{
	unsigned char fwArgPrjID[3];    //firmware version/argument version/project identification
	int ret = 0x00;
	unsigned char isBlank = 0x0;    //Indicate the IC have any firmware
	unsigned short fw_checksum = 0x0;  //The checksum for firmware in IC
	unsigned short fw_bin_checksum = 0x0;  //The checksum for firmware in file
	unsigned char update_type = NONE_UPDATE;  
	unsigned int downSize = 0x0;       //The available size of firmware data in file 
	unsigned char prjID = {0};           //The identification for COB project
	unsigned int specificArguAddr = 0;   //The specific argument base address in firmware date with cobID 
	unsigned char arguCount = 0x0;      //The argument count for COB firmware
	unsigned char IsCobPrj = 0;         //Judge the project type depend firmware file
	unsigned char projectFlag = 0;      //protect flag
    bl_log_trace("bl_update_fw_for_self_interactive_ctp start\n");

//check protect flag
    projectFlag = bl_get_protect_flag();
    if(projectFlag && (fileType == HEADER_FILE_UPDATE))
    {
        bl_log_trace("bl_update_fw_for_self_interactive_ctp:projectFlag = %x, fileType = %x", projectFlag, fileType);
		return 0;
    }
	
//Step 1:Obtain project type
    IsCobPrj = bl_is_cob_project_for_self_interactive(fw_data, fw_size);
    bl_log_trace("bl_update_fw_for_self_interactive_ctp:IsCobPrj = %x", IsCobPrj);

//Step 2:Obtain IC version number
	MDELAY(5);
	ret = bl_get_fwArgPrj_id(fwArgPrjID);
	if((ret < 0)
		|| (fileType == BIN_FILE_UPDATE)
		|| ((ret == 0) && (fwArgPrjID[0] == 0))
		|| ((ret == 0) && (fwArgPrjID[0] == 0xff)) 
		|| ((ret == 0) && (fwArgPrjID[0] == BL_FWVER_PJ_ID_REG))
		|| (bl_read_flash_vertify(fw_data) < 0))
	{
	    isBlank = 1;
		bl_log_trace("bl_update_fw_for_self_interactive_ctp:This is blank IC ret = %x fwArgPrjID[0]=%x fwArgPrjID[1]=%x fwArgPrjID[2]=%x\n",ret,fwArgPrjID[0],fwArgPrjID[1], fwArgPrjID[2]);
    }
	else
    {
        isBlank = 0;
		bl_log_trace("bl_update_fw_for_self_interactive_ctp:ret=%x fwID=%x argID=%x prjID=%x\n",ret,fwArgPrjID[0],fwArgPrjID[1], fwArgPrjID[2]);
    }
    bl_log_trace("bl_update_fw_for_self_interactive_ctp:isBlank = %x\n",isBlank);
	
//Step 3:Specify download size
    if(IsCobPrj)
    {
    	downSize = bl_get_cob_project_down_size_arguCnt_for_interactive_ctp(fw_data,fw_size,&arguCount);
    	bl_log_trace("bl_update_fw_for_self_interactive_ctp:downSize = %x,arguCount = %x\n",downSize,arguCount);
    }
    else
    {
    	downSize = fw_size;
    	bl_log_trace("bl_update_fw_for_self_interactive_ctp:downSize = %x\n",downSize);
    }

UPDATE_SECOND_FOR_COB:
//Step 4:Update the fwArgPrjID
    if(!isBlank)
    {
        bl_get_fwArgPrj_id(fwArgPrjID);
    }
	
//Step 5:Specify the argument data for cob project
    if(IsCobPrj && !isBlank)
    {
        MDELAY(50);
        ret = bl_get_prj_id(&prjID);
		if(ret < 0)
        {
            bl_log_trace("bl_update_fw_for_self_interactive_ctp:bl_get_prj_id error\n");
			ret = -1;
			goto UPDATE_ERROR;
        }
		else
		{
		    bl_log_trace("bl_update_fw_for_self_interactive_ctp:prjID = %x\n",prjID);
		}
		ret = bl_get_specific_argument_for_self_interactive_ctp(&specificArguAddr,prjID,fw_data,fw_size,arguCount);
		if(ret < 0)
		{
            bl_log_trace("Can't found argument for CTP module,use default argu:\n");
		}
		bl_log_trace("bl_update_fw_for_self_interactive_ctp:specificArguAddr = %x\n",specificArguAddr);
       }
		
//Step 6:Specify whether switch the touch
    bl_log_trace("isBlank = %d  ver1 = %d ver2 = %d binVer1 = %d binVer2 = %d specificAddr = %x", isBlank,fwArgPrjID[0],fwArgPrjID[1],fw_data[specificArguAddr+BL_ARGUMENT_BASE_OFFSET+BL_FWVER_MAIN_OFFSET],fw_data[specificArguAddr + BL_ARGUMENT_BASE_OFFSET + BL_FWVER_ARGU_OFFSET], specificArguAddr);
    if(!isBlank)
	{
		bl_get_fw_bin_checksum_for_self_interactive_ctp(fw_data, &fw_bin_checksum, downSize, specificArguAddr);
		ret = bl_get_fw_checksum(&fw_checksum);
		if((ret < 0) || (fw_checksum != fw_bin_checksum)){
			bl_log_trace("bl_update_fw_for_self_interactive_ctp:Read checksum fail fw_checksum = %x\n",fw_checksum);
			fw_checksum = 0x00;
		}
		bl_log_trace("bl_update_fw_for_self_interactive_ctp:fw_checksum = 0x%x,fw_bin_checksum = 0x%x___\n",fw_checksum, fw_bin_checksum);
    }

//Step 7:Select update fw+arg or only update arg
    update_type = choose_update_type_for_self_interactive_ctp(isBlank, fw_data, fwArgPrjID[0], fwArgPrjID[1], fw_checksum, fw_bin_checksum,specificArguAddr);

//Step 8:Start Update depend condition
    if(update_type != NONE_UPDATE)
    {
        ret = bl_update_flash_for_self_interactive_ctp(update_type, fw_data, downSize,specificArguAddr);
        if(ret < 0)
        {
            bl_log_trace("bl_update_fw_for_self_interactive_ctp:bl_update_flash failed\n");
			goto UPDATE_ERROR;
        }
    }
	 
//Step 9:Execute second update flow when project firmware is cob and last update_type is FW_ARG_UPDATE
    if((ret == 0)&&(IsCobPrj)&&(isBlank))
    {
        isBlank = 0;
        bl_log_trace("bl_update_fw_for_self_interactive_ctp:bl_update_flash for COB project need second update with blank IC:isBlank = %d\n",isBlank);
        goto UPDATE_SECOND_FOR_COB;
    }
	bl_log_trace("bl_update_fw_for_self_interactive_ctp exit\n");

UPDATE_ERROR:
	return ret;
}

static int bl_update_fw_for_compatible_ctp(unsigned char fileType,unsigned char* fw_data, int fw_size)
{
	unsigned char fwArgPrjID;	//firmware version
	int ret = 0x00;
	unsigned short fw_bin_checksum = 0x0;
	unsigned short fw_checksum = 0x0;
	unsigned char isBlank = 0x0;	//Indicate the IC have any firmware
	unsigned char update_type = NONE_UPDATE;  
	bl_log_trace("bl_update_fw_for_compatible_ctp start\n");
	
//Step 1:Obtain IC version number
	MDELAY(5);
	ret = bl_get_prj_id(&fwArgPrjID);
	if((ret < 0)
		|| (fileType == BIN_FILE_UPDATE)
		|| ((ret == 0) && (fwArgPrjID == 0))
		|| ((ret == 0) && (fwArgPrjID == 0xff)) 
		|| ((ret == 0) && (fwArgPrjID == BL_PRJ_ID_REG)))
	{
		isBlank = 1;
		bl_log_trace("bl_update_fw_for_compatible_ctp:This is blank IC ret = %x fwArgPrjID=%x\n",ret,fwArgPrjID);
	}
	else
	{
		isBlank = 0;
		bl_log_trace("bl_update_fw_for_compatible_ctp:ret=%x fwID=%x\n",ret,fwArgPrjID);
	}
	bl_log_trace("bl_update_fw_for_compatible_ctp:isBlank = %d  fwID = %d binFwID = %d", isBlank,fwArgPrjID,fw_data[BL_PRJ_ID_REG]);
//step 2:check checksum

    bl_log_trace("isBlank = %d  fwArgPrjID = %d  binFwArgPrjID = %d\n", isBlank,fwArgPrjID, fw_data[PJ_ID_OFFSET]);
    if(!isBlank)
    {
    	bl_get_fw_bin_checksum_for_compatible_ctp(fw_data, &fw_bin_checksum, fw_size);
    	ret = bl_get_fw_checksum(&fw_checksum);
    	if((ret < 0) || (fw_checksum != fw_bin_checksum)){
    		bl_log_trace("bl_update_fw_for_compatible_ctp:Read checksum fail fw_checksum = %x\n",fw_checksum);
    		fw_checksum = 0x00;
    	}
    	bl_log_trace("bl_update_fw_for_compatible_ctp:fw_checksum = 0x%x,fw_bin_checksum = 0x%x___\n",fw_checksum, fw_bin_checksum);
    }

//Step 2:Select update fw+arg or only update arg
	update_type = choose_update_type_for_compatible_ctp(isBlank, fw_data, fwArgPrjID, fw_checksum);

//Step 3:Start Update depend condition
	if(update_type != NONE_UPDATE)
	{
		ret = bl_update_flash_for_copatible_ctp(update_type, fw_data, fw_size);
		if(ret < 0)
		{
			bl_log_trace("bl_update_fw_for_compatible_ctp:bl_update_flash failed\n");
			goto UPDATE_ERROR;
		}
	}
	 
UPDATE_ERROR:
	bl_log_trace("bl_update_fw_for_compatible_ctp exit\n");
	return ret;
}

static int bl_update_fw(unsigned char fileType,unsigned char ctpType, unsigned char* pFwData, unsigned int fwLen)
{
    int ret = 0;
	bl_log_trace("bl_update_fw:ctpType = %x\n",ctpType); 
    switch(ctpType)
    {
        case SELF_CTP:
			ret = bl_update_fw_for_self_ctp(fileType,pFwData,fwLen);
			break;
        case SELF_INTERACTIVE_CTP:
			ret = bl_update_fw_for_self_interactive_ctp(fileType,pFwData,fwLen);
			break;

		case COMPATIBLE_CTP:
			ret = bl_update_fw_for_compatible_ctp(fileType,pFwData,fwLen);
			break;			
    }
	return ret;
}

#ifdef BL_AUTO_UPDATE_FARMWARE
int bl_auto_update_fw(void)
{
	int ret = 0;
    unsigned int fwLen = sizeof(fwbin);

	bl_log_trace("bl_auto_update_fw:fwLen = %x\n",fwLen); 
	ret = bl_update_fw(HEADER_FILE_UPDATE,CTP_TYPE, (unsigned char *)fwbin, fwLen);
	
	if(ret < 0)
	{
		bl_log_trace("bl_auto_update_fw: bl_update_fw fail\n");  
	}
	else
	{
		bl_log_trace("bl_auto_update_fw: bl_update_fw success\n");  
	}
	return ret;
}
#endif
#endif
