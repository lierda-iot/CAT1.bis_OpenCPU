/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Read-only state contract for the single SIM0/CID1 network owner.
 */

#ifndef TIRTC_APP_NETWORK_MANAGER_H
#define TIRTC_APP_NETWORK_MANAGER_H

#include <stdbool.h>
#include "liot_rtc.h"

typedef enum
{
    DEMO_NET_INIT = 0,
    DEMO_NET_REGISTERING,
    DEMO_NET_DATA_CALL,
    DEMO_NET_TIME_SYNC,
    DEMO_NET_READY,
    DEMO_NET_RETRY,
} demo_net_state_e;

void demo_net_time_task(void *argv);
demo_net_state_e demo_net_time_get_state(void);
bool demo_net_time_is_data_ready(void);
bool demo_net_time_get_local(liot_rtc_time_s *time_now);

#endif /* TIRTC_APP_NETWORK_MANAGER_H */
