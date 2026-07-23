/**
 * @file liot_ch390.c
 * @brief CH390 multi-instance driver — full feature implementation (SPI mode)
 */

#include "liot_ch390.h"
#include "lierda_app_main.h"
#include "cmsis_os2.h"
#include "liot_gpio2.h"
#include <string.h>

extern void delay_us(uint32_t us);

/* ──── Minimal SPI driver interface (layout-compatible with ARM_DRIVER_SPI) ──── */
typedef void (*liot_ch390_spi_cb_t)(uint32_t event);

typedef struct {
    void    *GetVersion;
    void    *GetCapabilities;
    int32_t (*Initialize)(liot_ch390_spi_cb_t cb_event);
    int32_t (*Uninitialize)(void);
    int32_t (*PowerControl)(uint32_t state);
    int32_t (*Send)(const void *data, uint32_t num);
    int32_t (*Receive)(void *data, uint32_t num);
    int32_t (*Transfer)(const void *data_out, void *data_in, uint32_t num);
    uint32_t(*GetDataCount)(void);
    int32_t (*Control)(uint32_t control, uint32_t arg);
    void    *GetStatus;
} const liot_ch390_spi_drv_t;

#define LIOT_SPI_POWER_FULL             2
#define LIOT_SPI_MODE_MASTER            (0x01UL << 0)
#define LIOT_SPI_CPOL1_CPHA1            (3UL << 8)
#define LIOT_SPI_DATA_BITS(n)           (((n) & 0x3F) << 12)
#define LIOT_SPI_MSB_LSB                (0UL << 18)
#define LIOT_SPI_SS_MASTER_SW           (1UL << 19)

#define SPI_DONE_FLAG     0x01
#define SPI_WAIT_TIMEOUT  5

typedef struct {
    uint8_t             used;
    uint8_t             spiPort;
    uint8_t             intWakeupPad;
    uint8_t             mac[6];
    liot_gpio_e         rstGpio;
    liot_gpio_e         ssnGpio;
    liot_ch390_spi_drv_t *spiDrv;
    osEventFlagsId_t    spiDoneFlag;
    void              (*intWakeupCb)(void *arg);
    void               *intWakeupCbArg;
} liot_ch390_ctx_t;

static liot_ch390_ctx_t gCh390Ctx[LIOT_CH390_MAX_INST];

extern void* __Liot_SpiGetInterface(int8_t Num);

static void spiCallback0(uint32_t event) {
    osEventFlagsSet(gCh390Ctx[0].spiDoneFlag, SPI_DONE_FLAG);
}

static void spiCallback1(uint32_t event) {
    osEventFlagsSet(gCh390Ctx[1].spiDoneFlag, SPI_DONE_FLAG);
}

static liot_ch390_spi_cb_t gSpiCallbacks[LIOT_CH390_MAX_INST] = {
    spiCallback0, spiCallback1
};

static inline liot_ch390_ctx_t *handle_to_ctx(Liot_Ch390Handle_t handle) {
    return (liot_ch390_ctx_t *)handle;
}

static void ctx_scs(liot_ch390_ctx_t *ctx, uint8_t level) {
    Liot_GpioSetLevel(ctx->ssnGpio, level);
}
static void ctx_rst(liot_ch390_ctx_t *ctx, uint8_t level) {
    Liot_GpioSetLevel(ctx->rstGpio, level);
}

static void ctx_wait_spi(liot_ch390_ctx_t *ctx) {
    osEventFlagsWait(ctx->spiDoneFlag, SPI_DONE_FLAG, osFlagsWaitAll, SPI_WAIT_TIMEOUT);
}

static uint8_t ctx_spi_exchange(liot_ch390_ctx_t *ctx, uint8_t byte) {
    uint8_t out = 0;
    ctx->spiDrv->Transfer(&byte, &out, 1);
    ctx_wait_spi(ctx);
    return out;
}

/* ──── Lifecycle ──── */

