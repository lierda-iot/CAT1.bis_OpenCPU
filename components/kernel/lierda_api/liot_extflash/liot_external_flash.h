#ifndef LIOT_EXTERNAL_FLASH_H
#define LIOT_EXTERNAL_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  spi_port;
    uint32_t base_addr;
    uint32_t total_size;
} liot_ext_flash_cfg_t;

int32_t  liot_flash_init_ext(const liot_ext_flash_cfg_t *cfg);
int32_t  liot_flash_deinit_ext(void);
uint32_t liot_flash_erase_ext(uint32_t offset, uint32_t size);
uint32_t liot_flash_read_ext(uint8_t *pData, uint32_t offset, uint32_t size);
uint32_t liot_flash_write_ext(uint8_t *pData, uint32_t offset, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
