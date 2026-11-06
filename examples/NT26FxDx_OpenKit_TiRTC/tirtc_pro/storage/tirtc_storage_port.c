#include "tirtc_storage_port.h"
#include "tirtc_storage.h"
#include "liot_spi.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_log.h"
#include "ec7xx.h"
#include <string.h>
static liot_mutex_t s_mutex;

int tirtc_storage_port_init(void)
{
    liot_spi_config_s cfg = {0};
    if (!s_mutex && liot_rtos_mutex_create(&s_mutex) != 0) return TIRTC_STORAGE_E_IO;
    if (Liot_SetPinFunc(63, L_PIN_FUNC_1) != L_GPIO_ERR_SUCCESS ||
        Liot_SetPinFunc(62, L_PIN_FUNC_1) != L_GPIO_ERR_SUCCESS ||
        Liot_SetPinFunc(49, L_PIN_FUNC_1) != L_GPIO_ERR_SUCCESS)
        return TIRTC_STORAGE_E_IO;
    /* Shared 3.3 V already enabled by display port; never switch it here. */
    cfg.input_mode = LIOT_SPI_INPUT_TRUE;
    cfg.port = LIOT_SPI_PORT1;
    cfg.framesize = 8;
    cfg.spiclk = LIOT_SPI_CLK_6_5MHZ;
    cfg.cs_polarity0 = cfg.cs_polarity1 = LIOT_SPI_CS_ACTIVE_LOW;
    cfg.cpol = LIOT_SPI_CPOL_LOW;
    cfg.cpha = LIOT_SPI_CPHA_1Edge;
    cfg.input_sel = LIOT_SPI_DI_1;
    cfg.transmode = LIOT_SPI_DIRECT_POLLING;
    cfg.cs = LIOT_SPI_CS_NULL;
    cfg.device_mode = LIOT_SPI_DEVICE_MODE_MASTER;
    if (liot_spi_init_ext(cfg) != LIOT_SPI_SUCCESS) return TIRTC_STORAGE_E_IO;
    if (Liot_SetPinFunc(64, L_PIN_FUNC_0) != L_GPIO_ERR_SUCCESS ||
        Liot_GpioInit(L_GPIO_12, L_IO_OUTPUT, L_IO_HIGH, NULL) != L_GPIO_ERR_SUCCESS)
        return TIRTC_STORAGE_E_IO;
    return 0;
}
uint32_t tirtc_storage_port_ms(void) { return liot_rtos_get_running_time(); }
void tirtc_storage_port_delay(uint32_t ms) { liot_rtos_task_sleep_ms(ms); }
int tirtc_storage_port_lock(uint32_t ms)
{ return s_mutex && liot_rtos_mutex_lock(s_mutex, ms) == 0 ? 0 : TIRTC_STORAGE_E_BUSY; }
void tirtc_storage_port_unlock(void) { (void)liot_rtos_mutex_unlock(s_mutex); }
/* Snapshot publishers/readers are tasks. F6D_A's _from_isr wrapper is a no-op
 * when IPSR == 0 and PRIMASK == 0, so it cannot protect a task-side copy.
 * The normal FreeRTOS pair tracks nesting; the token is kept for port ABI. */
uint32_t tirtc_storage_port_enter(void) { liot_rtos_enter_critical(); return 0; }
void tirtc_storage_port_leave(uint32_t mask) { (void)mask; liot_rtos_exit_critical(); }
int tirtc_storage_spi_cs(int high)
{ return Liot_GpioSetLevel(L_GPIO_12, high ? L_IO_HIGH : L_IO_LOW) == L_GPIO_ERR_SUCCESS ? 0 : -1; }
static volatile uint32_t *reg_address(unsigned reg)
{
    switch (reg) {
    case TSPI_CR1: return &SPI1->CR1;
    case TSPI_DR: return &SPI1->DR;
    case TSPI_SR: return &SPI1->SR;
    case TSPI_IMSC: return &SPI1->IMSC;
    case TSPI_DMACR: return &SPI1->DMACR;
    case TSPI_CER: return &SPI1->CER;
    case TSPI_WBSR: return &SPI1->WBSR;
    case TSPI_RBSR: return (volatile uint32_t *)&SPI1->RBSR;
    case TSPI_RBLR: return &SPI1->RBLR;
    case TSPI_RCCR: return &SPI1->RCCR;
    case TSPI_FFR: return &SPI1->FFR;
    default: return &SPI1->ROMR;
    }
}
uint32_t tirtc_storage_spi_reg_read(unsigned reg) { return *reg_address(reg); }
void tirtc_storage_spi_reg_write(unsigned reg, uint32_t value) { *reg_address(reg) = value; }
int tirtc_storage_port_transfer(const uint8_t *tx, uint8_t *rx, uint32_t size)
{ return tirtc_storage_spi_fifo(tx, rx, size); }

int tirtc_storage_port_recover(void)
{
    int ret;
    /* SPI1 is owned exclusively by storage. Preserve configured clock/pins;
     * release the NOR transaction before clearing receive/transmit state. */
    tirtc_storage_spi_reg_write(TSPI_CR1, tirtc_storage_spi_reg_read(TSPI_CR1) & ~2U);
    ret = tirtc_storage_spi_cs(1);
    tirtc_storage_spi_reg_write(TSPI_IMSC, 0);
    tirtc_storage_spi_reg_write(TSPI_DMACR, 0);
    tirtc_storage_spi_reg_write(TSPI_FFR, 3);
    tirtc_storage_spi_reg_write(TSPI_RCCR, 1);
    tirtc_storage_spi_reg_write(TSPI_CER, 3);
    tirtc_storage_spi_reg_write(TSPI_ROMR, 0);
    tirtc_storage_spi_reg_write(TSPI_RBLR, 0);
    return ret ? TIRTC_STORAGE_E_IO : 0;
}

void tirtc_storage_port_diagnostic(const char *phase, int result,
                                   uint32_t elapsed_ms, uint32_t command)
{
    tirtc_storage_spi_diagnostic_t diag = {0};
    /* A whole-operation budget has no failing SPI transaction. Never attach
     * the previous successful transaction's detail to a budget failure. */
    if (phase != NULL && strcmp(phase, "transfer") == 0)
        tirtc_storage_spi_get_diagnostic(&diag);
    liot_trace("[flash12] phase=%s err=%d cmd=%02lx op_ms=%lu spi_stage=%lu bytes=%lu tx=%lu rx=%lu sr=%08lx spi_ms=%lu wait_ms=%lu\r\n",
               phase != NULL ? phase : "unknown", result,
               (unsigned long)command, (unsigned long)elapsed_ms,
               (unsigned long)diag.stage, (unsigned long)diag.size,
               (unsigned long)diag.sent, (unsigned long)diag.received,
               (unsigned long)diag.status, (unsigned long)diag.elapsed_ms,
               (unsigned long)diag.wait_ms);
}
