#include "sc7a20h.h"
#include "sc7a20h_def.h"
#include "liot_gpio2.h"
#include "liot_i2c.h"
#include "lierda_log.h"
#include "pinmap.h"


#define SC7A20H_DEBUG_PRINTF(format,  ...)   LIOT_PRINTF(UNILOG_LIOT_OPEN, P_DEBUG, format, ##__VA_ARGS__)
#define SC7A20H_INFO_PRINTF(format,  ...)    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, format, ##__VA_ARGS__)
#define SC7A20H_WARNING_PRINTF(format,  ...) LIOT_PRINTF(UNILOG_LIOT_OPEN, P_WARNING, format, ##__VA_ARGS__)
#define SC7A20H_ERROR_PRINTF(format,  ...)   LIOT_PRINTF(UNILOG_LIOT_OPEN, P_ERROR, format, ##__VA_ARGS__)


#if 0
static sc7a20h_i2c_type sc7a20h_init_reg[] = {
	{0x1F, 0x01},  //hi-pwr mode
	{0x23, 0x80},  //±2g  high byte in lower addr DLPF open
	{0x2E, 0x00},  //BY-PASS MODE
#if SL_SC7A20H_RAWDATA_HPF_ENABLE == 0x01	
	{0x21, 0x68},  //rawdata hpf
#else
	{0x21, 0x00},  //no rawdata hpf
#endif

#if SL_SC7A20H_FIFO_ENABLE == 0x00
	{0x24, 0x80},  //FIFO DISABLE
#else
	{0x24, 0xC0},  //FIFO ENABLE
#endif

#if SL_SC7A20H_INT_DEFAULT_LEVEL == 0x01
	{0x25, 0x02},  //defalut high level&& push-pull
#else
	{0x25, 0x00},  //defalut low  level&& push-pull	
#endif

#if SL_SC7A20H_FIFO_MODE_ENABLE == 0x00
	{0x2E, 0x9F},  //stream mode and fth=0x0F
#endif
	{0x20, 0x37},  //50Hz hi-pwr mode
	{0x22, 0x00},  //
	{0x57, 0x00},  //
};

//SC7A20H config shake interrupt check,
static sc7a20h_i2c_type sc7a20h_shake_reg[] = {
	{0x20, 0x4F},    //50Hz lower-pwr mode, in sc7a20h_mode_set();
	{0x23, 0x88},    //+-2g
	{0x21, 0x31},
	{SENSOR_PIN_OUTPUT_0, 0x40},    //AOI0中断on INT1 引脚
	{SENSOR_PIN_OUTPUT_1, 0x00},    //CLICK中断on INT2 引脚
	{0x24, 0x00},
	{0x30, 0x2a},    //x,y,z高事件或检测
	{0x32, 0x06},    //检测门限: 1-127, 值越小, 灵敏度越高
	{0x33, 0x00},
};

static sc7a20h_i2c_type sc7a20h_click_reg[] = {
	{0x1f, 0x01},  // 配合0x20, 工作模式使能
	{0x20, 0x77},
	{0x21, 0x71},  //高通滤波模式, HPIS1: 0 AOI1高通滤波禁止, 1:  AOI1高通滤波使能; HPIS2: 0 AOI2高通滤波禁止, 1:  AOI2高通滤波使能
	{0x22, 0x80},  //INT1,  0x80: CLICK使能中断, 0x00: 不使能中断, 0x40: AOI0中断on INT1 引脚
	{0x23, 0x90},  //0x90, +-4g(可能与摇晃中断冲突, 频率被截止)
	{0x24, 0x00},
	{0x25, 0x40},  //INT2,  0x80: CLICK使能中断, 0x00: 不使能中断, 0x40: AOI0中断on INT2 引脚
	{0x2e, 0x9f},  //stream mode and fth=0x0F
	{0x38, 0x0f},
	{0x3a, 0x6c},  //{0x3a, 0x6c},
	{0x3b, 0xb2},  //{0x3b, 0xb2},
	{0x3c, 0x04},  //{0x3c, 0x04},
	{0x3d, 0x33},  //{0x3d, 0x36},  敲击次数，要敲击重一些，才能触发点击中断（可调）
};
#endif


/* ================= 灵敏度查找表 (mg/LSB) ================= */
static const uint8_t g_sensitivity_table[] = {
    16,   /* 2G  */
    32,   /* 4G  */
    64,   /* 8G  */
    128,  /* 16G */
};

