#ifndef TIRTC_NETWORK_TIME_H
#define TIRTC_NETWORK_TIME_H
#include <stdbool.h>
/* Internal network module interface, task context only. */
int tirtc_network_time_start(void);
void tirtc_network_set_time_valid(bool valid);
#endif
