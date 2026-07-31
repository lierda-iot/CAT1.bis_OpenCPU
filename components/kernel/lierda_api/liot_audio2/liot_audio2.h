/**
 * @file liot_audio.h
 * @brief Audio Driver Interface for Lierda IoT Platform
 *
 * @author Suxx <ciot_iot_support@lierda.com>
 * @version 1.0
 * @date 2026-02-24
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

#ifndef __LIOT_AUDIO_H_
#define __LIOT_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum Liot_AudErr_e
 * @brief Audio driver error codes
 */
typedef enum {
    L_AUD_ERR_SUCCESS  = 0,     /*!< Operation was successful */
    L_AUD_ERR_EXECUTE,          /*!< General execution error */
    L_AUD_ERR_INVALID_PARAM,    /*!< Invalid input parameter */
    L_AUD_ERR_OPEN,             /*!< Failed to open device */
    L_AUD_ERR_CONFIG,           /*!< Configuration failed */
    L_AUD_ERR_PULL_SET,         /*!< Pull resistor setup failed */
    L_AUD_ERR_CALLBACK,         /*!< Callback registration failed */
    L_AUD_ERR_LEVEL_TRIGGER,    /*!< Level trigger configuration failed */
    L_AUD_ERR_NOMEM,            /*!< Out of memory */
    L_AUD_ERR_FILE,             /*!< File operation error */
} Liot_AudErr_e;

/**
 * @enum Liot_AudDevice_e
 * @brief Supported audio codec devices
 */
typedef enum {
    L_AUD_DEV_NONE,     /*!< No audio device */
    L_AUD_TM8211,       /*!< TM8211 audio codec */
    L_AUD_ES8311,       /*!< ES8311 audio codec */
    L_AUD_ES8374,       /*!< ES8374 audio codec */
    L_AUD_ES8375,       /*!< ES8375 audio codec */
    L_AUD_SPI_CODEC,     /*!< SPI codec */
    L_AUD_DEV_MAX,      /*!< Maximum number of supported devices */
} Liot_AudDevice_e;

/**
 * @enum Liot_AudSample_e
 * @brief Audio sample rates
 */
typedef enum
{
    L_AUD_08K_SAMPLES,  /*!< 8 kHz sample rate */
    L_AUD_16K_SAMPLES,  /*!< 16 kHz sample rate */
    L_AUD_22K_SAMPLES,  /*!< 22.05 kHz sample rate */
    L_AUD_24K_SAMPLES,  /*!< 24 kHz sample rate */
    L_AUD_32K_SAMPLES,  /*!< 32 kHz sample rate */
    L_AUD_44K_SAMPLES,  /*!< 44.1 kHz sample rate */
    L_AUD_48K_SAMPLES,  /*!< 48 kHz sample rate */
    L_AUD_96K_SAMPLES,  /*!< 96 kHz sample rate */
} Liot_AudSample_e;

/**
 * @enum Liot_AudFrameSize_e
 * @brief Audio frame size configurations
 */
typedef enum
{
    L_AUD_FRAMESIZE_16_16 = 0,  /*!< WordSize 16bit, SlotSize 16bit */
    L_AUD_FRAMESIZE_16_32 = 1,  /*!< WordSize 16bit, SlotSize 32bit */
    L_AUD_FRAMESIZE_24_32 = 2,  /*!< WordSize 24bit, SlotSize 32bit */
    L_AUD_FRAMESIZE_32_32 = 3,  /*!< WordSize 32bit, SlotSize 32bit */
} Liot_AudFrameSize_e;

/**
 * @enum Liot_AudBitsLen_e
 * @brief Audio bit depths
 */
typedef enum
{
    L_AUD_BIT_LENGTH_16BITS = 0,  /*!< 16 bits per sample */
    L_AUD_BIT_LENGTH_24BITS = 2,  /*!< 24 bits per sample */
    L_AUD_BIT_LENGTH_32BITS = 3,  /*!< 32 bits per sample */
} Liot_AudBitsLen_e;

/**
 * @enum Liot_AudMode_e
 * @brief I2S interface formats
 */
typedef enum
{
    L_AUD_MODE_MSB,  /*!< Left aligned mode */
    L_AUD_MODE_LSB,  /*!< Right aligned mode */
    L_AUD_MODE_I2S,  /*!< I2S mode */
    L_AUD_MODE_PCM,  /*!< PCM mode */
} Liot_AudMode_e;

/**
 * @enum Liot_AudRole_e
 * @brief I2S master/slave roles
 */
typedef enum
{
    L_AUD_ROLE_MASTER,  /*!< I2S is master */
    L_AUD_ROLE_SLAVE,   /*!< I2S is slave */
} Liot_AudRole_e;

/**
 * @enum Liot_AudChannel_e
 * @brief Audio channel configurations
 */
typedef enum
{
    L_AUD_DUAL = 1,      /*!< Dual channel (stereo) */
    L_AUD_MONO_LEFT,     /*!< Mono - left channel only */
    L_AUD_MONO_RIGHT,    /*!< Mono - right channel only */
} Liot_AudChannel_e;

/**
 * @enum Liot_AudState_e
 * @brief Audio driver states
 */
typedef enum
{
    L_AUD_STA_DEINIT,   /*!< Driver deinitialized */
    L_AUD_STA_IDLE,     /*!< Driver idle */
    L_AUD_STA_PLAYING,  /*!< Audio playing */
    L_AUD_STA_PAUSE,    /*!< Audio paused */
    L_AUD_STA_RESUME,   /*!< Audio resumed */
    L_AUD_STA_STOP,     /*!< Audio stopped */
} Liot_AudState_e;

/**
 * @enum Liot_AudEvent_e
 * @brief Audio playback events (for callback)
 */
