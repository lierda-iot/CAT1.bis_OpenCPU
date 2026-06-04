#ifndef __LIOT_I2S_TYPE_H
#define __LIOT_I2S_TYPE_H

typedef enum
{
    LIOT_I2S_SLAVE_MODE          = 0,        ///< I2S is slave 
    LIOT_I2S_MASTER_MODE         = 1,        ///< I2S is master
} liotI2sRole_e;

typedef enum
{
    LIOT_MSB_MODE                = 0,        ///< Left aligned mode
    LIOT_LSB_MODE                = 1,        ///< Right aligned mode
    LIOT_I2S_MODE                = 2,        ///< I2S mode
    LIOT_PCM_MODE                = 3,        ///< PCM mode
}liotI2sMode_e;

typedef enum
{
    LIOT_STOP_I2S                = 0,
    LIOT_ONLY_SEND               = 1,
    LIOT_ONLY_RECV               = 2,
    LIOT_SEND_RECV               = 3,
}liotI2sCtrlMode_e;

typedef enum 
{
    LIOT_I2S_POWER_OFF,                                      // Power off: no operation possible
    LIOT_I2S_POWER_FULL                                      // Power on: full operation at maximum performance
} liotI2sPowerState_e;

typedef enum
{
    LIOT_SAMPLERATE_8K           = 0,        ///< Sample rate 8k
    LIOT_SAMPLERATE_16K          = 1,        ///< Sample rate 16k
    LIOT_SAMPLERATE_22_05K       = 2,        ///< Sample rate 22.05k
    LIOT_SAMPLERATE_24K	        = 3,        ///< Sample rate 24k
    LIOT_SAMPLERATE_32K          = 4,        ///< Sample rate 32k
    LIOT_SAMPLERATE_44_1K        = 5,        ///< Sample rate 44.1k
    LIOT_SAMPLERATE_48K          = 6,        ///< Sample rate 48k
    LIOT_SAMPLERATE_96K          = 7,        ///< Sample rate 96k
} liotI2sSampleRate_e;

typedef enum
{
    LIOT_FRAME_SIZE_16_16        = 0,        ///< WordSize 16bit, SlotSize 16bit
    LIOT_FRAME_SIZE_16_32        = 1,        ///< WordSize 16bit, SlotSize 32bit
    LIOT_FRAME_SIZE_24_32        = 2,        ///< WordSize 24bit, SlotSize 32bit
    LIOT_FRAME_SIZE_32_32        = 3,        ///< WordSize 32bit, SlotSize 32bit
} liotI2sFrameSize_e;

typedef enum
{
    LIOT_PLAY                    = 0,        ///< Audio play once
    LIOT_RECORD                  = 1,        ///< Audio record
    LIOT_PLAY_RECORD             = 2,        ///< Audio play/record
    LIOT_PLAY_LOOP               = 3,        ///< Audio play loop
    LIOT_PLAY_LOOP_IRQ           = 4,        ///< Audio play loop irq
    LIOT_RECORD_LOOP_IRQ         = 5,        ///< Audio record loop irq
} liotI2sPlayRecord_e;

typedef enum
{
    LIOT_MONO                    = 0,
    LIOT_DUAL_CHANNEL            = 1,
} liotI2sChannelSel_e;

typedef enum
{
    LIOT_I2S_TX                  = 0,
    LIOT_I2S_RX                  = 1,
} liotI2sDirectionSel_e;

typedef enum
{
    LIOT_POLARITY_0              = 0,
    LIOT_POLARITY_1              = 1,
} liotI2sBclkPolarity_e;

typedef struct
{
    liotI2sMode_e               mode;       ///< Audio mode choose
    liotI2sRole_e               role;       ///< Role choose
    liotI2sSampleRate_e         sampleRate; ///< Sample rate choose
    liotI2sFrameSize_e          frameSize;  ///< Frame size choose
    liotI2sBclkPolarity_e       polarity;   ///< Bclk polarity choose
    liotI2sChannelSel_e         channelSel; ///< Mono or dual channel select
    uint32_t                totalNum;   ///< Audio source total num
} liotI2sParamCtrl_t;

typedef enum
{
    LIOT_STOP_SEND               = 0,        // only stop send
    LIOT_STOP_RECV               = 1,        // only stop recv
    LIOT_STOP_ALL                = 2,        // stop send and recv
    LIOT_START_SEND              = 3,        // only start send   
    LIOT_START_RECV              = 4,        // only start recv 
    LIOT_START_ALL               = 5,        // start send and recv
} liotI2sStartStop_e;

typedef enum
{
    LIOT_VOLUMN_INCREASE         = 0,        ///< Volumn increase
    LIOT_VOLUMN_DECREASE         = 1,        ///< Volumn decrease
} liotI2sVolumnCtrl_e;

typedef void (*liot_i2sCbFunc_fn) (uint32_t event, uint32_t arg);  ///< I2S init callback event.

typedef void (*liot_i2sUspFunc_fn)(void); // I2S callback

#endif /* _LIOT_I2S_H */
