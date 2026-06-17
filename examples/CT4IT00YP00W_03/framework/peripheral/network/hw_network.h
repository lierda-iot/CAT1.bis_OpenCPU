#ifndef NETWORK_PRIVATE_H
#define NETWORK_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char apn[64];
    char username[32];
    char password[32];
} network_config_t;

bool networkModuleInit(const network_config_t *cfg);
void networkDisconnect(void);

#endif /* NETWORK_PRIVATE_H */
