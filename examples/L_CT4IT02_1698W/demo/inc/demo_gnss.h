#ifndef DEMO_GNSS_H
#define DEMO_GNSS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool demo_gnss_get_location(int32_t *latitude_e7, int32_t *longitude_e7);
bool demo_gnss_is_ready(void);
int demo_gnss_inject_assistance(const uint8_t *data, size_t size);
int demo_gnss_query_assistance(void);
void liot_gnss_demo_thread(void *argv);

#endif
