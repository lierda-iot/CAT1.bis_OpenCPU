/**
 * @file    liot_multiapp.c
 * @brief   多 APP 二级加载器（仅 APP0 编入）
 * @note    客户在 user_main 中调用 Liot_MultiAppLoad 手动加载 APP1
 */
#include <stdint.h>
#include "liot_log.h"
#include "main.h"
#include "liot_multiapp.h"

typedef void (*Liot_AppMain_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

/**
 * @brief  加载并启动指定地址的 APP
 * @param  app_addr      APP 的 FLASH 起始地址
 * @param  arg1..arg4    传递给 APP main 函数的 4 个参数
 * @return 0 已启动；-1 头校验失败
 */
int Liot_MultiAppLoad(uint32_t app_addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    const app_header_t *head = (const app_header_t *)app_addr;
    uint32_t mainAddr = head->main_addr;

    /* 校验 main 函数地址（Thumb 模式低位为 1） */
    if (((mainAddr & 0x1U) == 0) || (mainAddr <= app_addr) || (mainAddr >= app_addr + 0x100000))
    {
        liot_trace("[multiapp] @0x%X: bad main 0x%X\r\n", app_addr, mainAddr);
        return -1;
    }

    liot_trace("[multiapp] load @0x%X main 0x%X\r\n", app_addr, mainAddr);
    ((Liot_AppMain_t)mainAddr)(arg1, arg2, arg3, arg4);
    return 0;
}
