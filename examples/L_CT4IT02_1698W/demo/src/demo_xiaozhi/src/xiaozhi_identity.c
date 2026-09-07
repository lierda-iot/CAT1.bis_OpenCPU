#include "xiaozhi_core.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "liot_dev.h"
#include "liot_log.h"

static uint32_t xz_hash(const char *text, uint32_t seed)
{
    uint32_t hash = seed;
    while (*text != '\0') {
        hash ^= (uint8_t)*text++;
        hash *= 16777619u;
    }
    return hash;
}

int xiaozhi_identity_init(xiaozhi_config_t *config)
{
    char imei[20] = {0};
    int ret;
    uint32_t h1;
    uint32_t h2;
    uint32_t h3;
    uint32_t h4;

    if (config == NULL) return -1;
    ret = liot_dev_get_imei(imei, sizeof(imei), 0);
    if (ret != 0 || strlen(imei) < 12) {
        liot_trace("[xiaozhi] IMEI unavailable ret=0x%x len=%u", ret,
                   (unsigned)strlen(imei));
        return -1;
    }

    h1 = xz_hash(imei, 2166136261u);
    h2 = xz_hash(imei, h1 ^ 0x9e3779b9u);
    h3 = xz_hash(imei, h2 ^ 0x85ebca6bu);
    h4 = xz_hash(imei, h3 ^ 0xc2b2ae35u);
    snprintf(config->device_id, sizeof(config->device_id), "02:%02x:%02x:%02x:%02x:%02x",
             (unsigned)((h1 >> 24) & 0xff), (unsigned)((h1 >> 16) & 0xff),
             (unsigned)((h1 >> 8) & 0xff), (unsigned)(h1 & 0xff),
             (unsigned)(h2 & 0xff));
    snprintf(config->client_id, sizeof(config->client_id),
             "%08lx-%04lx-4%03lx-%04lx-%04lx%08lx",
             (unsigned long)h1, (unsigned long)(h2 >> 16),
             (unsigned long)(h2 & 0x0fff),
             (unsigned long)(0x8000u | ((h3 >> 16) & 0x3fffu)),
             (unsigned long)(h3 & 0xffff), (unsigned long)h4);
    liot_trace("[xiaozhi] identity device=%s client=%s", config->device_id, config->client_id);
    return 0;
}
