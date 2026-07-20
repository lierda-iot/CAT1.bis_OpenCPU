/**
 * @file dual_eye_vpu_adapter.c
 * @brief Minimal VPU platform symbols required by the multimedia GIF decoder.
 */

#include <stddef.h>
#include <stdint.h>

#define DUAL_EYE_ARM_DRIVER_OK              0
#define DUAL_EYE_ARM_DRIVER_ERROR_PARAMETER (-5)

#define DUAL_EYE_CLK_RAI(id) (((id) >> 13U) & 0xFU)
#define DUAL_EYE_CLK_BP(id)  (((id) >> 8U) & 0x1FU)

#define DUAL_EYE_SIPC_RES_BASE 0x4F010100UL
#define DUAL_EYE_SIPC_RES_STEP 0x100UL

#define DUAL_EYE_GPR_TOP_BASE     0x40000000UL
#define DUAL_EYE_GPR_VPU_SEL_REG  0x40000070UL
#define DUAL_EYE_GPR_VPU_INIT_REG 0x40000024UL
#define DUAL_EYE_GPR_VPU_INIT_BIT (1UL << 2)

static volatile uint32_t *const dual_eye_clk_regs[] = {
    (volatile uint32_t *)0x40000000UL,
    (volatile uint32_t *)0x40000008UL,
    (volatile uint32_t *)0x4F000014UL,
    (volatile uint32_t *)0x4F000190UL,
    (volatile uint32_t *)0x4F000018UL,
    (volatile uint32_t *)0x4F000030UL,
    (volatile uint32_t *)0x4F000038UL,
    (volatile uint32_t *)0x4F000040UL,
    (volatile uint32_t *)0x4F000050UL,
    (volatile uint32_t *)0x4F000058UL,
};

static void dual_eye_clock_bit_set(uint32_t id, uint32_t enabled)
{
    uint32_t rai = DUAL_EYE_CLK_RAI(id);
    uint32_t mask = 1UL << DUAL_EYE_CLK_BP(id);
    volatile uint32_t *reg;

    if (rai >= (sizeof(dual_eye_clk_regs) / sizeof(dual_eye_clk_regs[0]))) {
        return;
    }

    reg = dual_eye_clk_regs[rai];
    if (enabled != 0U) {
        *reg |= mask;
    } else {
        *reg &= ~mask;
    }
}

int32_t CLOCK_clockEnable(uint32_t id)
{
    if (DUAL_EYE_CLK_RAI(id) >=
        (sizeof(dual_eye_clk_regs) / sizeof(dual_eye_clk_regs[0]))) {
        return DUAL_EYE_ARM_DRIVER_ERROR_PARAMETER;
    }

    dual_eye_clock_bit_set(id, 1U);
    return DUAL_EYE_ARM_DRIVER_OK;
}

void CLOCK_clockDisable(uint32_t id)
{
    dual_eye_clock_bit_set(id, 0U);
}

void GPR_vpuInit(void)
{
    *(volatile uint32_t *)DUAL_EYE_GPR_VPU_SEL_REG = 1U;
    *(volatile uint32_t *)DUAL_EYE_GPR_VPU_INIT_REG |= DUAL_EYE_GPR_VPU_INIT_BIT;
}

void GPR_vpuSel(uint32_t sel)
{
    *(volatile uint32_t *)DUAL_EYE_GPR_VPU_SEL_REG = sel;
}

uint32_t SIPC_ResLock(uint32_t res)
{
    volatile uint32_t *reg = (volatile uint32_t *)(DUAL_EYE_SIPC_RES_BASE +
                                                   (res * DUAL_EYE_SIPC_RES_STEP));

    return *reg & 1U;
}

void SIPC_ResRelease(uint32_t res)
{
    volatile uint32_t *reg = (volatile uint32_t *)(DUAL_EYE_SIPC_RES_BASE +
                                                   (res * DUAL_EYE_SIPC_RES_STEP));

    *reg = 1U;
}

int32_t slpManRegisterPredefinedBackupCb(uint32_t module, void *backup_cb,
                                         void *pdata)
{
    (void)module;
    (void)backup_cb;
    (void)pdata;

    return 0;
}

int32_t slpManRegisterPredefinedRestoreCb(uint32_t module, void *restore_cb,
                                          void *pdata)
{
    (void)module;
    (void)restore_cb;
    (void)pdata;

    return 0;
}

int32_t slpManDrvVoteSleep(uint32_t module, uint32_t status)
{
    (void)module;
    (void)status;

    return 0;
}

uint8_t PSRAM_dmaAccessClkCtrl(uint8_t onoff)
{
    (void)onoff;

    return 0;
}

void TMU_APTimeRead(uint32_t *sys_time, uint32_t time_type)
{
    volatile uint32_t *tmu = (volatile uint32_t *)0x4F070000UL;
    uint32_t low = 0U;
    uint32_t high = 0U;

    if (sys_time == NULL) {
        return;
    }

    if (time_type == 0U) {
        low = tmu[0xC0U / sizeof(uint32_t)];
        high = tmu[0xC4U / sizeof(uint32_t)];
    } else if (time_type == 1U) {
        tmu[0x70U / sizeof(uint32_t)] = 1U;
        low = tmu[0xD4U / sizeof(uint32_t)];
        high = tmu[0xD8U / sizeof(uint32_t)];
        tmu[0x70U / sizeof(uint32_t)] = 0U;
    }

    sys_time[0] = high;
    sys_time[1] = low;
}
