#ifndef __LIOT_I2S_H_
#define __LIOT_I2S_H_

typedef enum {
    L_I2S_ERR_SUCCESS  = 0,
    L_I2S_ERR_OPEN,
    L_I2S_ERR_NOMEM,
    L_I2S_ERR_NOTFOUND,
    L_I2S_ERR_CTRLSTATE,
} Liot_I2sErr_e;

typedef enum 
{
    L_I2S_08K_SAMPLES,  // set to  8k samples per second 
    L_I2S_16K_SAMPLES,  // set to 16k samples in per second 
    L_I2S_22K_SAMPLES,  // set to 22.05k samples per second 
    L_I2S_24K_SAMPLES,  // set to 24k samples per second 
    L_I2S_32K_SAMPLES,  // set to 32k samples in per second 
    L_I2S_44K_SAMPLES,  // set to 44.1k samples per second 
    L_I2S_48K_SAMPLES,  // set to 48k samples per second 
    L_I2S_96K_SAMPLES,  // set to 96k samples per second 
} Liot_I2sSample_e;

typedef enum
{
    L_I2S_FRAMESIZE_16_16        = 0,        ///< WordSize 16bit, SlotSize 16bit
    L_I2S_FRAMESIZE_16_32        = 1,        ///< WordSize 16bit, SlotSize 32bit
    L_I2S_FRAMESIZE_24_32        = 2,        ///< WordSize 24bit, SlotSize 32bit
    L_I2S_FRAMESIZE_32_32        = 3,        ///< WordSize 32bit, SlotSize 32bit
} Liot_I2sFrameSize_e;

typedef enum 
{
    L_I2S_BIT_LENGTH_16BITS = 0,   // set 16 bits per sample
    L_I2S_BIT_LENGTH_24BITS = 2,   // set 24 bits per sample
    L_I2S_BIT_LENGTH_32BITS = 3,   // set 32 bits per sample
} Liot_I2sBitsLen_e;

// Select I2S interface format for audio codec chip
typedef enum 
{
    L_I2S_MODE_MSB,    ///< Left aligned mode
    L_I2S_MODE_LSB,    ///< Right aligned mode
    L_I2S_MODE_I2S,    ///< I2S mode
    L_I2S_MODE_PCM,    ///< PCM mode
} Liot_I2sMode_e;

typedef enum
{
    L_ROLE_I2S_MASTER,   ///< I2S is master
    L_ROLE_I2S_SLAVE,    ///< I2S is slave 
} Liot_I2sRole_e;

// codec channel
typedef enum
{
    L_I2S_DUAL = 1,
    L_I2S_MONO_LEFT,
    L_I2S_MONO_RIGHT,
} Liot_I2sChannel_e;

typedef enum
{
    L_I2S_BCLK_POLARITY_0    = 0,
    L_I2S_BCLK_POLARITY_1    = 1,
} Liot_I2sBclkPolarity_e;

typedef enum
{
    L_I2S_PLAY          = 0,        ///< Audio play once
    L_I2S_RECORD        = 1,        ///< Audio record
    L_I2S_PLAY_RECORD   = 2,        ///< Audio play/record
} Liot_I2sPlayRecord_e;

typedef enum
{
    L_I2S_STOP_SEND     = 0,        // only stop send
    L_I2S_STOP_RECV     = 1,        // only stop recv
    L_I2S_STOP_ALL      = 2,        // stop send and recv
    L_I2S_START_SEND    = 3,        // only start send   
    L_I2S_START_RECV    = 4,        // only start recv 
    L_I2S_START_ALL     = 5,        // start send and recv
} Liot_I2sStartStop_e;

typedef void (*Liot_FuncCB) (uint32_t event, uint32_t arg);  ///< I2S init callback event.

typedef struct {
    Liot_I2sChannel_e channel;
    Liot_I2sRole_e role;
    Liot_I2sMode_e mode;
    Liot_I2sFrameSize_e frameSize;
    Liot_I2sSample_e samples;
    Liot_I2sBclkPolarity_e polarity;
    Liot_I2sPlayRecord_e playRecord;
    Liot_FuncCB txCb;
    Liot_FuncCB rxCb;
} Liot_I2sConfig_e;

Liot_I2sErr_e Liot_I2sInit(int8_t i2sNum, Liot_I2sConfig_e *config);
Liot_I2sErr_e Liot_I2sDeInit(int8_t i2sNum);
Liot_I2sErr_e Liot_I2sSend(int8_t i2sNum, uint8_t *data, uint32_t len);
Liot_I2sErr_e Liot_I2sReceive(int8_t i2sNum, uint8_t *data, uint32_t len);
Liot_I2sErr_e Liot_I2sStartStop(int8_t i2sNum, Liot_I2sStartStop_e startstop);
void Liot_I2sAdjustVolumn(int16_t* srcBuf, uint32_t srcTotalNum, uint16_t volScale);

#endif