/* Linker-derived APP allocation, never the size of the bundled modem package. */
#include "tirtc_resources.h"
#include "mem_map.h"
#include <stdint.h>

extern const uint8_t __tirtc_app_flash_used[];
extern const uint8_t __tirtc_app_flash_capacity[];
extern const uint8_t __tirtc_app_static_ram_used[];
extern const uint8_t __tirtc_app_ram_capacity[];

void tirtc_resources_read_layout(tirtc_ui_resources_t *out)
{
    if (!out) return;
    /* These are absolute linker symbol values, not memory to dereference. */
    out->flash_used_bytes = (uint32_t)(uintptr_t)__tirtc_app_flash_used;
    out->flash_total_bytes = (uint32_t)(uintptr_t)__tirtc_app_flash_capacity;
    out->static_ram_used_bytes = (uint32_t)(uintptr_t)__tirtc_app_static_ram_used;
    out->static_ram_total_bytes = (uint32_t)(uintptr_t)__tirtc_app_ram_capacity;
    out->flash_valid = out->flash_total_bytes != 0 && out->flash_used_bytes <= out->flash_total_bytes;
    out->static_ram_valid = out->static_ram_total_bytes != 0 && out->static_ram_used_bytes <= out->static_ram_total_bytes;
    /* Same configured partition size embedded in the application header. */
    out->filesystem_total_bytes = PKGFLXFS_IMG_SIZE;
}
