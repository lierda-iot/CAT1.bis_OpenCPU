#ifndef DEMO_XIAOZHI_CORE_H
#define DEMO_XIAOZHI_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Pure URL parsing and FNV-1a hashing extracted from demo_xiaozhi.
 * Header-only on purpose so the same source is host-testable without liot
 * RTOS / websocket dependencies. Production behavior is unchanged.
 */

/*
 * Parse "wss://host[:port]/path" into host, path and port.
 * Returns 0 on success; -1 for non-wss/malformed url; -2 when host does not
 * fit host_size. Mirrors xz_ws_parse_url (xiaozhi_ws.c).
 */
static inline int demo_xiaozhi_core_parse_ws_url(const char *url,
                                                 char *host,
                                                 int host_size,
                                                 char *path,
                                                 int path_size,
                                                 int *port)
{
    const char *start;
    const char *slash;
    const char *colon;
    int host_len;

    if ((url == NULL) || (strncmp(url, "wss://", 6) != 0)) {
        return -1;
    }

    start = url + 6;
    slash = strchr(start, '/');
    if (slash == NULL) {
        slash = start + (int)strlen(start);
    }
    colon = strchr(start, ':');
    if ((colon != NULL) && (colon < slash)) {
        host_len = (int)(colon - start);
        *port = atoi(colon + 1);
    } else {
        host_len = (int)(slash - start);
        *port = 443;
    }

    if ((host_len <= 0) || (host_len >= host_size)) {
        return -2;
    }
    if ((host == NULL) || (path == NULL) || (port == NULL) ||
        (path_size <= 0)) {
        return -1;
    }

    (void)memcpy(host, start, (size_t)host_len);
    host[host_len] = '\0';
    (void)snprintf(path, (size_t)path_size, "%s", (*slash != '\0') ? slash : "/");
    return 0;
}

/*
 * FNV-1a 32-bit hash. Mirrors xz_hash (xiaozhi_identity.c).
 */
static inline uint32_t demo_xiaozhi_core_fnv1a(const char *text, uint32_t seed)
{
    uint32_t hash = seed;

    if (text == NULL) {
        return hash;
    }
    while (*text != '\0') {
        hash ^= (uint8_t)*text++;
        hash *= 16777619u;
    }
    return hash;
}

#endif /* DEMO_XIAOZHI_CORE_H */
