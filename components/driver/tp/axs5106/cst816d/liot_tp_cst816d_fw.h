/**
 * @File Name: liot_tp_cst816d_fw.h
 * @brief CST816D 固件选择配置
 *        通过宏 LIOT_TP_CST816D_FW_VER 选择对应的固件头文件
 *        默认选择 V02 (校验和0x5e1c, Q1352B)
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#ifndef __LIOT_TP_CST816D_FW_H__
#define __LIOT_TP_CST816D_FW_H__

/* ================= 固件版本选择 =================
 * 取值说明:
 *   1  -> V01_0x6a07  汇顶 (CSW_2511146_HYN_COB_ZY_W01_W60)
 *   2  -> V01_0x77eb  金耐克 (TH-RGD-JMC-CST816D_V1)
 *   3  -> V02_0x5e1c  第三家 (TH-RGD-Q1352B-CST816D_V2) [默认]
 *   4  -> V04_0x96ab  第三家测试 (TH-RGD-Q1352B-CST816D_V4)
 *   5  -> V09_0x28e7  讯航-出货 (TH-MWD-1536A-V1-CST816D_V4)
 *
 * 也可在编译选项中定义: -DLIOT_TP_CST816D_FW_VER=5
 */

#ifndef LIOT_TP_CST816D_FW_VER
#define LIOT_TP_CST816D_FW_VER  7
#endif

/* 根据选择包含对应固件头文件 */
#if (LIOT_TP_CST816D_FW_VER == 1)
  #include "V01_0x6a07_CSW_2511146_CST816D_HYN_COB_ZY_W01_W60_updata.h"
  /* 汇顶触摸屏, checksum=0x6a07 */

#elif (LIOT_TP_CST816D_FW_VER == 2)
  #include "V01_0x77eb_TH-RGD-JMC-CST816D_V1_updata.h"
  /* 金耐克触摸屏, checksum=0x77eb */

#elif (LIOT_TP_CST816D_FW_VER == 3)
  #include "V02_0x5e1c_TH-RGD-Q1352B-CST816D_V2_updata.h"
  /* 第三家触摸屏(Q1352B), checksum=0x5e1c [默认] */

#elif (LIOT_TP_CST816D_FW_VER == 4)
  #include "V04_0x96ab_TH-RGD-Q1352B-CST816D_V4_updata.h"
  /* 第三家测试, checksum=0x96ab */

#elif (LIOT_TP_CST816D_FW_VER == 5)
  #include "V09_0x28e7_TH-MWD-1536A-V1-CST816D_V4_updata.h"
  /* 讯航触摸屏(出货/V4防水), checksum=0x28e7 */

#elif (LIOT_TP_CST816D_FW_VER == 6)
  #include "V03_0xa672_TH-RGD-Q1352B-CST816D_V3_updata.h"
  /* 讯航触摸屏(出货/V4防水), checksum=0x28e7 */
#elif (LIOT_TP_CST816D_FW_VER == 7)
  #include "V04_0xd582_TH-RGD-Q1352B-CST816D_V4_updata.h"
  /* 讯航触摸屏(出货/V4防水), checksum=0x28e7 */
#else
  #error "Unknown LIOT_TP_CST816D_FW_VER value, please select 1~5"
#endif

#endif /* __LIOT_TP_CST816D_FW_H__ */
