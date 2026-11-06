#ifndef TIRTC_PLATFORM_H
#define TIRTC_PLATFORM_H
#include "../ui/tirtc_ui.h"
#include "device_binding.h"
/* Start one background cloud owner after network/UI/status initialization. */
int tirtc_platform_start(void);
/* UI context: binding/network retries only queue a request; no modem/cloud I/O. */
int tirtc_platform_action(const tirtc_ui_action_t *action);
/* Main task every five seconds: cache copies only, never modem or cloud I/O. */
void tirtc_platform_publish(void);
/* Background-only: identity retrieval can query IMEI/ICCID. Never log output. */
int tirtc_platform_get_identity(demo_binding_tirtc_identity_t *identity);
#endif
