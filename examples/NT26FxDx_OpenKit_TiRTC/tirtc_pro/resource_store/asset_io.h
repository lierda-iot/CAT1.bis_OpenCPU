#ifndef TIRTC_ASSET_IO_H
#define TIRTC_ASSET_IO_H
#include <stdint.h>

#define TIRTC_ASSET_FONT_PATH "/ui14-font.bin"
#define TIRTC_ASSET_BACKGROUND_PATH "/ui13-bg.bin"
#define TIRTC_ASSET_E_MISMATCH (-74)
#define TIRTC_ASSET_E_NOMEM (-12)
#define TIRTC_ASSET_E_SPACE (-28)
#define TIRTC_ASSET_E_MISSING (-2)

uint32_t tirtc_asset_crc_update(uint32_t crc, const uint8_t *data, uint32_t size);
/* destination NULL verifies without retaining data. All reads <=4096 bytes.
 * Progress is called only after successful chunks, from this worker.
 * Background task context only. Overlapping calls return BUSY without I/O.
 * BUSY can retain one deferred-close token: retry to finish that close before
 * any new open. A read/CRC error is then delivered after retirement, even if
 * the next call supplies another path; no bytes from that next path are read.
 * A successful prior read is repeated for the new call/destination after close.
 * No caller may interpret BUSY as successful verification or an empty fd pool. */
int tirtc_asset_read_verified(const char *path, uint8_t *destination,
                             uint32_t size, uint32_t expected_crc,
                             void (*progress)(uint32_t bytes));
#endif
