/* Compatibility boundary for the official Lierda F6D_D MQTT implementation. */
#ifndef TIRTC_MQTT_COMPAT_H
#define TIRTC_MQTT_COMPAT_H

#include <stdbool.h>
#include "liot_mqtt_client.h"

/* Checks completion only: no wait, deinit submission, handle reset, or free.
 * NULL/zero handles own no vendor context and are complete. A nonzero handle
 * must be a valid SDK static slot in its fully reset state. Call only from a
 * normal task with actual (including inherited) priority below 22, with all
 * app MQTT initialization serialized and old callbacks already drained.
 * Call after BOTH deinit=0 and deinit=-2; neither return alone proves release.
 * False means retain handle/options/semaphore and check again later. Do not
 * resubmit deinit while an accepted request is pending.
 */
bool tirtc_mqtt_cleanup_complete(const liot_mqtt_client_t *client);

#endif
