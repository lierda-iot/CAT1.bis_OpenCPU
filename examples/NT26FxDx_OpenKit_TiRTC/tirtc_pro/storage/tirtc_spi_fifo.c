/* EC718 SPI B2 layout and FIFO sequence: SDK ec718.h and the shipped
 * F6D_A SPI_Transfer at 0x008d67c8. Unlike that polling implementation,
 * every wait below is bounded and always disables SPI and releases CS. */
#include "tirtc_storage_port.h"
#include "tirtc_storage.h"
#include <string.h>
static tirtc_storage_spi_diagnostic_t s_diagnostic;
void tirtc_storage_spi_get_diagnostic(tirtc_storage_spi_diagnostic_t *out)
{ if (out != NULL) *out = s_diagnostic; }

int tirtc_storage_spi_fifo(const uint8_t *tx, uint8_t *rx, uint32_t size)
{
    uint32_t start, operation_start, now, spins = 0, sent = 0, received = 0;
    uint32_t cr1, last_status = 0;
    int ret = 0, deadline_observed = 0;
    memset(&s_diagnostic, 0, sizeof(s_diagnostic));
    s_diagnostic.size = size;
    if (!tx || !rx || !size || size > 260U) {
        s_diagnostic.stage = TSPI_DIAG_ARGUMENT;
        return TIRTC_STORAGE_E_ARGUMENT;
    }
    operation_start = start = tirtc_storage_port_ms();
    cr1 = tirtc_storage_spi_reg_read(TSPI_CR1) & ~2U;
    tirtc_storage_spi_reg_write(TSPI_CR1, cr1);
    tirtc_storage_spi_reg_write(TSPI_IMSC, 0);
    tirtc_storage_spi_reg_write(TSPI_DMACR, 0);
    tirtc_storage_spi_reg_write(TSPI_FFR, 3);
    tirtc_storage_spi_reg_write(TSPI_RCCR, 1);
    tirtc_storage_spi_reg_write(TSPI_CER, 3);
    tirtc_storage_spi_reg_write(TSPI_ROMR, 0);
    tirtc_storage_spi_reg_write(TSPI_RBLR, size);
    if (tirtc_storage_spi_cs(0)) {
        s_diagnostic.stage = TSPI_DIAG_CS_ASSERT;
        ret = TIRTC_STORAGE_E_IO; goto finish;
    }
    start = tirtc_storage_port_ms();
    tirtc_storage_spi_reg_write(TSPI_CR1, cr1 | 2U);
    while (received < size) {
        int progress = 0;
        uint32_t status = tirtc_storage_spi_reg_read(TSPI_SR);
        last_status = status;
        if (sent < size && ((status >> 16) & 255U) <= 60U && sent - received <= 60U) {
            uint32_t word = 0, count = size - sent;
            if (count > 4U) count = 4U;
            memcpy(&word, tx + sent, count);
            tirtc_storage_spi_reg_write(TSPI_WBSR, (1U << count) - 1U);
            tirtc_storage_spi_reg_write(TSPI_DR, word);
            sent += count;
            progress = 1;
        }
        last_status = tirtc_storage_spi_reg_read(TSPI_SR);
        if (last_status & 4U) {
            uint32_t word = tirtc_storage_spi_reg_read(TSPI_DR);
            uint32_t strobe = tirtc_storage_spi_reg_read(TSPI_RBSR) & 15U;
            uint32_t count = 0;
            while (strobe) { count++; strobe >>= 1; }
            if (!count || count > size - received || received + count > sent ||
                (tirtc_storage_spi_reg_read(TSPI_RBSR) & 15U) != (1U << count) - 1U) {
                s_diagnostic.stage = TSPI_DIAG_STROBE;
                ret = TIRTC_STORAGE_E_IO; break;
            }
            memcpy(rx + received, &word, count);
            received += count;
            progress = 1;
        }
        /* Higher-priority UI/network tasks can preempt this worker. Time out
         * stalled hardware, not a completed transfer that was preempted. */
        if (progress) { start = tirtc_storage_port_ms(); deadline_observed = 0; }
        if (received == size) break;
        now = tirtc_storage_port_ms();
        if ((uint32_t)(now - start) >= 20U || ++spins >= 1000000U) {
            /* A task switch between the last status observation and now can
             * consume the whole deadline while hardware finishes. Revisit
             * the FIFO once with fresh status before deciding it is stalled.
             * No progress on that revisit fails without renewing the timer. */
            if (deadline_observed && !progress) {
                s_diagnostic.stage = TSPI_DIAG_RX_WAIT;
                ret = TIRTC_STORAGE_E_TIMEOUT; break;
            }
            deadline_observed = 1;
        }
    }
    while (!ret && ((last_status = tirtc_storage_spi_reg_read(TSPI_SR)) & 16U)) {
        if ((uint32_t)(tirtc_storage_port_ms() - start) >= 20U || ++spins >= 1000000U) {
            /* BSY may have cleared while a higher-priority task ran. */
            last_status = tirtc_storage_spi_reg_read(TSPI_SR);
            if (!(last_status & 16U)) break;
            s_diagnostic.stage = TSPI_DIAG_BUSY_WAIT;
            ret = TIRTC_STORAGE_E_TIMEOUT;
        }
    }
finish:
    now = tirtc_storage_port_ms();
    s_diagnostic.sent = sent;
    s_diagnostic.received = received;
    s_diagnostic.status = last_status;
    s_diagnostic.elapsed_ms = now - operation_start;
    s_diagnostic.wait_ms = now - start;
    tirtc_storage_spi_reg_write(TSPI_CR1, cr1);
    if (tirtc_storage_spi_cs(1) && !ret) {
        s_diagnostic.stage = TSPI_DIAG_CS_RELEASE;
        ret = TIRTC_STORAGE_E_IO;
    }
    tirtc_storage_spi_reg_write(TSPI_FFR, 3);
    tirtc_storage_spi_reg_write(TSPI_RCCR, 1);
    return ret;
}
