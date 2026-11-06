/* Lierda F6D_D MQTT asynchronous teardown compatibility.
 *
 * This read-only ABI probe is gated by the Makefile against libliot_mqtts.a
 * SHA256 e2cb8a61ddcca04d63fdc671e293b78454727a064c31d453b2bb06d276a33746.
 * Evidence: checkMqttContext accepts six static 880-byte slots; the public
 * state getter validates the slot before reading it. MqttDeleteContext frees
 * members, then mqttSetDefaultConfig resets the slot without freeing it.
 * MqttTaskSendProcess+0xb1c..0xb2a writes the final TX fields and exits.
 * MqttTaskRecvProcess+0x834..0x83e clears its handle, marks stopped and exits.
 *
 * The default template also contains these terminal values. Therefore the
 * priority guard is essential: the SDK TX task is priority 22, while this
 * probe must run below 22. No blocking/log/callback occurs between the start
 * of the final memset/memcpy and TX exit, so a lower-priority normal task
 * cannot observe a half-reset slot. All application MQTT init must remain
 * serialized, otherwise another init could reuse the same static slot.
 * Never query a retired TX task handle: FreeRTOS may already have freed its
 * TCB. uxTaskPriorityGet(NULL) reads only the currently executing task.
 */
#include "tirtc_mqtt_compat.h"
#include "liot_os.h"
#include <stddef.h>
#include <stdint.h>

#if !defined(TIRTC_MQTT_F6D_ABI_VERIFIED) || TIRTC_MQTT_F6D_ABI_VERIFIED != 1
#error "Verify the bundled F6D_D MQTT library hash before enabling this ABI probe"
#endif

/* Exact FreeRTOS task.h ABI: UBaseType_t is unsigned long, TaskHandle_t is
 * a pointer. This private forward declaration avoids importing kernel config
 * into the public SDK-only platform include graph. F6D_D base ELF 0x8120:
 * NULL selects pxCurrentTCB; a nested short critical reads priority at +44.
 */
struct tskTaskControlBlock;
extern unsigned long uxTaskPriorityGet(struct tskTaskControlBlock *task);

enum { MQTT_VENDOR_TX_PRIORITY = 22, MQTT_CONTEXT_BYTES = 880 };
typedef struct {
    uint32_t state;             /* 0: public MQTT state */
    uint32_t session_state;     /* 4: vendor allocated/session state */
    uint8_t reserved[832];
    uint32_t rx_state;          /* 840 */
    uint32_t tx_state;          /* 844 */
    uint32_t tx_lifecycle;      /* 848: 2 running, 1 reset */
    uint32_t rx_task;           /* 852 */
    uint32_t tx_task;           /* 856 */
    uint32_t queue;             /* 860 */
    uint32_t reserved_tail[3];
    uint32_t mutex;             /* 876 */
} mqtt_f6d_context_tail_t;

#define ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
ABI_ASSERT(mqtt_context_size, sizeof(mqtt_f6d_context_tail_t) == MQTT_CONTEXT_BYTES);
ABI_ASSERT(mqtt_context_rx_offset, offsetof(mqtt_f6d_context_tail_t, rx_state) == 840);
ABI_ASSERT(mqtt_context_tx_offset, offsetof(mqtt_f6d_context_tail_t, tx_state) == 844);
ABI_ASSERT(mqtt_context_lifecycle_offset, offsetof(mqtt_f6d_context_tail_t, tx_lifecycle) == 848);
ABI_ASSERT(mqtt_context_rx_handle_offset, offsetof(mqtt_f6d_context_tail_t, rx_task) == 852);
ABI_ASSERT(mqtt_context_tx_handle_offset, offsetof(mqtt_f6d_context_tail_t, tx_task) == 856);
ABI_ASSERT(mqtt_context_queue_offset, offsetof(mqtt_f6d_context_tail_t, queue) == 860);
ABI_ASSERT(mqtt_context_mutex_offset, offsetof(mqtt_f6d_context_tail_t, mutex) == 876);
ABI_ASSERT(mqtt_public_handle_size, sizeof(liot_mqtt_client_t) == sizeof(uint32_t));

bool tirtc_mqtt_cleanup_complete(const liot_mqtt_client_t *client)
{
    liot_mqtt_client_t handle;
    const volatile mqtt_f6d_context_tail_t *context;
    bool complete;

    if (client == NULL || *client == 0) return true;
    handle = *client;
    /* Public state validates exact static-pool membership. Invalid handles
     * may cause an SDK diagnostic; do this outside our critical section.
     * Connection DEFAULT alone is insufficient (an unconnected live task
     * also has DEFAULT). Recheck every field together below.
     */
    if (liot_mqtt_client_state(&handle) != MQTT_CONN_DEFAULT) return false;
    context = (const volatile mqtt_f6d_context_tail_t *)(uintptr_t)(uint32_t)handle;

    liot_rtos_enter_critical();
    complete = uxTaskPriorityGet(NULL) < MQTT_VENDOR_TX_PRIORITY &&
        *client == handle &&
        context->state == 0U && context->session_state == 0U &&
        context->rx_state == 2U && context->tx_state == 2U &&
        context->tx_lifecycle == 1U && context->rx_task == 0U &&
        context->tx_task == 0U && context->queue == 0U &&
        context->mutex == 0U;
    liot_rtos_exit_critical();
    return complete;
}