/* ================= ODR 频率查找表 (Hz) ================= */
static const uint16_t g_odr_hz_table[] = {
    0,    /* 0x00 */
	1,    /* 0x01 */
    12,   /* 0x02 */
    25,   /* 0x03 */
    50,   /* 0x04 */
    100,  /* 0x05 */
    200,  /* 0x06 */
    400,  /* 0x07 */
};

/* ================= 内部辅助函数 ================= */

static uint8_t get_sensitivity_mg(sc7a20h_range_t range) {
    uint8_t index = (range >> 4) & 0x03;
    if (index >= 4) index = 0;
    return g_sensitivity_table[index];
}

static uint16_t get_odr_hz(sc7a20h_odr_t odr) {
    if (odr > 7) return 50;
    return g_odr_hz_table[odr];
}

/**
 * @brief mg 转寄存器 LSB 值
 */
static uint8_t mg_to_lsb(uint16_t mg, sc7a20h_range_t range) {
    uint16_t sensitivity = get_sensitivity_mg(range);
    if (sensitivity == 0) sensitivity = 16;
    uint32_t val = mg / sensitivity;
    return (val > 127) ? 127 : (uint8_t)val;
}

/**
 * @brief ms 转 ODR 计数值
 */
static uint8_t ms_to_odr_cnt(uint16_t ms, sc7a20h_odr_t odr) {
    uint16_t hz = get_odr_hz(odr);
    if (hz == 0) hz = 50;
    uint32_t cnt = ((uint32_t)ms * hz) / 1000;
    return (cnt > 127) ? 127 : (uint8_t)cnt;
}

/**
 * @brief 【核心】安全的影子寄存器写操作 (RMW 机制)
 *        只修改 mask 指定的位，其他位保持不变
 */
static sc7a20h_err_e shadow_reg_write(sc7a20h_dev_t *dev, 
                                      uint8_t reg_addr,
                                      uint8_t *shadow_reg,
                                      uint8_t mask,
                                      uint8_t value) {
    /* 1. 更新影子寄存器 */
    *shadow_reg &= ~mask;           /* 清除旧位 */
    *shadow_reg |= (value & mask);  /* 设置新位 */
    
    /* 2. 写入硬件 */
    if (dev->hal.write_reg(reg_addr, shadow_reg, 1) != 0) {
        return SC7A20H_ERR_BUS;
    }
    
    return SC7A20H_OK;
}

/**
 * @brief 同步影子寄存器 (初始化时从硬件读取)
 */
static sc7a20h_err_e sync_shadow_regs(sc7a20h_dev_t *dev) {
    dev->hal.read_reg(REG_CTRL1, &dev->shadow_ctrl1, 1);
    dev->hal.read_reg(REG_CTRL2, &dev->shadow_ctrl2, 1);
    dev->hal.read_reg(REG_CTRL4, &dev->shadow_ctrl4, 1);
    dev->hal.read_reg(REG_INT1_CTRL, &dev->shadow_int1_ctrl, 1);
    dev->hal.read_reg(REG_INT2_CTRL, &dev->shadow_int2_ctrl, 1);
    dev->hal.read_reg(REG_FIFO_CTRL, &dev->shadow_fifo_ctrl, 1);

    return SC7A20H_OK;
}

/**
 * @brief 配置 Click 中断路由 (使用影子寄存器保护其他中断)
 */
static sc7a20h_err_e config_click_interrupt(sc7a20h_dev_t *dev, 
                                            sc7a20h_int_pin_t pin,
                                            bool enable) {
    if (pin == SC7A20H_INT_PIN_1) {
        return shadow_reg_write(dev, REG_INT1_CTRL, &dev->shadow_int1_ctrl,
                               BIT_INT1_CLICK, enable ? BIT_INT1_CLICK : 0);
    } else if (pin == SC7A20H_INT_PIN_2) {
        return shadow_reg_write(dev, REG_INT2_CTRL, &dev->shadow_int2_ctrl,
                               BIT_INT2_CLICK, enable ? BIT_INT2_CLICK : 0);
    }

    return SC7A20H_ERR_PARAM;
}

/**
 * @brief 配置 Shake 中断路由 (使用影子寄存器保护其他中断)
 */
