#ifndef LED_MODULE_H
#define LED_MODULE_H

#include <stdint.h>

/**
 * @file ledModule.h
 * @brief LED 模块接口。
 */

/**
 * @brief LED 灯效模式。
 */
typedef enum {
    ledPatternOff = 0,             /**< 熄灭。 */
    ledPatternInteractionBlink,    /**< 交互闪烁。 */
} ledPattern_t;

/**
 * @brief 设置 LED 灯效。
 * @param ownerJobId 所属 job 标识。
 * @param pattern 灯效模式。
 */
void ledModuleSetPattern(uint32_t ownerJobId, ledPattern_t pattern);

#endif /* LED_MODULE_H */