Liot_Ch390Handle_t Liot_Ch390Create(const Liot_Ch390Cfg_t *cfg) {
    if (!cfg || cfg->spiPort >= LIOT_CH390_MAX_INST) return NULL;

    liot_ch390_ctx_t *ctx = &gCh390Ctx[cfg->spiPort];
    if (ctx->used) return NULL;

    ctx->used = 1;
    ctx->spiPort = cfg->spiPort;
    ctx->rstGpio = (liot_gpio_e)cfg->rstGpio;
    ctx->ssnGpio = (liot_gpio_e)cfg->ssnGpio;
    ctx->intWakeupPad = cfg->intWakeupPad;
    ctx->intWakeupCb = cfg->intWakeupCb;
    ctx->intWakeupCbArg = cfg->intWakeupCbArg;
    memcpy(ctx->mac, cfg->mac, 6);

    ctx->spiDrv = (liot_ch390_spi_drv_t *)__Liot_SpiGetInterface(cfg->spiPort);
    if (!ctx->spiDrv) {
        ctx->used = 0;
        liot_trace("CH390[%d] spi driver not found", cfg->spiPort);
        return NULL;
    }

    ctx->spiDoneFlag = osEventFlagsNew(NULL);
    if (!ctx->spiDoneFlag) {
        ctx->used = 0;
        return NULL;
    }

    Liot_AonPowerCtl(TRUE);

    Liot_GpioInit(ctx->rstGpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioSetLevel(ctx->ssnGpio, L_IO_HIGH);

    ctx->spiDrv->Initialize(gSpiCallbacks[ctx->spiPort]);
    ctx->spiDrv->PowerControl(LIOT_SPI_POWER_FULL);
    ctx->spiDrv->Control(LIOT_SPI_MODE_MASTER |
                         LIOT_SPI_CPOL1_CPHA1 |
                         LIOT_SPI_DATA_BITS(8) |
                         LIOT_SPI_MSB_LSB |
                         LIOT_SPI_SS_MASTER_SW,
                         LIOT_CH390_SPI_SPEED);

    liot_wakeup_cfg_t wakeupCfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_FALL,
    };
    Liot_WakeupIntInit((liot_wakeuppad_e)ctx->intWakeupPad, wakeupCfg,
                       ctx->intWakeupCb, ctx->intWakeupCbArg);

    liot_trace("CH390[%d] created, spi=%d rstGpio=%d ssnGpio=%d WakeupPad=%d" , cfg->spiPort, cfg->spiPort, cfg->rstGpio, cfg->ssnGpio, cfg->intWakeupPad);
    return (Liot_Ch390Handle_t)ctx;
}

void Liot_Ch390Destroy(Liot_Ch390Handle_t handle) {
    if (!handle) return;
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);
    if (ctx->spiDoneFlag) {
        osEventFlagsDelete(ctx->spiDoneFlag);
        ctx->spiDoneFlag = NULL;
    }
    ctx->used = 0;
}

/* ──── Reset ──── */

void Liot_Ch390HardwareReset(Liot_Ch390Handle_t handle) {
    if (!handle) return;
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);
    ctx_rst(ctx, 0);
    delay_us(10);
    ctx_rst(ctx, 1);
    delay_us(10000);
}

void Liot_Ch390SoftwareReset(Liot_Ch390Handle_t handle) {
    Liot_Ch390WriteReg(handle, CH390_NCR, NCR_RST);
    delay_us(10);
    Liot_Ch390WriteReg(handle, CH390_NCR, 0);
    Liot_Ch390WriteReg(handle, CH390_NCR, NCR_RST);
    delay_us(10);
}
/* ──── Register access ──── */

uint8_t Liot_Ch390ReadReg(Liot_Ch390Handle_t handle, uint8_t reg) {
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);
    ctx_scs(ctx, 0);
    ctx_spi_exchange(ctx, OPC_REG_R | reg);
    uint8_t val = ctx_spi_exchange(ctx, 0x00);
    ctx_scs(ctx, 1);
    return val;
}