static sc7a20h_err_e config_shake_interrupt(sc7a20h_dev_t *dev,
                                            sc7a20h_int_pin_t pin,
                                            bool enable) {
    if (pin == SC7A20H_INT_PIN_1) {
        return shadow_reg_write(dev, REG_INT1_CTRL, &dev->shadow_int1_ctrl,
                               BIT_INT1_AOI1, enable ? BIT_INT1_AOI1 : 0);
    } else if (pin == SC7A20H_INT_PIN_2) {
        return shadow_reg_write(dev, REG_INT2_CTRL, &dev->shadow_int2_ctrl,
                               BIT_INT2_AOI1, enable ? BIT_INT2_AOI1 : 0);
    }
    return SC7A20H_ERR_PARAM;
}


/* ================= 核心 API 实现 ================= */

sc7a20h_err_e sc7a20h_check(sc7a20h_dev_t *dev)
{
	uint8_t reg_value1 = 0, reg_value2 = 0;

	if (dev->hal.read_reg(REG_WHO_AM_I, &reg_value1, 1) != 0) {
		SC7A20H_ERROR_PRINTF("read WHO_AM_I register failed!");
		return SC7A20H_ERR_BUS;
	}

	if (dev->hal.read_reg(REG_VERSION, &reg_value2, 1) != 0) {
		SC7A20H_ERROR_PRINTF("read VERSION register failed!");
		return SC7A20H_ERR_BUS;
	}
	if ((reg_value1 != 0x11) && (reg_value2 != 0x28)) {
		SC7A20H_ERROR_PRINTF("invalid version value: 0x%x (expect: 0x11), 0x%x (expect: 0x28)", reg_value1, reg_value2);
		return SC7A20H_ERR_ID;
	}

	return SC7A20H_OK;
}

sc7a20h_err_e sc7a20h_power_down(sc7a20h_dev_t *dev)
{
	uint8_t read_val  = 0xff;
	
	/* 4. 进入 Power Down */
    dev->shadow_ctrl1 = 0x00;
    dev->hal.write_reg(REG_CTRL1, &dev->shadow_ctrl1, 1);

	dev->hal.read_reg(REG_CTRL1, &read_val, 1);  //close aoi1 function

	if(read_val == 0x00)	return  SC7A20H_OK;
	else					return  SC7A20H_ERR_BUS;
}

sc7a20h_err_e sc7a20h_soft_reset(sc7a20h_dev_t *dev)
{
	uint8_t read_val  = 0xff;
	uint8_t write_value = 0x80;
	
	dev->hal.write_reg(REG_CTRL4, &write_value, 1);  //FLAG
	osDelay(100);
	dev->hal.write_reg(REG_CTRL5, &write_value, 1);  //BOOT
	write_value = SOFT_RESET_VALUE;
	dev->hal.write_reg(REG_SOFT_RESET, &write_value, 1);  //SOFT_RESET
	osDelay(200);
	dev->hal.read_reg(REG_CTRL4, &read_val, 1);  //close aoi1 function

	if(read_val == 0x00)	return  SC7A20H_OK;
	else					return  SC7A20H_ERR_BUS;
}

sc7a20h_err_e sc7a20h_mode_set(sc7a20h_dev_t *dev, sc7a20h_mode_t mode)
{
	if (mode > SC7A20H_MODE_HI_PERF_2 || !dev || !dev->is_init) {
		return SC7A20H_ERR_PARAM;
	}

	uint8_t reg_val = 0;
	dev->hal.read_reg(REG_MODE_HR_CTRL, &reg_val, 1);
	if (mode >= SC7A20H_MODE_HI_PERF) {
		reg_val |= 0x01;  // 位 0 为工作模式高位
	} else {
		reg_val &= ~0x01; // 位 0 为工作模式高位
	}
	dev->hal.write_reg(REG_MODE_HR_CTRL, &reg_val, 1);

	reg_val = (mode & 0x01) << 3;  // 位 3 为工作模式低位
    return shadow_reg_write(dev, REG_CTRL1, &dev->shadow_ctrl1, MASK_MODE, reg_val);
}

sc7a20h_err_e sc7a20h_odr_set(sc7a20h_dev_t *dev, sc7a20h_odr_t odr)
{
	if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;

	SC7A20H_INFO_PRINTF("set odr: 0X%x", odr);
	return shadow_reg_write(dev, REG_CTRL1, &dev->shadow_ctrl1, MASK_ODR, (odr << 4));
}

