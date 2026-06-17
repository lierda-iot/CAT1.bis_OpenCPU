#include "ledModule.h"

/**
 * @file ledModule.c
 * @brief LED 模块示例实现。
 */

void ledModuleSetPattern(uint32_t ownerJobId, ledPattern_t pattern)
{
    (void)ownerJobId;
    (void)pattern;
    /**
     * @note 这里接入真实 LED 驱动。
     */
}
