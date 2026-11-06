#ifndef TIRTC_STORAGE_PORT_H
#define TIRTC_STORAGE_PORT_H
#include <stdint.h>
#include <stddef.h>
int tirtc_storage_port_init(void);
/* Storage mutex held, no active transfer. Clears the SPI transport only;
 * never changes the shared rail or sends commands to erase/reset the chip. */
int tirtc_storage_port_recover(void);
void tirtc_storage_port_diagnostic(const char *phase, int result,
                                   uint32_t elapsed_ms, uint32_t command);
int tirtc_storage_port_transfer(const uint8_t *tx, uint8_t *rx, uint32_t size);
uint32_t tirtc_storage_port_ms(void);
void tirtc_storage_port_delay(uint32_t ms);
int tirtc_storage_port_lock(uint32_t timeout_ms);
void tirtc_storage_port_unlock(void);
/* Task context only; paired, nestable FreeRTOS critical section. No I/O here. */
uint32_t tirtc_storage_port_enter(void);
void tirtc_storage_port_leave(uint32_t mask);
/* FIFO transport is separate so host tests exercise real timeout/cleanup logic. */
enum { TSPI_CR1, TSPI_DR, TSPI_SR, TSPI_IMSC, TSPI_DMACR, TSPI_CER,
       TSPI_WBSR, TSPI_RBSR, TSPI_RBLR, TSPI_RCCR, TSPI_FFR, TSPI_ROMR };
uint32_t tirtc_storage_spi_reg_read(unsigned reg);
void tirtc_storage_spi_reg_write(unsigned reg, uint32_t value);
int tirtc_storage_spi_cs(int high);
int tirtc_storage_spi_fifo(const uint8_t *tx, uint8_t *rx, uint32_t size);
enum {
    TSPI_DIAG_NONE, TSPI_DIAG_ARGUMENT, TSPI_DIAG_CS_ASSERT,
    TSPI_DIAG_RX_WAIT, TSPI_DIAG_BUSY_WAIT, TSPI_DIAG_STROBE,
    TSPI_DIAG_CS_RELEASE
};
typedef struct {
    uint32_t stage, size, sent, received, status, elapsed_ms, wait_ms;
} tirtc_storage_spi_diagnostic_t;
/* Copy under the storage mutex; describes the last transfer, no register I/O. */
void tirtc_storage_spi_get_diagnostic(tirtc_storage_spi_diagnostic_t *out);
#endif