void Liot_Ch390WriteReg(Liot_Ch390Handle_t handle, uint8_t reg, uint8_t val) {
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);
    ctx_scs(ctx, 0);
    ctx_spi_exchange(ctx, OPC_REG_W | reg);
    ctx_spi_exchange(ctx, val);
    ctx_scs(ctx, 1);
}

/* ──── Memory (SRAM) access ──── */

void Liot_Ch390ReadMem(Liot_Ch390Handle_t handle, uint8_t *data, int length) {
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);
    ctx_scs(ctx, 0);
    ctx_spi_exchange(ctx, OPC_MEM_READ);
    ctx->spiDrv->Receive(data, length);
    ctx_wait_spi(ctx);
    ctx_scs(ctx, 1);
}

void Liot_Ch390WriteMem(Liot_Ch390Handle_t handle, uint8_t *data, int length) {
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);
    ctx_scs(ctx, 0);
    ctx_spi_exchange(ctx, OPC_MEM_WRITE);
    ctx->spiDrv->Send(data, length);
    ctx_wait_spi(ctx);
    ctx_scs(ctx, 1);
}

/* ──── PHY / EEPROM access ──── */

uint16_t Liot_Ch390ReadPhy(Liot_Ch390Handle_t handle, uint8_t reg) {
    Liot_Ch390WriteReg(handle, CH390_EPAR, CH390_PHY | reg);
    Liot_Ch390WriteReg(handle, CH390_EPCR, EPCR_ERPRR | EPCR_EPOS);
    delay_us(100);
    Liot_Ch390WriteReg(handle, CH390_EPCR, 0x00);
    return (Liot_Ch390ReadReg(handle, CH390_EPDRH) << 8) |
           (Liot_Ch390ReadReg(handle, CH390_EPDRL) & 0xFF);
}

void Liot_Ch390WritePhy(Liot_Ch390Handle_t handle, uint8_t reg, uint16_t val) {
    Liot_Ch390WriteReg(handle, CH390_EPAR, CH390_PHY | reg);
    Liot_Ch390WriteReg(handle, CH390_EPDRL, val & 0xFF);
    Liot_Ch390WriteReg(handle, CH390_EPDRH, (val >> 8) & 0xFF);
    Liot_Ch390WriteReg(handle, CH390_EPCR, EPCR_ERPRW | EPCR_EPOS);
    delay_us(100);
    Liot_Ch390WriteReg(handle, CH390_EPCR, 0x00);
}
void Liot_Ch390WriteEeprom(Liot_Ch390Handle_t handle, uint8_t reg, uint16_t val) {
    Liot_Ch390WriteReg(handle, CH390_EPAR, reg);
    Liot_Ch390WriteReg(handle, CH390_EPDRL, val & 0xFF);
    Liot_Ch390WriteReg(handle, CH390_EPDRH, (val >> 8) & 0xFF);
    Liot_Ch390WriteReg(handle, CH390_EPCR, EPCR_ERPRW);
    while (Liot_Ch390ReadReg(handle, CH390_EPCR) & EPCR_ERRE) {}
    Liot_Ch390WriteReg(handle, CH390_EPCR, 0x00);
}

/* ──── Configuration ──── */

void Liot_Ch390SetPhyMode(Liot_Ch390Handle_t handle, Liot_Ch390PhyMode_e mode) {
    uint16_t bmcr = 0, anar = 0;
    switch (mode) {
    case LIOT_CH390_10MHD:  bmcr = 0x1000; anar = 0x21;  break;
    case LIOT_CH390_10MFD:  bmcr = 0x1100; anar = 0x41;  break;
    case LIOT_CH390_100MHD: bmcr = 0x3000; anar = 0x81;  break;
    case LIOT_CH390_100MFD: bmcr = 0x3100; anar = 0x101; break;
    case LIOT_CH390_AUTO:   bmcr = 0x1000; anar = 0x01E1; break;
    }
    Liot_Ch390WritePhy(handle, CH390_PHY_BMCR, bmcr);
    Liot_Ch390WritePhy(handle, CH390_PHY_ANAR, anar);
    Liot_Ch390WriteReg(handle, CH390_GPR, 0x00);
}

