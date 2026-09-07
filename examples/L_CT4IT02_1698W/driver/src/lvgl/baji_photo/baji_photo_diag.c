#include "baji_photo_diag.h"

#include <string.h>

#include "liot_os.h"

static volatile uint32_t g_baji_photo_diag_next = 0u;
static const char k_baji_photo_diag_empty_id[] = "-";
static const char k_baji_photo_diag_zero[] = "0";

uint32_t baji_photo_diag_next_id(void)
{
    uint32_t id;

    liot_rtos_enter_critical();
    g_baji_photo_diag_next += 1u;
    if (g_baji_photo_diag_next == 0u) {
        g_baji_photo_diag_next = 1u;
    }
    id = g_baji_photo_diag_next;
    liot_rtos_exit_critical();
    return id;
}

const char *baji_photo_diag_id_tail(const char *id)
{
    size_t len;

    if ((id == NULL) || (id[0] == '\0')) {
        return k_baji_photo_diag_empty_id;
    }

    len = strlen(id);
    if (len <= 8u) {
        return id;
    }

    return id + (len - 8u);
}

const char *baji_photo_diag_u64_dec(uint64_t value, char *buf, unsigned int len)
{
    char tmp[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    unsigned int digits = 0u;
    unsigned int i;

    if ((buf == NULL) || (len < 2u)) {
        return k_baji_photo_diag_zero;
    }

    do {
        tmp[digits++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while ((value != 0u) && (digits < (unsigned int)sizeof(tmp)));

    if ((digits + 1u) > len) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    for (i = 0u; i < digits; ++i) {
        buf[i] = tmp[digits - 1u - i];
    }
    buf[digits] = '\0';
    return buf;
}
