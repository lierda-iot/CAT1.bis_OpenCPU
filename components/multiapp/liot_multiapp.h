/**
 * @file    liot_multiapp.h
 * @brief   多 APP 二级加载器（仅 APP0 编入）
 * @note    客户在 user_main 中手动调用 Liot_MultiAppLoad 加载子 APP
 *          例如：Liot_MultiAppLoad(PKGFLXAPP_APPX_LNA, 0, 0, 0, 0);
 */
#ifndef __LIOT_MULTIAPP_H__
#define __LIOT_MULTIAPP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  多 APP 初始化：启动 APP1..APPn
 * @param  app_addr          要加载的 APP 地址
 * @param  arg1              传递给 APPn main 的第 1 个参数
 * @param  arg2              传递给 APPn main 的第 2 个参数
 * @param  arg3              传递给 APPn main 的第 3 个参数
 * @param  arg4              传递给 APPn main 的第 4 个参数
 * @return 0（个别 APPn 校验失败只打印并跳过）
 * @warning 必须在 SysTabInit() 成功之后执行
 */
int Liot_MultiAppLoad(uint32_t app_addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

#ifdef __cplusplus
}
#endif

#endif /* __LIOT_MULTIAPP_H__ */