void Liot_Ch390DefaultConfig(Liot_Ch390Handle_t handle) {
    if (!handle) return;
    liot_ch390_ctx_t *ctx = handle_to_ctx(handle);

    Liot_Ch390SetPhyMode(handle, LIOT_CH390_AUTO);

    Liot_Ch390WriteReg(handle, CH390_NSR, NSR_WAKEST | NSR_TX2END | NSR_TX1END);
    Liot_Ch390WriteReg(handle, CH390_ISR, 0xFF);
    Liot_Ch390WriteReg(handle, CH390_TCR2, 0x80);
    Liot_Ch390WriteReg(handle, CH390_TCSCR, TCSCR_ALL);
    Liot_Ch390WriteReg(handle, CH390_INTCR, INCR_TYPE_PP | INCR_POL_L);

    uint8_t multicast[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    Liot_Ch390SetMulticast(handle, multicast);

    uint8_t allZero = 1;
    for (int i = 0; i < 6; i++) {
        if (ctx->mac[i] != 0) { allZero = 0; break; }
    }
    if (allZero) {
        Liot_Ch390GetMac(handle, ctx->mac);
    } else {
        Liot_Ch390SetMac(handle, ctx->mac);
    }

    Liot_Ch390WriteReg(handle, CH390_IMR, IMR_ALL);
    Liot_Ch390WriteReg(handle, CH390_RCR, RCR_DIS_CRC | RCR_RXEN);

    liot_trace("CH390[%d] config done, mac=%x:%x:%x:%x:%x:%x", ctx->spiPort,
                  ctx->mac[0], ctx->mac[1], ctx->mac[2],
                  ctx->mac[3], ctx->mac[4], ctx->mac[5]);
}
void Liot_Ch390SetMac(Liot_Ch390Handle_t handle, const uint8_t *mac) {
    for (int i = 0; i < 6; i++)
        Liot_Ch390WriteReg(handle, CH390_PAR + i, mac[i]);
}

void Liot_Ch390SetMulticast(Liot_Ch390Handle_t handle, const uint8_t *hash) {
    for (int i = 0; i < 8; i++)
        Liot_Ch390WriteReg(handle, CH390_MAR + i, hash[i]);
}

static uint8_t reflect_8(uint8_t val) {
    uint8_t res = 0;
    for (int i = 0; i < 8; i++)
        if (val & (1 << i)) res |= 1 << (7 - i);
    return res;
}

void Liot_Ch390SetHashBit(Liot_Ch390Handle_t handle, const uint8_t *mac) {
    const uint32_t poly = 0x4C11DB7;
    uint32_t crc = 0xFFFFFFFF;
    for (int b = 0; b < 6; b++) {
        uint8_t cur = reflect_8(mac[b]);
        crc ^= (uint32_t)cur << 24;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80000000) ? (crc << 1) ^ poly : (crc << 1);
    }
    uint8_t bit = (crc ^ 0xFFFFFFFF) >> 26;
    uint8_t mar = CH390_MAR + bit / 8;
    uint8_t val = Liot_Ch390ReadReg(handle, mar);
    val |= 1 << (bit % 8);
    Liot_Ch390WriteReg(handle, mar, val);
}

void Liot_Ch390InterruptConfig(Liot_Ch390Handle_t handle, uint8_t mask) {
    Liot_Ch390WriteReg(handle, CH390_IMR, mask);
}

void Liot_Ch390IntPinConfig(Liot_Ch390Handle_t handle, uint8_t type, uint8_t pol) {
    Liot_Ch390WriteReg(handle, CH390_INTCR, type | pol);
}

