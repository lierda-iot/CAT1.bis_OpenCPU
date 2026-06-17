#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H

#include <stdint.h>

/**
 * @file audioModule.h
 * @brief 音频模块接口。
 */

/**
 * @brief 音频资源标识。
 */
typedef enum {
    audioResNone = 0,              /**< 无效资源。 */
    audioResTalkPrompt,          /**< 唤醒提示音。 */
    audioResStandbyPrompt,         /**< 待机提示音。 */
} audioRes_t;

/**
 * @brief 播放音频资源。
 * @param ownerJobId 所属 job 标识。
 * @param resource 音频资源标识。
 */
void audioModulePlay(uint32_t ownerJobId, audioRes_t resource);

/**
 * @brief 停止当前播放。
 * @param ownerJobId 所属 job 标识。
 */
void audioModuleStop(uint32_t ownerJobId);

/**
 * @brief 打开采集与 VAD 能力。
 * @param ownerJobId 所属 job 标识。
 */
void audioModuleOpenCapture(uint32_t ownerJobId);

/**
 * @brief 关闭采集与 VAD 能力。
 * @param ownerJobId 所属 job 标识。
 */
void audioModuleCloseCapture(uint32_t ownerJobId);

/**
 * @brief 增加音量。
 */
void audioModuleVolumeIncrease(void);

/**
 * @brief 减小音量。
 */
void audioModuleVolumeDecrease(void);

#endif /* AUDIO_MODULE_H */