typedef enum {
    L_AUD_EVT_ERROR   = -1,  /*!< An error occurred during audio playback */
    L_AUD_EVT_START   = 0,   /*!< Audio playback started */
    L_AUD_EVT_PAUSE   = 1,   /*!< Audio playback paused */
    L_AUD_EVT_FINISH  = 2,   /*!< Audio playback finished */
    L_AUD_EVT_CLOSE   = 3,   /*!< Playing channel close */
    L_AUD_EVT_RESUME  = 4,   /*!< Audio playback resumed */
} Liot_AudEvent_e;

/**
 * @brief Audio event callback function type
 * @param event   The audio event that occurred
 * @param context User-defined context data
 */
typedef void (*Liot_AudCb_t)(Liot_AudEvent_e event, void *context);

/**
 * @enum Liot_AudVolStep_e
 * @brief Volume scaling step modes
 */
typedef enum {
    L_AUD_VOL_STEP_0_1 = 0,  /*!< Volume step in 0.1 increments */
    L_AUD_VOL_STEP_0_01,     /*!< Volume step in 0.01 increments */
} Liot_AudVolStep_e;

/**
 * @struct Liot_AudHwConfig_t
 * @brief Audio hardware configuration structure
 */
typedef struct
{
    int8_t i2cNum;               /*!< I2C bus number */
    int8_t i2sNum;               /*!< I2S interface number */
    int8_t spiNum;               /*!< SPI port for L_AUD_SPI_CODEC: 0 or 1 */
    int8_t paGpioNum;            /*!< Power Amplifier GPIO number */
    Liot_AudDevice_e codecType;  /*!< Audio codec type */
    Liot_AudChannel_e channel;   /*!< Audio channel configuration */
    Liot_AudRole_e role;         /*!< I2S master/slave role */
    Liot_AudMode_e mode;         /*!< I2S interface mode */
    Liot_AudFrameSize_e frameSize; /*!< Audio frame size configuration */
    Liot_AudSample_e samples;    /*!< Audio sample rate */
    Liot_AudCb_t callback;       /*!< Audio event callback (can be NULL) */
    void *cbContext;             /*!< User context passed to callback */
} Liot_AudHwConfig_t;

/**
 * @brief Initialize the audio driver
 * @param config Pointer to hardware configuration structure
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioInit(Liot_AudHwConfig_t *config);

/**
 * @brief Deinitialize the audio driver
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioDeInit(void);

/**
 * @brief Set codec playback volume via hardware register
 * @param Volume Volume level (range depends on codec)
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioSetCodecVolume(int Volume);

/**
 * @brief Get current codec playback volume from hardware register
 * @param Volume Pointer to store current volume level
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioGetCodecVolume(int *Volume);

/**
 * @brief Set software playback volume
 * @param Volume Volume level (0-100)
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioSetVolume(int Volume);

/**
 * @brief Get current playback volume
 * @param Volume Pointer to store current volume level
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioGetVolume(int *Volume);

/**
 * @brief Set microphone volume and gain
 * @param micGain Microphone gain level
 * @param micVolume Microphone volume level
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioSetMicVolume(uint8_t micGain, int micVolume);

/**
 * @brief Set volume scaling step mode
 * @param mode Volume step mode (0.1 or 0.01 increments)
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioSetVolMode(Liot_AudVolStep_e mode);

/**
 * @brief Play audio data
 * @param data Pointer to audio data buffer
 * @param datalen Length of audio data in bytes
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlay(uint8_t* data, int datalen);

/**
 * @brief Record audio data
 * @param data Pointer to buffer to store recorded audio
 * @param datalen Length of buffer in bytes
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioRecord(uint8_t* data, int datalen);

/**
 * @brief Pause audio playback
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayPause(void);

/**
 * @brief Resume audio playback
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayResume(void);

/**
 * @brief Play audio data from file
 * @param fileName Path to audio file
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayFile(char* fileName);

/**
 * @brief Wait for audio playback to finish
 * @param timeout Timeout in seconds, or LIOT_WAIT_FOREVER
 * @return L_AUD_ERR_SUCCESS if finished, L_AUD_ERR_EXECUTE on timeout
 */
Liot_AudErr_e Liot_AudioWaitPlayFinish(int timeout);

/**
 * @brief Stop audio playback
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioStop(void);

/**
 * @brief Play MP3 file
 * @param fileName Path to MP3 file
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayMp3File(char* fileName);

/**
 * @brief Play MP3 data from memory buffer
 * @param data Pointer to MP3 data buffer
 * @param datalen Length of MP3 data in bytes
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayMp3(uint8_t* data, int datalen);

/**
 * @brief Play WAV file from filesystem
 * @param fileName Path to WAV file
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayWavFile(char* fileName);

/**
 * @brief Play WAV data from memory buffer
 * @param data Pointer to WAV data (including header)
 * @param datalen Total length of WAV data in bytes
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioPlayWav(uint8_t* data, int datalen);

/**
 * @brief Start MP3 streaming playback session
 * @note  Call once before feeding MP3 data with Liot_AudioMp3StreamPlay.
 *        Initializes the decoder only once to avoid gaps between segments.
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioMp3StreamStart(void);

/**
 * @brief Feed MP3 data to the streaming decoder for playback
 * @param data Pointer to MP3 data buffer
 * @param datalen Length of MP3 data in bytes
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioMp3StreamPlay(uint8_t* data, int datalen);

/**
 * @brief Stop MP3 streaming playback and release decoder resources
 * @return L_AUD_ERR_SUCCESS if successful, otherwise error code
 */
Liot_AudErr_e Liot_AudioMp3StreamStop(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