void Liot_Ch390RxEnable(Liot_Ch390Handle_t handle, int op) {
    uint8_t rcr = Liot_Ch390ReadReg(handle, CH390_RCR);
    if (op)
        rcr |= RCR_RXEN;
    else
        rcr &= ~RCR_RXEN;
    Liot_Ch390WriteReg(handle, CH390_RCR, rcr);
}

void Liot_Ch390RxFilterConfig(Liot_Ch390Handle_t handle, uint8_t config) {
    uint8_t rcr = Liot_Ch390ReadReg(handle, CH390_RCR) & RCR_RXEN;
    Liot_Ch390WriteReg(handle, CH390_RCR, rcr | config);
}
void Liot_Ch390WakeupConfig(Liot_Ch390Handle_t handle, uint8_t events) {
    uint8_t ncr = Liot_Ch390ReadReg(handle, CH390_NCR);
    if (events)
        ncr |= NCR_WAKEEN;
    else
        ncr &= ~NCR_WAKEEN;
    Liot_Ch390WriteReg(handle, CH390_NCR, ncr);
    Liot_Ch390WriteReg(handle, CH390_WCR, events);
}

void Liot_Ch390WakeNotify(Liot_Ch390Handle_t handle) {
    uint8_t ncr = Liot_Ch390ReadReg(handle, CH390_NCR);
    Liot_Ch390WriteReg(handle, CH390_NCR, ncr ^ 0x10);
}

void Liot_Ch390LoopBackEnable(Liot_Ch390Handle_t handle, int op) {
    uint8_t ncr = Liot_Ch390ReadReg(handle, CH390_NCR) & ~0x06;
    if (op) ncr |= NCR_LBK_MAC;
    Liot_Ch390WriteReg(handle, CH390_NCR, ncr);
}

void Liot_Ch390SleepControl(Liot_Ch390Handle_t handle, int op) {
    if (op) {
        Liot_Ch390WriteReg(handle, CH390_SCCR, 0x01);
    } else {
        Liot_Ch390ReadReg(handle, CH390_RSCCR);
        delay_us(100);
    }
}

/* ──── GPIO (CH390 internal) ──── */

void Liot_Ch390GpioConfig(Liot_Ch390Handle_t handle, uint8_t GPIOx, uint8_t dir) {
    uint8_t gpcr = Liot_Ch390ReadReg(handle, CH390_GPCR);
    if (dir)
        gpcr |= GPIOx;
    else
        gpcr &= ~GPIOx;
    Liot_Ch390WriteReg(handle, CH390_GPCR, gpcr);
}

void Liot_Ch390GpioWriteBit(Liot_Ch390Handle_t handle, uint8_t GPIOx, uint8_t level) {
    uint8_t gpr = Liot_Ch390ReadReg(handle, CH390_GPR);
    if (level)
        gpr |= GPIOx;
    else
        gpr &= ~GPIOx;
    Liot_Ch390WriteReg(handle, CH390_GPR, gpr);
}

uint8_t Liot_Ch390GpioReadBit(Liot_Ch390Handle_t handle, uint8_t GPIOx) {
    return !!(Liot_Ch390ReadReg(handle, CH390_GPR) & GPIOx);
}
/* ──── Status queries ──── */

void Liot_Ch390GetMac(Liot_Ch390Handle_t handle, uint8_t *mac) {
    for (int i = 0; i < 6; i++)
        mac[i] = Liot_Ch390ReadReg(handle, CH390_PAR + i);
}

void Liot_Ch390GetMulticast(Liot_Ch390Handle_t handle, uint8_t *hash) {
    for (int i = 0; i < 8; i++)
        hash[i] = Liot_Ch390ReadReg(handle, CH390_MAR + i);
}

uint16_t Liot_Ch390GetVendorId(Liot_Ch390Handle_t handle) {
    return (Liot_Ch390ReadReg(handle, CH390_VIDL) & 0xFF) |
           (Liot_Ch390ReadReg(handle, CH390_VIDH) << 8);
}

