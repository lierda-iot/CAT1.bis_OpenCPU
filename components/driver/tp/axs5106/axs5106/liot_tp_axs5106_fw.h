/**
 * @File Name: liot_tp_axs5106_fw.h
 * @brief AXS5106 固件选择配置
 *        通过宏 LIOT_TP_AXS5106_FW_VER 选择对应的固件头文件
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#ifndef __LIOT_TP_AXS5106_FW_H__
#define __LIOT_TP_AXS5106_FW_H__

/* ================= 固件版本选择 =================
 * 取值说明:
 *   1  -> V01 默认固件
 *
 * 也可在编译选项中定义: -DLIOT_TP_AXS5106_FW_VER=1
 */

#ifndef LIOT_TP_AXS5106_FW_VER
#define LIOT_TP_AXS5106_FW_VER  2
#endif

/* 根据选择包含对应固件头文件 */
#if (LIOT_TP_AXS5106_FW_VER == 1)
  #include "axs5106_fw_V01_default.h"
  /* AXS5106 默认固件 */
#elif (LIOT_TP_AXS5106_FW_VER == 2)
  #include "axs5106_fw_V02_default.h"
#else
  #error "Unknown LIOT_TP_AXS5106_FW_VER value, please select 1"
#endif

#endif /* __LIOT_TP_AXS5106_FW_H__ */
