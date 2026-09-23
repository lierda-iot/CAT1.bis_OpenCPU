#include "liot_type.h"
#include "liot_audio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include <string.h>

#define DEMO_3A_LOOPBACK_CHUNK      (1280)
#define DEMO_3A_LOOPBACK_RING_SIZE  (DEMO_3A_LOOPBACK_CHUNK * 8)
#define DEMO_3A_LOOPBACK_RETRY_MS   (10)

__attribute__((aligned(16))) static uint8_t demo3aLoopbackRing[DEMO_3A_LOOPBACK_RING_SIZE] = {0};
__attribute__((aligned(16))) static uint8_t demo3aLoopbackChunk[DEMO_3A_LOOPBACK_CHUNK] = {0};

static volatile uint32_t demo3aLoopbackRd = 0;
static volatile uint32_t demo3aLoopbackWr = 0;
static volatile uint32_t demo3aLoopbackPlayCnt = 0;
static volatile uint32_t demo3aLoopbackDropCnt = 0;

static void demo3aLoopbackRingReset(void)
{
    demo3aLoopbackRd = 0;
    demo3aLoopbackWr = 0;
    demo3aLoopbackPlayCnt = 0;
    demo3aLoopbackDropCnt = 0;
}

static uint32_t demo3aLoopbackRingUsed(void)
{
    uint32_t rd = demo3aLoopbackRd;
    uint32_t wr = demo3aLoopbackWr;

    return (wr >= rd) ? (wr - rd) : (DEMO_3A_LOOPBACK_RING_SIZE - rd + wr);
}

static uint32_t demo3aLoopbackRingFree(void)
{
    return DEMO_3A_LOOPBACK_RING_SIZE - demo3aLoopbackRingUsed() - 1;
}

static uint32_t demo3aLoopbackRingWrite(uint8_t *data, uint32_t len)
{
    uint32_t first = 0;

    if (demo3aLoopbackRingFree() < len)
    {
        demo3aLoopbackDropCnt++;
        return 0;
    }

    first = DEMO_3A_LOOPBACK_RING_SIZE - demo3aLoopbackWr;
    if (first > len)
        first = len;
    memcpy(demo3aLoopbackRing + demo3aLoopbackWr, data, first);
    if (len > first)
        memcpy(demo3aLoopbackRing, data + first, len - first);

    demo3aLoopbackWr = (demo3aLoopbackWr + len) % DEMO_3A_LOOPBACK_RING_SIZE;
    return len;
}

static uint32_t demo3aLoopbackRingRead(uint8_t *data, uint32_t len)
{
    uint32_t first = 0;

    if (demo3aLoopbackRingUsed() < len)
        return 0;

    first = DEMO_3A_LOOPBACK_RING_SIZE - demo3aLoopbackRd;
    if (first > len)
        first = len;
    memcpy(data, demo3aLoopbackRing + demo3aLoopbackRd, first);
    if (len > first)
        memcpy(data + first, demo3aLoopbackRing, len - first);

    demo3aLoopbackRd = (demo3aLoopbackRd + len) % DEMO_3A_LOOPBACK_RING_SIZE;
    return len;
}

static void demo3aLoopbackRecordCb(uint8_t *pcmData, uint16_t len)
{
    demo3aLoopbackRingWrite(pcmData, len);
}

static void demo3aTask(void *argv)
{
    (void)argv;

    liot_rtos_task_sleep_ms(2000);

    Liot_AonPowerCtl(TRUE);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);

    Liot_AudHwConfig_t cfg = {
        .i2cNum = 0,
        .i2sNum = 0,
        .paGpioNum = 11,
        .codecType = L_AUD_ES8311,
        .channel = L_AUD_MONO_RIGHT,
        .role = L_AUD_ROLE_SLAVE,
        .mode = L_AUD_MODE_I2S,
        .frameSize = L_AUD_FRAMESIZE_16_16,
        .samples = L_AUD_16K_SAMPLES,
        .use3A = 1,
    };

    Liot_AudAnsConfig_t ans = { .bypass = 0, .mode = 1 };
    Liot_AudAgcConfig_t agc = { .bypass = 0, .targetLevel = 3, .compressionGain = 6, .limiterEnable = 1 };
    Liot_AudAecConfig_t aec = { .bypass = 0, .delay = 10, .cngMode = 1, .echoMode = 3, .nlpFlag = 1 };

    Liot_AudioSetAns(&ans);
    Liot_AudioSetAgc(&agc);
    Liot_AudioSetAec(&aec);

    liot_trace("Audio 3A init");
    Liot_AudioInit(&cfg);
    Liot_AudioSetVolume(50);
    Liot_AudioSetCodecVolume(50);
    Liot_AudioSetMicVolume(8, 200);

    memset(demo3aLoopbackRing, 0, sizeof(demo3aLoopbackRing));
    memset(demo3aLoopbackChunk, 0, sizeof(demo3aLoopbackChunk));
    demo3aLoopbackRingReset();

    liot_trace("Audio 3A loopback start");
    Liot_AudioRecordStream(demo3aLoopbackRecordCb);

    while (1)
    {
        liot_rtos_task_sleep_ms(DEMO_3A_LOOPBACK_RETRY_MS);

        while (demo3aLoopbackRingRead(demo3aLoopbackChunk, DEMO_3A_LOOPBACK_CHUNK) == DEMO_3A_LOOPBACK_CHUNK)
        {
            while (Liot_AudioPlayStream(demo3aLoopbackChunk, DEMO_3A_LOOPBACK_CHUNK) != L_AUD_ERR_SUCCESS)
                liot_rtos_task_sleep_ms(DEMO_3A_LOOPBACK_RETRY_MS);

            demo3aLoopbackPlayCnt++;
            if ((demo3aLoopbackPlayCnt % 100) == 0)
                liot_trace("Audio 3A loopback play=%d drop=%d", demo3aLoopbackPlayCnt, demo3aLoopbackDropCnt);
        }
    }
}

void user_main(void)
{
    liot_trace("NT26FxDx_OpenKit 3A demo start");

    liot_task_t demo3aHandle = NULL;
    liot_rtos_task_create(&demo3aHandle, 10240, LIOT_APP_TASK_PRIORITY,
                          "demo_3a", demo3aTask, NULL);
}