uint16_t Liot_Ch390GetProductId(Liot_Ch390Handle_t handle) {
    return (Liot_Ch390ReadReg(handle, CH390_PIDL) & 0xFF) |
           (Liot_Ch390ReadReg(handle, CH390_PIDH) << 8);
}

uint8_t Liot_Ch390GetRevision(Liot_Ch390Handle_t handle) {
    return Liot_Ch390ReadReg(handle, CH390_CHIPR);
}

uint8_t Liot_Ch390GetIntStatus(Liot_Ch390Handle_t handle) {
    uint8_t isr = Liot_Ch390ReadReg(handle, CH390_ISR);
    Liot_Ch390WriteReg(handle, CH390_ISR, isr);
    return isr;
}

int Liot_Ch390GetLinkStatus(Liot_Ch390Handle_t handle) {
    return !!(Liot_Ch390ReadReg(handle, CH390_NSR) & NSR_LINKST);
}

int Liot_Ch390GetDuplexMode(Liot_Ch390Handle_t handle) {
    return !!(Liot_Ch390ReadReg(handle, CH390_NCR) & NCR_FDX);
}

int Liot_Ch390GetPhySpeed(Liot_Ch390Handle_t handle) {
    return !!(Liot_Ch390ReadReg(handle, CH390_NSR) & NSR_SPEED);
}
/* ──── Packet TX/RX ──── */

uint32_t Liot_Ch390Receive(Liot_Ch390Handle_t handle, uint8_t *buf) {
    Liot_Ch390ReadReg(handle, CH390_MRCMDX);
    uint8_t rx_ready = Liot_Ch390ReadReg(handle, CH390_MRCMDX);

    if (rx_ready & CH390_PKT_ERR) {
        Liot_Ch390WriteReg(handle, CH390_RCR, 0);
        Liot_Ch390WriteReg(handle, CH390_MPTRCR, 0x01);
        Liot_Ch390WriteReg(handle, CH390_MRRH, 0x0C);
        delay_us(1000);
        Liot_Ch390WriteReg(handle, CH390_RCR, RCR_DIS_CRC | RCR_RXEN);
        return 0;
    }
    if (!(rx_ready & CH390_PKT_RDY))
        return 0;

    uint8_t header[4];
    Liot_Ch390ReadMem(handle, header, 4);

    uint16_t rxLen = header[2] | (header[3] << 8);
    if (rxLen <= CH390_PKT_MAX)
        Liot_Ch390ReadMem(handle, buf, rxLen);

    if ((header[1] & 0x3F) || (rxLen > CH390_PKT_MAX))
        return 0;

    return (uint32_t)rxLen;
}

void Liot_Ch390Send(Liot_Ch390Handle_t handle, uint8_t *buf, uint16_t len) {
    Liot_Ch390WriteMem(handle, buf, len);
    while (Liot_Ch390ReadReg(handle, CH390_TCR) & TCR_TXREQ) {}
    Liot_Ch390WriteReg(handle, CH390_TXPLL, len & 0xFF);
    Liot_Ch390WriteReg(handle, CH390_TXPLH, (len >> 8) & 0xFF);
    Liot_Ch390SendRequest(handle);
}

void Liot_Ch390SendRequest(Liot_Ch390Handle_t handle) {
    uint8_t tcr = Liot_Ch390ReadReg(handle, CH390_TCR);
    Liot_Ch390WriteReg(handle, CH390_TCR, tcr | TCR_TXREQ);
}

void Liot_Ch390DropPacket(Liot_Ch390Handle_t handle, uint16_t len) {
    uint16_t mdr = Liot_Ch390ReadReg(handle, CH390_MRRL) |
                   (Liot_Ch390ReadReg(handle, CH390_MRRH) << 8);
    mdr += len;
    mdr = mdr < 0x4000 ? mdr : mdr - 0x3400;
    Liot_Ch390WriteReg(handle, CH390_MRRL, mdr & 0xFF);
    Liot_Ch390WriteReg(handle, CH390_MRRH, (mdr >> 8) & 0xFF);
}
