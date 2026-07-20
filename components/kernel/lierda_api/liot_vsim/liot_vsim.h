/*
 * @Author: lixianhong lixh@lierda.com
 * @Date: 2025-09-16 15:42:21
 * @LastEditors: lixianhong lixh@lierda.com
 * @LastEditTime: 2025-09-18 11:09:57
 * @FilePath: \cat1_ec718_general\PLAT\middleware\lierda_open\lierda_api\liot_vsim\inc\liot_vsim.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 * @File Name: liot_vsim.h
 * @brief
 * @Author : Lxh email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2025-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#ifndef LIOT_VSIM_H
#define LIOT_VSIM_H

#include "liot_api_common.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
    char name[16];           // VSIM 厂家名称 (TGT: 途歌)
    uint8_t vsimMode;        // VSIM 模式 （0: 物理SIM, 1: 虚拟SIM）
} liot_vsim_info_t;

/**
 * @brief 设置VSIM模块
 * 
 * @param vsimMfg VSIM 厂家名称
 * @param enable  0: 关闭, 1: 开启
 * @return int 0: 成功, -1: 失败    
 */
int liot_vsim_set(char* vsimMfg, bool enable);

/**
 * @brief 获取VSIM模块状态
 * 
 * @param vsimMfg VSIM 信息
 * @return int 0: 成功, -1: 失败
 */
int liot_vsim_get(liot_vsim_info_t* vsimInfo);

#ifdef __cplusplus
} /*"C" */
#endif

#endif


