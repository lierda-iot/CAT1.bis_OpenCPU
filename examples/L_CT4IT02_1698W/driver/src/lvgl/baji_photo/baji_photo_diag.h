#ifndef BAJI_PHOTO_DIAG_H
#define BAJI_PHOTO_DIAG_H

#include <stdint.h>

#define BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN 24u

uint32_t baji_photo_diag_next_id(void);
const char *baji_photo_diag_id_tail(const char *id);
const char *baji_photo_diag_u64_dec(uint64_t value, char *buf, unsigned int len);

#endif
