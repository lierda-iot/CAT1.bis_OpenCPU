/**
 * @file liot_sound.h
 * @brief Audio Driver Interface for Lierda IoT Platform
 *
 * @author Suxx <ciot_iot_support@lierda.com>
 * @version 1.0
 * @date 2026-02-24
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

#ifndef __LIOT_SOUND_H_
#define __LIOT_SOUND_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum Liot_SndErr_e
 * @brief Audio driver error codes
 */
typedef enum {
    L_SND_ERR_SUCCESS  = 0,     /*!< Operation was successful */
    L_SND_ERR_EXECUTE,          /*!< General execution error */
    L_SND_ERR_INVALID_PARAM,    /*!< Invalid input parameter */
    L_SND_ERR_OPEN,             /*!< Failed to open device */
    L_SND_ERR_CONFIG,           /*!< Configuration failed */
    L_SND_ERR_PULL_SET,         /*!< Pull resistor setup failed */
    L_SND_ERR_CALLBACK,         /*!< Callback registration failed */
    L_SND_ERR_LEVEL_TRIGGER,    /*!< Level trigger configuration failed */
    L_SND_ERR_NOMEM,            /*!< Out of memory */
    L_SND_ERR_FILE,             /*!< File operation error */
} Liot_SndErr_e;

/**
 * @enum Liot_SndDevice_e
 * @brief Supported audio codec devices
 */
typedef enum {
    L_SND_DEV_NONE,     /*!< No sound device */
    L_SND_TM8211,       /*!< TM8211 audio codec */
    L_SND_ES8311,       /*!< ES8311 audio codec */
    L_SND_ES8374,       /*!< ES8374 audio codec */
    L_SND_ES8375,       /*!< ES8375 audio codec */
    L_SND_DEV_MAX,      /*!< Maximum number of supported devices */
} Liot_SndDevice_e;

/**
 * @enum Liot_SndSample_e
 * @brief Audio sample rates
 */
typedef enum 
{
    L_SND_08K_SAMPLES,  /*!< 8 kHz sample rate */
    L_SND_16K_SAMPLES,  /*!< 16 kHz sample rate */
    L_SND_22K_SAMPLES,  /*!< 22.05 kHz sample rate */
    L_SND_24K_SAMPLES,  /*!< 24 kHz sample rate */
    L_SND_32K_SAMPLES,  /*!< 32 kHz sample rate */
    L_SND_44K_SAMPLES,  /*!< 44.1 kHz sample rate */
    L_SND_48K_SAMPLES,  /*!< 48 kHz sample rate */
    L_SND_96K_SAMPLES,  /*!< 96 kHz sample rate */
} Liot_SndSample_e;

/**
 * @enum Liot_SndFrameSize_e
 * @brief Audio frame size configurations
 */
typedef enum
{
    L_SND_FRAMESIZE_16_16 = 0,  /*!< WordSize 16bit, SlotSize 16bit */
    L_SND_FRAMESIZE_16_32 = 1,  /*!< WordSize 16bit, SlotSize 32bit */
    L_SND_FRAMESIZE_24_32 = 2,  /*!< WordSize 24bit, SlotSize 32bit */
    L_SND_FRAMESIZE_32_32 = 3,  /*!< WordSize 32bit, SlotSize 32bit */
} Liot_SndFrameSize_e;

/**
 * @enum Liot_SndBitsLen_e
 * @brief Audio bit depths
 */
typedef enum 
{
    L_SND_BIT_LENGTH_16BITS = 0,  /*!< 16 bits per sample */
    L_SND_BIT_LENGTH_24BITS = 2,  /*!< 24 bits per sample */
    L_SND_BIT_LENGTH_32BITS = 3,  /*!< 32 bits per sample */
} Liot_SndBitsLen_e;

/**
 * @enum Liot_SndMode_e
 * @brief I2S interface formats
 */
typedef enum 
{
    L_SND_MODE_MSB,  /*!< Left aligned mode */
    L_SND_MODE_LSB,  /*!< Right aligned mode */
    L_SND_MODE_I2S,  /*!< I2S mode */
    L_SND_MODE_PCM,  /*!< PCM mode */
} Liot_SndMode_e;

/**
 * @enum Liot_SndRole_e
 * @brief I2S master/slave roles
 */
typedef enum
{
    L_SND_ROLE_MASTER,  /*!< I2S is master */
    L_SND_ROLE_SLAVE,   /*!< I2S is slave */
} Liot_SndRole_e;

/**
 * @enum Liot_SndChannel_e
 * @brief Audio channel configurations
 */