sc7a20h_err_e sc7a20h_init(sc7a20h_dev_t *dev, const sc7a20h_hal_t *hal)
{
    if (!dev || !hal || !hal->write_reg || !hal->read_reg) {
        return SC7A20H_ERR_PARAM;
    }
    
    memset(dev, 0, sizeof(sc7a20h_dev_t));
    dev->hal = *hal;
    
    /* 1. 检查 ID */
    sc7a20h_err_e err = sc7a20h_check(dev);
    if (err != SC7A20H_OK) {
        return err;
    }

	/* 2. 进入 Power Down */
	err = sc7a20h_power_down(dev);
	if (err != SC7A20H_OK) {
		SC7A20H_ERROR_PRINTF("set power down failed failed: 0x%x", err);
		return err;
	}

	/* 2.1. 软复位 */
	err = sc7a20h_soft_reset(dev);
	if (err != SC7A20H_OK) {
		SC7A20H_ERROR_PRINTF("soft reset failed: 0x%x", err);
		return err;
	}
    
    /* 3. 同步影子寄存器 */
    sync_shadow_regs(dev);

    dev->is_init = true;
    dev->init_tick = dev->hal.get_tick_ms();
    
    return SC7A20H_OK;
}

sc7a20h_err_e sc7a20h_apply_config(sc7a20h_dev_t *dev, const sc7a20h_config_t *cfg)
{
	uint8_t reg_val = 0;
	sc7a20h_err_e err = SC7A20H_OK;

    if (!dev->is_init || !cfg) {
        return SC7A20H_ERR_NOT_INIT;
    }

    /* 保存配置 */
    dev->cfg = *cfg;
    
    /* 1. 配置 CTRL1 (ODR + Mode) */
    err = sc7a20h_odr_set(dev, cfg->odr);
    if (err != SC7A20H_OK) goto err_exit;
    dev->hal.delay_ms(50);
    
    err = sc7a20h_mode_set(dev, cfg->mode);
    if (err != SC7A20H_OK) goto err_exit;
    dev->hal.delay_ms(50);

    /* 2. 配置 CTRL4 (Range) */
    reg_val = (cfg->range & MASK_RANGE) | 0x80;  /* 0x80: high byte at lower addr */
    dev->shadow_ctrl4 = reg_val;
    err = dev->hal.write_reg(REG_CTRL4, &dev->shadow_ctrl4, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 3. 配置 CTRL2 (HPF) */
    dev->shadow_ctrl2 = cfg->high_pass_filter ? 0x71 : 0x00;
    err = dev->hal.write_reg(REG_CTRL2, &dev->shadow_ctrl2, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 4. 配置 FIFO */
    if (cfg->fifo.enable) {
#if SL_SC7A20H_FIFO_MODE_ENABLE == 0x00
        dev->shadow_fifo_ctrl = 0x9F;  /* FIFO Mode + Watermark, stream mode and fth=0x0F */
#endif
    } else {
        dev->shadow_fifo_ctrl = 0x00;  /* By pass mode*/
    }
    err = dev->hal.write_reg(REG_FIFO_CTRL, &dev->shadow_fifo_ctrl, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 5. 配置 Click 功能 */
    if (cfg->click.enable) {
        err = sc7a20h_config_click(dev, &cfg->click);
        if (err != SC7A20H_OK) goto err_exit;
    }
    
    /* 6. 配置 Shake 功能 */
    if (cfg->shake.enable) {
        err = sc7a20h_config_shake(dev, &cfg->shake);
        if (err != SC7A20H_OK) goto err_exit;
    }

err_exit:
	SC7A20H_INFO_PRINTF("apply config errCode: 0x%x", err);
	return err;
}

sc7a20h_err_e sc7a20h_config_click(sc7a20h_dev_t *dev, const sc7a20h_click_cfg_t *cfg)
{
	uint8_t reg_val;
    sc7a20h_err_e err = SC7A20H_OK;

	if (!dev->is_init || !cfg) {
        return SC7A20H_ERR_NOT_INIT;
    }

    /* 保存配置 */
    dev->cfg.click = *cfg;
    
    /* 1. 配置 CLICK_CFG (0x38) - 独立寄存器 */
    reg_val = 0;
    if (cfg->axis_x) reg_val |= 0x01;
    if (cfg->axis_y) reg_val |= 0x02;
    if (cfg->axis_z) reg_val |= 0x04;
    
    err = dev->hal.write_reg(REG_CLICK_CFG, &reg_val, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 2. 配置阈值 (0x3A) */
    reg_val = mg_to_lsb(cfg->threshold_mg, dev->cfg.range);  //TODO: 阈值范围需要根据范围调整
    err = dev->hal.write_reg(REG_CLICK_COEFF1, &reg_val, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 3. 配置时间参数 */
    err = dev->hal.write_reg(REG_CLICK_COEFF2, &cfg->time_limit, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    err = dev->hal.write_reg(REG_CLICK_COEFF3, &cfg->time_latency, 1);
    if (err != SC7A20H_OK) goto err_exit;

	reg_val = (cfg->time_window << 4) | cfg->mode;
    err = dev->hal.write_reg(REG_CLICK_COEFF4, &reg_val, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 4. 配置中断路由 - 【关键】使用影子寄存器 RMW */
    if (cfg->enable) {
        err = config_click_interrupt(dev, cfg->int_pin, true);
    } else {
        err = config_click_interrupt(dev, cfg->int_pin, false);
    }

err_exit:
	SC7A20H_INFO_PRINTF("config click errCode: 0x%x", err);
	return err;
}

sc7a20h_err_e sc7a20h_config_shake(sc7a20h_dev_t *dev, const sc7a20h_shake_cfg_t *cfg) {
    if (!dev->is_init || !cfg) {
        return SC7A20H_ERR_NOT_INIT;
    }
    
    sc7a20h_err_e err;
    uint8_t reg_val;
    
    /* 保存配置 */
    dev->cfg.shake = *cfg;
    
    /* 1. 配置 AOI1_CFG (0x30) */
    reg_val = 0;
    if(cfg->aoi_and_mode) reg_val |= 0x80;
    if(cfg->detect_mode) reg_val |= 0x40;

    if (cfg->axis_x_high) reg_val |= 0x02;
    if (cfg->axis_y_high) reg_val |= 0x08;
    if (cfg->axis_z_high) reg_val |= 0x20;

    if (cfg->axis_x_low)  reg_val |= 0x01;
    if (cfg->axis_y_low)  reg_val |= 0x04;
    if (cfg->axis_z_low)  reg_val |= 0x10;
    
    err = dev->hal.write_reg(REG_AOI1_CFG, &reg_val, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 2. 配置阈值 (0x32) */
    reg_val = mg_to_lsb(cfg->threshold_mg, dev->cfg.range);  //TODO: 阈值范围需要根据范围调整
    err = dev->hal.write_reg(REG_AOI1_THS, &reg_val, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 3. 配置持续时间 (0x33) */
    reg_val = ms_to_odr_cnt(cfg->duration_ms, dev->cfg.odr);
    err = dev->hal.write_reg(REG_AOI1_DUR, &reg_val, 1);
    if (err != SC7A20H_OK) goto err_exit;
    
    /* 4. 配置中断路由 - 【关键】使用影子寄存器 RMW */
    if (cfg->enable) {
        err = config_shake_interrupt(dev, cfg->int_pin, true);
    } else {
        err = config_shake_interrupt(dev, cfg->int_pin, false);
    }

err_exit:
	SC7A20H_INFO_PRINTF("config shake errCode: 0x%x", err);
	return err;
}

sc7a20h_err_e sc7a20h_set_click_threshold(sc7a20h_dev_t *dev, uint16_t mg) {
    if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;
    
    uint8_t reg_val = mg_to_lsb(mg, dev->cfg.range);
    sc7a20h_err_e err = dev->hal.write_reg(REG_CLICK_COEFF1, &reg_val, 1);
    
    if (err == SC7A20H_OK) {
        dev->cfg.click.threshold_mg = mg;
    }
    
    return err;
}

sc7a20h_err_e sc7a20h_set_shake_threshold(sc7a20h_dev_t *dev, uint16_t mg) {
    if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;
    
    uint8_t reg_val = mg_to_lsb(mg, dev->cfg.range);
    sc7a20h_err_e err = dev->hal.write_reg(REG_AOI1_THS, &reg_val, 1);
    
    if (err == SC7A20H_OK) {
        dev->cfg.shake.threshold_mg = mg;
    }
    
    return err;
}

sc7a20h_err_e sc7a20h_set_shake_duration(sc7a20h_dev_t *dev, uint16_t ms) {
    if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;
    
    uint8_t reg_val = ms_to_odr_cnt(ms, dev->cfg.odr);
    sc7a20h_err_e err = dev->hal.write_reg(REG_AOI1_DUR, &reg_val, 1);
    
    if (err == SC7A20H_OK) {
        dev->cfg.shake.duration_ms = ms;
    }
    
    return err;
}

sc7a20h_err_e sc7a20h_set_range(sc7a20h_dev_t *dev, sc7a20h_range_t range) {
    if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;
    
    uint8_t reg_val = (range & MASK_RANGE) | 0x80;
    dev->shadow_ctrl4 = reg_val;
    dev->cfg.range = range;
    
    return dev->hal.write_reg(REG_CTRL4, &dev->shadow_ctrl4, 1);
}

sc7a20h_err_e sc7a20h_read_accel(sc7a20h_dev_t *dev, sc7a20h_accel_data_t *data) {
    if (!dev->is_init || !data) {
        return SC7A20H_ERR_NOT_INIT;
    }
    
    /* 等待 DRDY */
    uint8_t status = 0;
    uint32_t start = dev->hal.get_tick_ms();
    do {
        dev->hal.read_reg(REG_DRDY_STATUS, &status, 1);
        if ((dev->hal.get_tick_ms() - start) > 100) {
            return SC7A20H_ERR_TIMEOUT;
        }
        dev->hal.delay_ms(10);
    } while ((status & 0x0F) != 0x0F);
    
    /* 读取 6 字节 (0xA8 = 0x28 | 0x80 自动递增) */
    uint8_t raw[6];
    if (dev->hal.read_reg(0xA8, raw, 6) != 0) {
        return SC7A20H_ERR_BUS;
    }
    
    data->x = (int16_t)((raw[1] << 8) | raw[0]) >> 4;  /* 12bit */
    data->y = (int16_t)((raw[3] << 8) | raw[2]) >> 4;
    data->z = (int16_t)((raw[5] << 8) | raw[4]) >> 4;
    data->timestamp_ms = dev->hal.get_tick_ms();

	// SC7A20H_INFO_PRINTF("Accel: X=%d, Y=%d, Z=%d", data->x, data->y, data->z);
    return SC7A20H_OK;
}

sc7a20h_err_e sc7a20h_get_int_status(sc7a20h_dev_t *dev, sc7a20h_int_status_t *status) {
    if (!dev->is_init || !status) {
        return SC7A20H_ERR_NOT_INIT;
    }
    
    memset(status, 0, sizeof(sc7a20h_int_status_t));
    
    /* 读取 Shake 状态 (0x31) */
    if (dev->cfg.shake.enable) {
        if (dev->hal.read_reg(REG_AOI1_STAT, &status->raw_status_aoi, 1) == 0) {
            status->shake_event = (status->raw_status_aoi & 0x3F) != 0; // 位11 1111, 其中 1 为中断事件产生
            status->shake_low  = (status->raw_status_aoi & 0x15) != 0;  // 位01 0101, 其中 1 为低中断事件产生
            status->shake_high = (status->raw_status_aoi & 0x2a) != 0;  // 位10 1010, 其中 1 为高中断事件产生
			SC7A20H_INFO_PRINTF("read shake reg[%x] status: 0x%x", REG_AOI1_STAT, status->raw_status_aoi);
        }
    }
    
    /* 读取 Click 状态 (0x39) - 读取即清除 */
    if (dev->cfg.click.enable) {
        if (dev->hal.read_reg(REG_CLICK_SRC, &status->raw_status_click, 1) == 0) {
            status->click_times = status->raw_status_click;
			SC7A20H_INFO_PRINTF("read click reg[%x] status: 0x%x", REG_CLICK_SRC, status->raw_status_click);
        }
    }
    
    /* 读取 DRDY 状态 */
    uint8_t drdy = 0;
    if (dev->hal.read_reg(REG_DRDY_STATUS, &drdy, 1) == 0) {
        status->data_ready = (drdy & 0x0F) == 0x0F;
    }
    
    return SC7A20H_OK;
}

uint8_t sc7a20h_read_fifo(sc7a20h_dev_t *dev, sc7a20h_accel_data_t *data_buf, uint8_t max_len) {
    if (!dev->is_init || !data_buf || max_len == 0) {
        return 0;
    }

/*#if SL_SC7A20H_FIFO_MODE_ENABLE == 0x00
	dev->hal.write_reg(0x22, 0x00);
#else
	dev->hal.write_reg(0x22, 0x01);
#endif*/

    uint8_t fifo_num = 0;
    dev->hal.read_reg(REG_FIFO_SRC, &fifo_num, 1);
    fifo_num &= 0x3F;

    if (fifo_num == 0 || fifo_num > max_len) {
        fifo_num = (fifo_num > max_len) ? max_len : 0;
    }
    
    if (fifo_num == 0) return 0;


/*#if SL_SC7A20H_FIFO_MODE_ENABLE == 0x00
	fifo_len = fifo_num * 6;  //XYZ,*6
#else
	fifo_len = fifo_num * 3;  //XYZ,*3
#endif*/

    /* 读取 FIFO 数据 (6 字节/样本) */
    uint8_t len = fifo_num * 6;
    uint8_t *raw_buf = (uint8_t *)data_buf;  /* 复用缓冲区 */
    
    dev->hal.read_reg(0xA8, raw_buf, len);

/*#if SL_SC7A20H_FIFO_MODE_ENABLE == 0x00
	sc7a20h_read_bytes(0x69, SC7A20H_FIFO_DATA, fifo_len);

	for(j = 0; j < fifo_num; j++)
	{
		x_data_buf[j] = (signed short)((SC7A20H_FIFO_DATA[j*6]<<8) + SC7A20H_FIFO_DATA[j*6+1]) >> 4;  //12bit
		y_data_buf[j] = (signed short)((SC7A20H_FIFO_DATA[j*6+2]<<8) + SC7A20H_FIFO_DATA[j*6+3]) >> 4;  //12bit
		z_data_buf[j] = (signed short)((SC7A20H_FIFO_DATA[j*6+4]<<8) + SC7A20H_FIFO_DATA[j*6+5]) >> 4;  //12bit
	}
#else
	sc7a20h_read_bytes(0x69, SL_SC7A20H_FIFO_DATA, fifo_len);

	for(j = 0; j < fifo_num;j++)
	{
		x_data_buf[j] = (signed char)SC7A20H_FIFO_DATA[0+j*3];//8bit
		y_data_buf[j] = (signed char)SC7A20H_FIFO_DATA[1+j*3];//8bit
		z_data_buf[j] = (signed char)SC7A20H_FIFO_DATA[2+j*3];//8bit
	}
#endif*/

    /* 解析数据 */
    for (uint8_t i = 0; i < fifo_num; i++) {
        uint8_t *p = raw_buf + (i * 6);
        data_buf[i].x = (int16_t)((p[1] << 8) | p[0]) >> 4;
        data_buf[i].y = (int16_t)((p[3] << 8) | p[2]) >> 4;
        data_buf[i].z = (int16_t)((p[5] << 8) | p[4]) >> 4;
        data_buf[i].timestamp_ms = dev->hal.get_tick_ms();
    }

	// dev->hal.write_reg(0x2E, 0x00);  //BY PASS MODE
	// dev->hal.write_reg(0x2E, 0x9F);  //FIFO MODE

    return fifo_num;
}

#if 0
sc7a20h_err_e sc7a20h_enter_low_power(sc7a20h_dev_t *dev) {
    if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;
    
    dev->shadow_ctrl1 = 0x00;  /* Power Down */
    return dev->hal.write_reg(REG_CTRL1, &dev->shadow_ctrl1, 1);
}

sc7a20h_err_e sc7a20h_wakeup(sc7a20h_dev_t *dev) {
    if (!dev->is_init) return SC7A20H_ERR_NOT_INIT;
    
    /* 恢复 ODR + Mode */
    uint8_t reg_val = (dev->cfg.odr & MASK_ODR) | ((dev->cfg.mode << 4) & MASK_MODE);
    dev->shadow_ctrl1 = reg_val;
    sc7a20h_err_e err = dev->hal.write_reg(REG_CTRL1, &dev->shadow_ctrl1, 1);
    if (err == SC7A20H_OK) {
        dev->hal.delay_ms(50);
    }
    return err;
}
#endif

bool sc7a20h_is_ready(sc7a20h_dev_t *dev) {
    return dev->is_init;
}
