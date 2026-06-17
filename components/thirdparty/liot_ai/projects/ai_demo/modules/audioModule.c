#include <stdint.h>
#include <stdbool.h>
#include "audioModule.h"
#include "appFramework.h"

/**
 * @file audioModule.c
 * @brief 音频模块示例实现。
 */

void audioModulePlay(uint32_t ownerJobId, audioRes_t resource)
{
    (void)resource;

    /**
     * @note 这里接入真实播放驱动。
     *       播放完成后应调用 appOnAudioPlayDone(ownerJobId)。
     */
    (void)ownerJobId;
}

void audioModuleStop(uint32_t ownerJobId)
{
    (void)ownerJobId;
    /**
     * @note 这里接入真实停止播放逻辑。
     */
}

void audioModuleOpenCapture(uint32_t ownerJobId)
{
    /**
     * @note 这里接入真实采集、Mic、VAD 打开逻辑。
     *       当静默超时后应调用 appOnAudioVadTimeout(ownerJobId)。
     */
    (void)ownerJobId;
}

void audioModuleCloseCapture(uint32_t ownerJobId)
{
    (void)ownerJobId;
    /**
     * @note 这里接入真实采集关闭逻辑。
     */
}

void audioModuleVolumeIncrease(void)
{
    /**
     * @note 这里接入真实音量增加逻辑。
     */
}

void audioModuleVolumeDecrease(void)
{
    /**
     * @note 这里接入真实音量减小逻辑。
     */
}
