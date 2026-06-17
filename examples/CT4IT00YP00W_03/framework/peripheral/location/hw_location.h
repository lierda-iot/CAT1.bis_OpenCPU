#ifndef LOCATION_MODULE_H
#define LOCATION_MODULE_H

#include <stdint.h>

#define LOCATION_LONGITUDE_LEN  32
#define LOCATION_LATITUDE_LEN   32
#define LOCATION_DESC_LEN       256

#define LOCATION_FALLBACK_ENABLE        1
#define LOCATION_FALLBACK_LONGITUDE     "116.397128"
#define LOCATION_FALLBACK_LATITUDE      "39.916527"
#define LOCATION_FALLBACK_DESC          "北京市东城区故宫博物院"

typedef struct {
    char longitude[LOCATION_LONGITUDE_LEN];
    char latitude[LOCATION_LATITUDE_LEN];
    char desc[LOCATION_DESC_LEN];
} location_info_t;

int locationModuleGetPosition(location_info_t *info);

#endif /* LOCATION_MODULE_H */
