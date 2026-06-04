/**
 * @File Name: liot_pwmaud.h
 * @brief PWM Audio Driver Interface
 * @author wty email:ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2023-09-07
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */

#ifndef __LIOT_PWM_DAC_H__
#define __LIOT_PWM_DAC_H__

#include "liot_type.h"
#include "liot_i2s_type.h"

typedef enum
{
    PWMAud_Deinited = 0, ///< PWM audio not initialized
    PWMAud_Inited,       ///< PWM audio configuration completed
    PWMAud_Start,        ///< PWM audio timer started
    PWMAud_Stop,         ///< PWM audio timer stopped
    PWMAud_Playing,      ///< PWM audio data output in progress
    PWMAud_PlayEnd,      ///< PWM audio data output completed
    PWMAud_Busy,         ///< PWM audio not ready
} PWMAud_state_e;

/**
 * @brief PWM Audio channel mode selection
 *
 * Note: the difference between dual-channel differential and hardware-complementary dual-channel
 * differential is as follows:
 * 
 * For dual-channel differential, two PWM channels can be arbitrarily designated for differential
 * operation, but there is a microsecond-level (μs-level) phase difference in the differential audio.
 * 
 * For hardware-complementary dual-channel differential, only the corresponding PWMx and PWMxn
 * channels (see the Pin Multiplexing Table for details) can be selected for differential operation, 
 * while there is no phase difference in the differential audio.
 */
typedef enum
{
    LIOT_PWM_AUDIO_MODE_MONO = 0,  ///< Mono PWM audio
    LIOT_PWM_AUDIO_MODE_DIFF,      ///< Dual differential PWM audio
    LIOT_PWM_AUDIO_MODE_DUAL,      ///< Dual channel dual PWM audio
    LIOT_PWM_AUDIO_MODE_HARD_DIFF, ///< Hardware complementary dual differential PWM audio
} liot_pwmaudio_channel_mode_e;

typedef struct
{
    liotI2sSampleRate_e         sampleRate; ///< Sample rate choose
    liotI2sFrameSize_e          frameSize;  ///< Frame size choose
    uint32_t totalNum;                  ///< Total number of audio sources
}liot_i2s_param_t;


typedef struct
{
    int8_t pwmaudio_num;  ///< PWM output/left channel
    int8_t pwmNaudio_num; ///< PWM differential/right channel/complementary differential channel

    int8_t pwmaudio_pin;  ///< PWM audio output/left channel pin
    int8_t pwmNaudio_pin; ///< PWM audio differential/right channel/complementary differential pin

    liot_pwmaudio_channel_mode_e mode; ///< PWM audio mode
}liot_pwmaudio_param_t;

/**
 * @brief PWM audio callback function pointer
 *
 * Callback function type for PWM audio events notification
 *
 * @param[in] event Event type from PWM audio driver
 * @param[in] arg Additional argument for the event
 */
typedef void (*pwm_audio_cb_Func)(uint32_t event, uint32_t arg);

/**
 * @brief Initialize PWM audio
 *
 * Configures and initializes the PWM audio driver with specified parameters
 * and callback function.
 *
 * @param[in] pwm_param Pointer to PWM audio pin parameters structure
 * @param[in] txCb Pointer to transmit callback function
 */
void PWMAud_Init(liot_pwmaudio_param_t *pwm_param, pwm_audio_cb_Func txCb);

/**
 * @brief Deinitialize PWM audio
 *
 * Releases resources and resets the PWM audio driver to uninitialized state.
 */
void PWMAud_Deinit(void);
/**
 * @brief Configure PWM audio parameters
 *
 * Configures audio sample rate, frame width and other I2S parameters.
 *
 * @param[in] paramCtrl Pointer to I2S parameter structure
 */
void PWMAud_config(liot_i2s_param_t *paramCtrl);

/**
 * @brief Start PWM audio
 *
 * Enables the timer used for PWM audio generation.
 */
void PWMAud_start(void);

/**
 * @brief Stop PWM audio
 *
 * Disables the timer used for PWM audio generation.
 */
void PWMAud_stop(void);
/**
 * @brief Set PWM audio volume level
 *
 * Adjusts the audio volume scaling factor.
 *
 * @param[in] volScale Volume level (0-5)
 */
void PWMAud_setvolume(uint16_t volScale);

/**
 * @brief Get current PWM audio volume level
 *
 * Retrieves the current audio volume scaling factor.
 *
 * @return Current volume level as a scaling factor
 */
uint16_t PWMAud_getvolume(void);

/**
 * @brief Transmit PWM audio data
 *
 * Sends PCM audio data for playback through PWM audio interface.
 *
 * @param[in] memAddr Pointer to PCM audio data buffer
 * @param[in] trunkSize Size of PCM audio data in bytes
 */
void PWMAud_transmit(uint8_t *memAddr, uint32_t trunkSize);

/**
 * @brief Set PWM audio state
 *
 * Manually sets the current state of the PWM audio driver.
 *
 * @param[in] state Desired PWM audio state
 */
void PWMAud_set_state(PWMAud_state_e state);

/**
 * @brief Get current PWM audio state
 *
 * Retrieves the current state of the PWM audio driver.
 *
 * @return Current PWM audio state
 */
PWMAud_state_e PWMAud_get_state(void);

#endif