typedef enum
{
    L_SND_DUAL = 1,      /*!< Dual channel (stereo) */
    L_SND_MONO_LEFT,     /*!< Mono - left channel only */
    L_SND_MONO_RIGHT,    /*!< Mono - right channel only */
} Liot_SndChannel_e;

/**
 * @enum Liot_SndState_e
 * @brief Audio driver states
 */
typedef enum
{
    L_SND_STA_DEINIT,   /*!< Driver deinitialized */
    L_SND_STA_IDLE,     /*!< Driver idle */
    L_SND_STA_PLAYING,  /*!< Audio playing */
    L_SND_STA_PAUSE,    /*!< Audio paused */
} Liot_SndState_e;

/**
 * @enum Liot_SndVolStep_e
 * @brief Volume scaling step modes
 */
typedef enum {
    L_SND_VOL_STEP_0_1 = 0,  /*!< Volume step in 0.1 increments */
    L_SND_VOL_STEP_0_01,     /*!< Volume step in 0.01 increments */
} Liot_SndVolStep_e;

/**
 * @struct Liot_SndHwConfig_t
 * @brief Audio hardware configuration structure
 */
typedef struct
{
    int8_t i2cNum;               /*!< I2C bus number */
    int8_t i2sNum;               /*!< I2S interface number */
    int8_t paGpioNum;            /*!< Power Amplifier GPIO number */
    Liot_SndDevice_e codecType;  /*!< Audio codec type */
    Liot_SndChannel_e channel;   /*!< Audio channel configuration */
    Liot_SndRole_e role;         /*!< I2S master/slave role */
    Liot_SndMode_e mode;         /*!< I2S interface mode */
    Liot_SndFrameSize_e frameSize; /*!< Audio frame size configuration */
    Liot_SndSample_e samples;    /*!< Audio sample rate */
} Liot_SndHwConfig_t;

/**
 * @brief Initialize the audio driver
 * @param config Pointer to hardware configuration structure
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundInit(Liot_SndHwConfig_t *config);

/**
 * @brief Deinitialize the audio driver
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundDeInit(void);

/**
 * @brief Set codec playback volume via hardware register
 * @param Volume Volume level (range depends on codec)
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundSetCodecVolume(int Volume);

/**
 * @brief Get current codec playback volume from hardware register
 * @param Volume Pointer to store current volume level
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundGetCodecVolume(int *Volume);

/**
 * @brief Set software playback volume
 * @param Volume Volume level (0-100)
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundSetVolume(int Volume);

/**
 * @brief Get current playback volume
 * @param Volume Pointer to store current volume level
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundGetVolume(int *Volume);

/**
 * @brief Set microphone volume and gain
 * @param micGain Microphone gain level
 * @param micVolume Microphone volume level
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundSetMicVolume(uint8_t micGain, int micVolume);

/**
 * @brief Set volume scaling step mode
 * @param mode Volume step mode (0.1 or 0.01 increments)
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundSetVolMode(Liot_SndVolStep_e mode);

/**
 * @brief Play audio data
 * @param data Pointer to audio data buffer
 * @param datalen Length of audio data in bytes
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlay(uint8_t* data, int datalen);

/**
 * @brief Record audio data
 * @param data Pointer to buffer to store recorded audio
 * @param datalen Length of buffer in bytes
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundRecord(uint8_t* data, int datalen);

/**
 * @brief Pause audio playback
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlayPause(void);

/**
 * @brief Resume audio playback
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlayResume(void);

/**
 * @brief Play MP3 file
 * @param fileName Path to MP3 file
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlayMp3File(char* fileName);

/**
 * @brief Play MP3 data from memory buffer
 * @param data Pointer to MP3 data buffer
 * @param datalen Length of MP3 data in bytes
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlayMp3(uint8_t* data, int datalen);

/**
 * @brief Play WAV file from filesystem
 * @param fileName Path to WAV file
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlayWavFile(char* fileName);

/**
 * @brief Play WAV data from memory buffer
 * @param data Pointer to WAV data (including header)
 * @param datalen Total length of WAV data in bytes
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundPlayWav(uint8_t* data, int datalen);

/**
 * @brief Start MP3 streaming playback session
 * @note  Call once before feeding MP3 data with Liot_SoundPlayMp3Stream.
 *        Initializes the decoder only once to avoid gaps between segments.
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundMp3StreamStart(void);

/**
 * @brief Feed MP3 data to the streaming decoder for playback
 * @param data Pointer to MP3 data buffer
 * @param datalen Length of MP3 data in bytes
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundMp3StreamPlay(uint8_t* data, int datalen);

/**
 * @brief Stop MP3 streaming playback and release decoder resources
 * @return L_SND_ERR_SUCCESS if successful, otherwise error code
 */
Liot_SndErr_e Liot_SoundMp3StreamStop(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif