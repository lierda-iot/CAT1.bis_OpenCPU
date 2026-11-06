/* Cellular adapter for Lierda V04.
 * Uses the SDK's public liot_sim/nw/datacall APIs and CID1 configuration.
 * Retains CID1/APN credentials; automatic recovery is confined to this worker.
 */
#include "tirtc_network.h"
#include "tirtc_network_time.h"
#include "tirtc_network_signal.h"
#include "../status/tirtc_status.h"
#include <string.h>
#include <stdio.h>
#include "liot_sim.h"
#include "liot_nw.h"
#include "liot_datacall.h"
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"

#define NETWORK_CID 1
/* Bundled FreeRTOSConfig.h sets configTICK_RATE_HZ=1000 (one tick per ms). */
#define NETWORK_POLL_TICKS 2000U
#define NETWORK_TASK_STACK (16U * 1024U)
#define NETWORK_TASK_PRIORITY 10U
#define REQUEST_REFRESH 1U
#define REQUEST_RECONNECT 2U
#define CONNECT_PENDING_TICKS 60000U
#define ROUTE_STALE_MS 15000U
#define SIGNAL_STALE_MS 6000U
#define SIGNAL_LOG_MS 10000U
#define MANUAL_MIN_MS 5000U
#define TIME_SERVICE_RETRY_MS 5000U

/* Same short-lived mutex as the UI copy-in mailboxes. Never held during SDK I/O. */
extern void tirtc_ui_platform_lock(void);
extern void tirtc_ui_platform_unlock(void);
static liot_task_t s_task;
static liot_sem_t s_wake;
static bool s_running;
static unsigned s_requests;
/* Owned only by the background task, not read/written by the action callback. */
static uint8_t s_pending_sim = LIOT_SIM_INVALID;
static bool s_pending;
static uint32_t s_pending_since;
static uint8_t s_route_sim = LIOT_SIM_INVALID;
static bool s_retry_wait;
static unsigned s_retry_step;
static uint32_t s_retry_since, s_retry_delay, s_last_attempt;
static bool s_attempted, s_callback_registered;
/* Time-worker creation belongs to this network owner, not UI/main callers. */
static bool s_time_service_started, s_time_service_attempted;
static bool s_time_service_failure_reported;
static uint32_t s_time_service_attempt_ms;
static tirtc_network_snapshot_t s_cache = {false, -1, 1U, false};
static uint32_t s_cache_sample_ms;
static char s_cache_ip[48];
static tirtc_ui_network_t s_ui_cache;
static bool s_ui_cache_have, s_ui_cache_expired, s_ui_disconnected;
static bool s_ui_signal_expired;
static uint32_t s_signal_sample_ms, s_disconnect_epoch[LIOT_NW_MAX_SIM_NUM];
/* Only the network worker advances the filter and diagnostic cadence. */
static tirtc_signal_filter_t s_signal_filter;
static uint32_t s_signal_log_ms;
static bool s_signal_logged;

static bool has_signal(const tirtc_ui_network_t *snapshot)
{
    return snapshot->signal_valid || snapshot->signal_rsrq_valid ||
           snapshot->signal_snr_valid || snapshot->signal_rssi_valid;
}

static void clear_signal(tirtc_ui_network_t *snapshot)
{
    snapshot->signal_valid = snapshot->signal_rsrq_valid = false;
    snapshot->signal_snr_valid = snapshot->signal_rssi_valid = false;
    snapshot->signal_bars = 0;
    snapshot->signal_dbm = snapshot->signal_rsrq_x2 = 0;
    snapshot->signal_snr_db = snapshot->signal_rssi_dbm = 0;
}

static void next_generation(void)
{ if (++s_cache.generation == 0U) ++s_cache.generation; }
void tirtc_network_get_snapshot(tirtc_network_snapshot_t *out)
{
    uint32_t sampled, now;
    if (!out) return;
    liot_rtos_enter_critical();
    *out = s_cache; sampled = s_cache_sample_ms;
    liot_rtos_exit_critical();
    now = liot_rtos_get_system_tick();
    if (out->ready && (uint32_t)(now - sampled) >= ROUTE_STALE_MS) {
        liot_rtos_enter_critical();
        if (s_cache.ready && s_cache_sample_ms == sampled && s_cache.generation == out->generation) {
            s_cache.ready = false; next_generation();
        }
        *out = s_cache;
        liot_rtos_exit_critical();
    }
}
bool tirtc_network_is_ready(void)
{ tirtc_network_snapshot_t s; tirtc_network_get_snapshot(&s); return s.ready; }
bool tirtc_network_time_ready(void)
{ tirtc_network_snapshot_t s; tirtc_network_get_snapshot(&s); return s.time_valid; }
void tirtc_network_set_time_valid(bool valid)
{ liot_rtos_enter_critical(); s_cache.time_valid = valid; liot_rtos_exit_critical(); }
static bool update_route(uint8_t sim, tirtc_ui_network_t *snapshot,
                         uint32_t disconnect_epoch, uint32_t signal_sampled)
{
    uint32_t now = liot_rtos_get_system_tick();
    int slot = sim <= LIOT_SIM_2 ? (int)sim : -1;
    bool accepted = true;
    if (has_signal(snapshot) && (uint32_t)(now - signal_sampled) >= SIGNAL_STALE_MS)
        clear_signal(snapshot);
    liot_rtos_enter_critical();
    /* A PDP callback can arrive while any synchronous modem query is waiting.
     * Never resurrect its old IP/registration with a pre-disconnect sample. */
    if (slot >= 0 && s_disconnect_epoch[slot] != disconnect_epoch) {
        accepted = false;
        snapshot->network_status_valid = snapshot->network_connected = false;
        snapshot->network_connecting = false;
        snapshot->registration = TIRTC_REG_UNKNOWN;
        snapshot->ip_address[0] = 0;
        clear_signal(snapshot);
        strcpy(snapshot->network_message, "移动数据连接已中断，正在恢复");
    }
    if (s_cache.ready != snapshot->network_connected || s_cache.sim_id != slot ||
        (snapshot->network_connected && strcmp(s_cache_ip, snapshot->ip_address))) next_generation();
    s_cache.ready = snapshot->network_connected; s_cache.sim_id = slot;
    s_cache_sample_ms = now;
    s_ui_cache = *snapshot; s_ui_cache_have = true; s_ui_cache_expired = false; s_ui_disconnected = false;
    s_signal_sample_ms = signal_sampled; s_ui_signal_expired = false;
    if (snapshot->network_connected) {
        strncpy(s_cache_ip, snapshot->ip_address, sizeof(s_cache_ip) - 1U);
        s_cache_ip[sizeof(s_cache_ip) - 1U] = 0;
    } else s_cache_ip[0] = 0;
    liot_rtos_exit_critical();
    return accepted;
}
void tirtc_network_publish(void)
{
    tirtc_network_snapshot_t route;
    tirtc_ui_network_t ui;
    uint32_t now;
    bool disconnected = false, route_expired = false, publish = false;
    tirtc_network_get_snapshot(&route); /* expire public ready/generation first */
    now = liot_rtos_get_system_tick();
    liot_rtos_enter_critical();
    if (s_ui_cache_have) {
        disconnected = s_ui_disconnected;
        route_expired = !s_ui_cache_expired &&
                        (uint32_t)(now - s_cache_sample_ms) >= ROUTE_STALE_MS;
        if (disconnected || route_expired) {
            ui = s_ui_cache;
            ui.network_status_valid = false; ui.network_connected = false;
            ui.network_connecting = false; ui.ip_address[0] = 0;
            clear_signal(&ui);
            strcpy(ui.network_message, disconnected ? "移动数据连接已中断，正在恢复" : "网络状态采集超时，当前连接未知");
            s_ui_cache = ui; s_ui_cache_expired = true;
            s_ui_signal_expired = true; s_ui_disconnected = false; publish = true;
        } else if (!s_ui_signal_expired && has_signal(&s_ui_cache) &&
                   (uint32_t)(now - s_signal_sample_ms) >= SIGNAL_STALE_MS) {
            /* A slow modem call must not leave old full bars on screen.
             * Expiring radio data alone does not tear down a valid PDP. */
            ui = s_ui_cache; clear_signal(&ui); s_ui_cache = ui;
            s_ui_signal_expired = true; publish = true;
        }
    }
    liot_rtos_exit_critical();
    if (publish) {
        (void)tirtc_ui_publish_network(&ui);
        if (disconnected || route_expired) {
            tirtc_status_event(disconnected ? "移动数据连接中断" : "网络状态采集超时");
            liot_trace("[network10] cached UI invalidated: %s", disconnected ? "disconnect event" : "sample expired");
        } else liot_trace("[NET41] radio sample expired; bars cleared");
    }
}
static void reset_recovery(uint8_t sim)
{
    s_route_sim = sim; s_pending = false; s_pending_sim = LIOT_SIM_INVALID;
    s_retry_wait = false; s_retry_step = 0; s_attempted = false;
    memset(&s_signal_filter, 0, sizeof(s_signal_filter));
}
static void schedule_retry(uint32_t now)
{
    static const uint32_t delays[] = {5000,10000,20000,40000,60000,120000,300000};
    unsigned index = s_retry_step;
    if (index >= sizeof(delays) / sizeof(delays[0])) index = sizeof(delays) / sizeof(delays[0]) - 1U;
    s_retry_delay = delays[index]; s_retry_since = now; s_retry_wait = true;
    if (s_retry_step < sizeof(delays) / sizeof(delays[0])) ++s_retry_step;
}
/* The F6D dispatcher sends the common.h 0x860100xx indications, NOT the
 * unrelated 1/2/3 datacall_event_id enum. Callback data is only a wake hint:
 * success never establishes a route; disconnect conservatively invalidates
 * a matching SIM immediately and is then verified by a fresh modem poll. */
static void datacall_event(uint8_t sim, unsigned int event, int cid, bool result, void *ctx)
{
    (void)ctx;
    if (cid != NETWORK_CID || sim > LIOT_SIM_2) return;
    if (event != (unsigned int)LIOT_DATACALL_ACT_RSP_IND && event != (unsigned int)LIOT_DATACALL_DEACT_RSP_IND &&
        event != (unsigned int)LIOT_DATACALL_PDP_DEACTIVE_IND) return;
    if (event == (unsigned int)LIOT_DATACALL_PDP_DEACTIVE_IND ||
        (event == (unsigned int)LIOT_DATACALL_DEACT_RSP_IND && result)) {
        liot_rtos_enter_critical();
        ++s_disconnect_epoch[sim];
        if (s_cache.sim_id == (int)sim) {
            if (s_cache.ready) next_generation();
            s_cache.ready = false; s_ui_disconnected = true;
        }
        liot_rtos_exit_critical();
    }
    if (s_wake != NULL) (void)liot_rtos_semaphore_release(s_wake);
}
static struct {
    bool have, valid, connected, connecting;
    tirtc_ui_sim_state_t sim;
    tirtc_ui_registration_t registration;
    char message[96];
} s_last_event;

static void copy_text(char *to, size_t capacity, const char *from, size_t available)
{
    size_t length = 0;
    if (!capacity) return;
    while (length < available && length + 1U < capacity && from[length]) {
        to[length] = from[length]; length++;
    }
    to[length] = '\0';
}
#define MESSAGE(snapshot, literal) \
    copy_text((snapshot)->network_message, sizeof((snapshot)->network_message), literal, sizeof(literal))

/* Report actual transitions only. Signal-strength fluctuations do not flood
 * the event page, and identifiers/APN/passwords are never printed here. */
static void publish_network(uint8_t sim, tirtc_ui_network_t *snapshot,
                            uint32_t disconnect_epoch, uint32_t signal_sampled)
{
    if (!update_route(sim, snapshot, disconnect_epoch, signal_sampled)) {
        liot_trace("[NET41] discarded poll interrupted by PDP disconnect");
    }
    if (!snapshot->signal_valid) memset(&s_signal_filter, 0, sizeof(s_signal_filter));
    if (!s_last_event.have || s_last_event.valid != snapshot->network_status_valid ||
        s_last_event.connected != snapshot->network_connected ||
        s_last_event.connecting != snapshot->network_connecting ||
        s_last_event.sim != snapshot->sim_state ||
        s_last_event.registration != snapshot->registration ||
        strcmp(s_last_event.message, snapshot->network_message)) {
        tirtc_status_event(snapshot->network_message);
        liot_trace("[network10] valid=%u sim=%u reg=%u data=%u pending=%u: %s",
                   (unsigned)snapshot->network_status_valid, (unsigned)snapshot->sim_state,
                   (unsigned)snapshot->registration, (unsigned)snapshot->network_connected,
                   (unsigned)snapshot->network_connecting, snapshot->network_message);
        s_last_event.have = true; s_last_event.valid = snapshot->network_status_valid;
        s_last_event.connected = snapshot->network_connected;
        s_last_event.connecting = snapshot->network_connecting;
        s_last_event.sim = snapshot->sim_state; s_last_event.registration = snapshot->registration;
        copy_text(s_last_event.message, sizeof(s_last_event.message), snapshot->network_message,
                  sizeof(snapshot->network_message));
    }
    (void)tirtc_ui_publish_network(snapshot);
}

static tirtc_ui_sim_state_t sim_state(liot_sim_status_e status)
{
    switch (status) {
    case LIOT_SIM_STATUS_READY:
    case LIOT_SIM_STATUS_PIN1_READY: return TIRTC_SIM_READY;
    case LIOT_SIM_STATUS_NOSIM: return TIRTC_SIM_ABSENT;
    case LIOT_SIM_STATUS_SIMPIN:
    case LIOT_SIM_STATUS_SIMPIN2: return TIRTC_SIM_PIN_REQUIRED;
    case LIOT_SIM_STATUS_SIMPUK:
    case LIOT_SIM_STATUS_SIMPUK2:
    case LIOT_SIM_STATUS_PIN1BLOCK:
    case LIOT_SIM_STATUS_PIN2BLOCK: return TIRTC_SIM_PUK_REQUIRED;
    case LIOT_SIM_STATUS_UNKNOW:
    case LIOT_SIM_STATUS_SIM_PRESENT:
    case LIOT_SIM_STATUS_PIN1_DISABLE: return TIRTC_SIM_UNKNOWN;
    case LIOT_SIM_STATUS_PHONE_TO_SIMPIN:
    case LIOT_SIM_STATUS_PHONE_TO_FIRST_SIMPIN:
    case LIOT_SIM_STATUS_PHONE_TO_FIRST_SIMPUK:
    case LIOT_SIM_STATUS_NETWORKPIN:
    case LIOT_SIM_STATUS_NETWORKPUK:
    case LIOT_SIM_STATUS_NETWORK_SUBSETPIN:
    case LIOT_SIM_STATUS_NETWORK_SUBSETPUK:
    case LIOT_SIM_STATUS_PROVIDERPIN:
    case LIOT_SIM_STATUS_PROVIDERPUK:
    case LIOT_SIM_STATUS_CORPORATEPIN:
    case LIOT_SIM_STATUS_CORPORATEPUK: return TIRTC_SIM_ERROR;
    default: return TIRTC_SIM_UNKNOWN;
    }
}

static tirtc_ui_registration_t registration(liot_nw_reg_state_e value)
{
    switch (value) {
    case LIOT_NW_REG_STATE_NOT_REGISTERED: return TIRTC_REG_NOT_REGISTERED;
    case LIOT_NW_REG_STATE_TRYING_ATTACH_OR_SEARCHING: return TIRTC_REG_SEARCHING;
    case LIOT_NW_REG_STATE_HOME_NETWORK: return TIRTC_REG_HOME;
    case LIOT_NW_REG_STATE_ROAMING: return TIRTC_REG_ROAMING;
    case LIOT_NW_REG_STATE_DENIED: return TIRTC_REG_DENIED;
    default: return TIRTC_REG_UNKNOWN;
    }
}

static bool registered(const tirtc_ui_network_t *state)
{ return state->registration == TIRTC_REG_HOME || state->registration == TIRTC_REG_ROAMING; }

/* The detailed API supplies CSQ as well as LTE measurements in one request.
 * RSRP/RSRQ are CESQ indices, not dBm; SNR is already whole dB. F6D may
 * return 127/-1 as unavailable, so the helper validates every field's range.
 * This synchronous query stays in this worker, never in a GUI callback. */
static uint32_t read_signal(uint8_t sim, tirtc_ui_network_t *snapshot)
{
    liot_nw_signal_strength_info_s raw;
    tirtc_signal_sample_t signal;
    uint32_t now;
    int result;
    raw.rssi = raw.bitErrorRate = 99;
    raw.rsrp = raw.rsrq = 255; raw.snr = 127;
    result = liot_nw_get_signal_strength(sim, &raw);
    now = liot_rtos_get_system_tick();
    signal = tirtc_signal_decode(result == LIOT_NW_SUCCESS,
                                raw.rssi, raw.rsrp, raw.rsrq, raw.snr);
    snapshot->signal_valid = signal.valid;
    snapshot->signal_dbm = signal.rsrp_dbm;
    snapshot->signal_rsrq_valid = signal.rsrq_valid;
    snapshot->signal_snr_valid = signal.snr_valid;
    snapshot->signal_rssi_valid = signal.rssi_valid;
    snapshot->signal_rsrq_x2 = signal.rsrq_x2;
    snapshot->signal_snr_db = signal.snr_db;
    snapshot->signal_rssi_dbm = signal.rssi_dbm;
    snapshot->signal_bars = tirtc_signal_filter_update(&s_signal_filter, signal.bars, signal.valid);
    if (!s_signal_logged || (uint32_t)(now - s_signal_log_ms) >= SIGNAL_LOG_MS) {
        s_signal_logged = true; s_signal_log_ms = now;
        TIRTC_LOG_DEBUG("[NET41] radio ret=%d raw=%d/%d/%d/%d rsrp=%d rsrq_x2=%d snr=%d valid=%u q=%u/%u bars=%u sample_ms=%lu",
                   result, raw.rssi, raw.rsrp, raw.rsrq, raw.snr,
                   (int)signal.rsrp_dbm, (int)signal.rsrq_x2, (int)signal.snr_db,
                   (unsigned)signal.valid, (unsigned)signal.rsrq_valid,
                   (unsigned)signal.snr_valid, (unsigned)snapshot->signal_bars, (unsigned long)now);
    }
    return now;
}

/* The SDK/lwIP address words contain network-order bytes. Reject unspecified,
 * loopback, link-local, and multicast addresses: none proves a cellular data IP. */
static bool valid_v4(const liot_ip4_addr_t *ip)
{
    const unsigned char *b = (const unsigned char *)&ip->addr;
    return b[0] != 0U && b[0] != 127U && b[0] < 224U &&
           !(b[0] == 169U && b[1] == 254U);
}
static bool valid_v6(const liot_ip6_addr_t *ip)
{
    const unsigned char *b = (const unsigned char *)ip->addr;
    unsigned i, nonzero = 0;
    if (b[0] == 0xFFU || (b[0] == 0xFEU && (b[1] & 0xC0U) == 0x80U)) return false;
    for (i = 0; i < 15; i++) nonzero |= b[i];
    return nonzero != 0U || b[15] > 1U;
}
static bool read_ip(tirtc_ui_network_t *snapshot, liot_data_call_info_t *info)
{
    const unsigned char *b;
    int count;
    /* Avoid SDK ntoa's shared static buffers: other service tasks use them too. */
    if (info->v4.state == 1 && valid_v4(&info->v4.addr.ip)) {
        b = (const unsigned char *)&info->v4.addr.ip.addr;
        count = snprintf(snapshot->ip_address, sizeof(snapshot->ip_address), "%u.%u.%u.%u",
                         (unsigned)b[0], (unsigned)b[1], (unsigned)b[2], (unsigned)b[3]);
    } else if (info->v6.state == 1 && valid_v6(&info->v6.addr.ip)) {
        b = (const unsigned char *)info->v6.addr.ip.addr;
        count = snprintf(snapshot->ip_address, sizeof(snapshot->ip_address),
                "%x:%x:%x:%x:%x:%x:%x:%x",
                (unsigned)b[0]*256U+b[1], (unsigned)b[2]*256U+b[3],
                (unsigned)b[4]*256U+b[5], (unsigned)b[6]*256U+b[7],
                (unsigned)b[8]*256U+b[9], (unsigned)b[10]*256U+b[11],
                (unsigned)b[12]*256U+b[13], (unsigned)b[14]*256U+b[15]);
    } else return false;
    return count > 0 && (size_t)count < sizeof(snapshot->ip_address);
}
/* F6D get_info writes state=1 when an address is present, state=0 when absent.
 * It does not return liot_datacall_state_e. An inactive context may leave
 * ip_version=0. The type can also be overwritten while collecting DNS data. */
static bool has_address(const liot_data_call_info_t *info)
{ return (info->v4.state == 1 && valid_v4(&info->v4.addr.ip)) ||
         (info->v6.state == 1 && valid_v6(&info->v6.addr.ip)); }
static bool valid_info(const liot_data_call_info_t *info)
{
    return info->cid == NETWORK_CID && info->ip_version >= 0 && info->ip_version <= 3 &&
           info->v4.state >= 0 && info->v4.state <= 1 && info->v6.state >= 0 && info->v6.state <= 1;
}

static void recover_data(uint8_t sim, tirtc_ui_network_t *snapshot,
                         const liot_data_call_info_t *info, bool info_valid, bool active)
{
    Liot_DataCallCFG_t config;
    char apn[LIOT_APN_LEN_MAX];
    int result;
    s_last_attempt = liot_rtos_get_system_tick(); s_attempted = true;
    schedule_retry(s_last_attempt);
    if (snapshot->sim_state != TIRTC_SIM_READY) {
        MESSAGE(snapshot, "SIM 未就绪，未发起连接"); return;
    }
    if (!registered(snapshot)) {
        MESSAGE(snapshot, "注册未确认，未发起连接"); return;
    }
    if (!info_valid) { MESSAGE(snapshot, "数据状态未知，未发起连接"); return; }
    if (active || has_address(info)) {
        MESSAGE(snapshot, "数据通道已激活，保留当前连接"); return;
    }
    if (s_pending) {
        MESSAGE(snapshot, "正在等待数据连接结果"); return;
    }
    /* Public SDK exposes APN/IP configuration, but no credential getter.
     * AUTO selects PAP/CHAP without clearing stored username/password; it
     * does update the auth protocol. NONE explicitly clears credentials.
     * Empty APN preserves the platform's automatic APN selection strategy. */
    memset(&config, 0, sizeof(config));
    config.method = LIOT_DATACALL_APN_GET;
    if (Liot_DataCallCfgDefaultEpsBearer(&config) != LIOT_DATACALL_SUCCESS ||
        /* F6D_A liot_start_data_call checks strlen(apn) <= 99, reserving NUL. */
        config.apn_len >= LIOT_APN_LEN_MAX ||
        config.ip_version < LIOT_PS_PDN_TYPE_IP_V4 || config.ip_version > LIOT_PS_PDN_TYPE_IP_V4V6) {
        MESSAGE(snapshot, "无法读取已有网络配置，未发起连接"); return;
    }
    copy_text(apn, sizeof(apn), config.apn, config.apn_len);
    /* The active slot or PDP can change while configuration I/O is waiting. */
    {
        uint8_t current_sim = LIOT_SIM_INVALID;
        liot_sim_status_e current_card = LIOT_SIM_STATUS_UNKNOW;
        liot_nw_reg_status_info_s current_reg;
        liot_data_call_info_t current_info;
        memset(&current_reg, 0, sizeof(current_reg));
        current_reg.data_reg.state = LIOT_NW_REG_STATE_UNKNOWN;
        memset(&current_info, 0, sizeof(current_info));
        if (liot_sim_get_slot(&current_sim) != LIOT_SIM_SUCCESS || current_sim != sim) {
            MESSAGE(snapshot, "SIM 状态已变化，请刷新后重试"); return;
        }
        if (liot_sim_get_card_status(sim, &current_card) != LIOT_SIM_SUCCESS ||
            sim_state(current_card) != TIRTC_SIM_READY ||
            liot_nw_get_reg_status(sim, &current_reg) != LIOT_NW_SUCCESS ||
            (current_reg.data_reg.state != LIOT_NW_REG_STATE_HOME_NETWORK &&
             current_reg.data_reg.state != LIOT_NW_REG_STATE_ROAMING) ||
            liot_get_data_call_info(sim, NETWORK_CID, &current_info) != LIOT_DATACALL_SUCCESS ||
            !valid_info(&current_info)) {
            MESSAGE(snapshot, "连接状态已变化或未知，请刷新后重试"); return;
        }
        if (liot_datacall_get_sim_profile_is_active(sim, NETWORK_CID) || has_address(&current_info)) {
            MESSAGE(snapshot, "数据通道已激活，保留当前连接"); return;
        }
    }
    result = liot_set_data_call_asyn_mode(sim, NETWORK_CID, true);
    if (result == LIOT_DATACALL_SUCCESS)
        result = liot_start_data_call(sim, NETWORK_CID, config.ip_version, apn,
                                     (CHAR *)"", (CHAR *)"", LIOT_DATA_AUTH_TYPE_AUTO);
    if (result != LIOT_DATACALL_SUCCESS) {
        liot_trace("[network10] recovery request failed: %d", result);
        MESSAGE(snapshot, "数据连接请求失败，请刷新状态"); return;
    }
    s_retry_wait = false;
    if (s_retry_step) --s_retry_step; /* accepted is not a failed attempt yet */
    s_pending_sim = sim; s_pending = true; s_pending_since = liot_rtos_get_system_tick();
    snapshot->network_connecting = true;
    MESSAGE(snapshot, "已请求数据连接，等待分配 IP");
}

/* This function and every modem API above execute only in the worker task. */
static void network_poll_once(bool reconnect)
{
    tirtc_ui_network_t snapshot;
    liot_sim_status_e card = LIOT_SIM_STATUS_UNKNOW;
    liot_nw_reg_status_info_s reg;
    liot_nw_operator_info_s operator;
    liot_data_call_info_t info;
    uint8_t sim = LIOT_SIM_INVALID;
    uint32_t epochs[LIOT_NW_MAX_SIM_NUM], signal_sampled = 0;
    bool info_valid = false, active = false;
    memset(&snapshot, 0, sizeof(snapshot));
    memset(&info, 0, sizeof(info));
    memset(&reg, 0, sizeof(reg));
    reg.data_reg.state = LIOT_NW_REG_STATE_UNKNOWN;
    snapshot.sim_state = TIRTC_SIM_UNKNOWN;
    snapshot.registration = TIRTC_REG_UNKNOWN;
    MESSAGE(&snapshot, "正在读取移动网络状态");
    liot_rtos_enter_critical();
    memcpy(epochs, s_disconnect_epoch, sizeof(epochs));
    liot_rtos_exit_critical();

    if (liot_sim_get_slot(&sim) != LIOT_SIM_SUCCESS || sim > LIOT_SIM_2) {
        reset_recovery(LIOT_SIM_INVALID);
        MESSAGE(&snapshot, "当前 SIM 状态读取失败");
        publish_network(LIOT_SIM_INVALID, &snapshot, 0, 0); return;
    }
    if (sim != s_route_sim) reset_recovery(sim);
    if (liot_sim_get_card_status(sim, &card) == LIOT_SIM_SUCCESS)
        snapshot.sim_state = sim_state(card);
    if (sim != s_pending_sim || snapshot.sim_state != TIRTC_SIM_READY) s_pending = false;
    if (snapshot.sim_state != TIRTC_SIM_READY) {
        snapshot.network_status_valid = snapshot.sim_state != TIRTC_SIM_UNKNOWN;
        if (snapshot.sim_state == TIRTC_SIM_ABSENT) MESSAGE(&snapshot, "未检测到 SIM 卡");
        else if (snapshot.sim_state == TIRTC_SIM_UNKNOWN) MESSAGE(&snapshot, "SIM 状态未知，请稍后刷新");
        else MESSAGE(&snapshot, "SIM 未就绪，请检查 SIM 状态");
        publish_network(sim, &snapshot, epochs[sim], 0); return;
    }
    if (liot_nw_get_reg_status(sim, &reg) == LIOT_NW_SUCCESS) {
        snapshot.registration = registration(reg.data_reg.state);
        snapshot.network_roaming = snapshot.registration == TIRTC_REG_ROAMING;
        if (registered(&snapshot) && reg.data_reg.act == LIOT_NW_ACCESS_TECH_E_UTRAN)
            copy_text(snapshot.network_type, sizeof(snapshot.network_type), "4G LTE", sizeof("4G LTE"));
    }
    if (registered(&snapshot)) {
        signal_sampled = read_signal(sim, &snapshot);
        memset(&operator, 0, sizeof(operator));
        if (liot_nw_get_operator_name(sim, &operator) == LIOT_NW_SUCCESS) {
            if (operator.long_oper_name[0])
                copy_text(snapshot.operator_name, sizeof(snapshot.operator_name), operator.long_oper_name, sizeof(operator.long_oper_name));
            else copy_text(snapshot.operator_name, sizeof(snapshot.operator_name), operator.short_oper_name, sizeof(operator.short_oper_name));
        }
    }
    info_valid = liot_get_data_call_info(sim, NETWORK_CID, &info) == LIOT_DATACALL_SUCCESS && valid_info(&info);
    active = liot_datacall_get_sim_profile_is_active(sim, NETWORK_CID);
    if (info_valid) {
        snapshot.network_status_valid = true;
        copy_text(snapshot.apn, sizeof(snapshot.apn), info.apn_name, sizeof(info.apn_name));
        snapshot.network_connected = registered(&snapshot) && active && read_ip(&snapshot, &info);
    }
    if (snapshot.network_connected) {
        uint8_t current_sim = LIOT_SIM_INVALID;
        if (liot_sim_get_slot(&current_sim) != LIOT_SIM_SUCCESS || current_sim != sim) {
            snapshot.network_connected = false; snapshot.network_status_valid = false;
            snapshot.network_connecting = false; snapshot.ip_address[0] = 0;
            MESSAGE(&snapshot, "SIM 状态已变化，等待重新确认");
            reset_recovery(LIOT_SIM_INVALID);
            clear_signal(&snapshot);
            publish_network(LIOT_SIM_INVALID, &snapshot, 0, 0); return;
        }
    }
    if (!registered(&snapshot)) s_pending = false;
    if (snapshot.network_connected) {
        s_pending = false; s_retry_wait = false; s_retry_step = 0; s_attempted = false;
    } else if (s_pending && (uint32_t)(liot_rtos_get_system_tick() - s_pending_since) >= CONNECT_PENDING_TICKS) {
        s_pending = false; schedule_retry(liot_rtos_get_system_tick());
    }
    snapshot.network_connecting = s_pending;
    if (snapshot.network_connected) MESSAGE(&snapshot, "移动数据已连接");
    else if (snapshot.network_connecting) MESSAGE(&snapshot, "正在等待移动数据连接");
    else if (!info_valid) MESSAGE(&snapshot, "数据状态读取失败，当前状态未知");
    else if (snapshot.registration == TIRTC_REG_UNKNOWN) MESSAGE(&snapshot, "注册状态未知，数据未连接");
    else if (!registered(&snapshot)) MESSAGE(&snapshot, "尚未完成网络注册");
    else if (active) MESSAGE(&snapshot, "数据通道已激活，尚无有效 IP");
    else if (info_valid && has_address(&info)) MESSAGE(&snapshot, "已有 IP，数据激活状态待确认");
    else MESSAGE(&snapshot, "已注册，移动数据未连接");
    {
        uint32_t now = liot_rtos_get_system_tick();
        bool manual = reconnect && (!s_attempted || (uint32_t)(now - s_last_attempt) >= MANUAL_MIN_MS);
        bool due = !s_retry_wait || (uint32_t)(now - s_retry_since) >= s_retry_delay;
        if (!snapshot.network_connected && registered(&snapshot) && info_valid &&
            !active && !has_address(&info) && !s_pending && (manual || due))
            recover_data(sim, &snapshot, &info, info_valid, active);
        else if (s_retry_wait && !snapshot.network_connected && !s_pending && registered(&snapshot) && info_valid)
            MESSAGE(&snapshot, "数据连接未完成，等待自动重试");
    }
    publish_network(sim, &snapshot, epochs[sim], signal_sampled);
}

static unsigned take_requests(void)
{
    unsigned requests;
    tirtc_ui_platform_lock(); requests = s_requests; s_requests = 0; tirtc_ui_platform_unlock();
    return requests;
}
static void ensure_time_service(void)
{
    uint32_t now;
    if (s_time_service_started) return;
    now = liot_rtos_get_system_tick();
    if (s_time_service_attempted &&
        (uint32_t)(now - s_time_service_attempt_ms) < TIME_SERVICE_RETRY_MS)
        return;
    s_time_service_attempted = true;
    s_time_service_attempt_ms = now;
    if (tirtc_network_time_start() == 0) {
        s_time_service_started = true;
        if (s_time_service_failure_reported)
            tirtc_status_event("网络校时服务已恢复");
        return;
    }
    if (!s_time_service_failure_reported) {
        s_time_service_failure_reported = true;
        tirtc_status_event("网络校时服务启动失败，将重试");
        liot_trace("[network10] time service unavailable; retry after %u ms",
                   (unsigned)TIME_SERVICE_RETRY_MS);
    }
}

static void network_task(void *context)
{
    (void)context;
    liot_trace("[NET41] CID1 automatic recovery; radio poll=2000ms stale=6000ms; configured APN retained");
    if (!s_callback_registered) {
        s_callback_registered = liot_datacall_register_cb(LIOT_DATACALL_REGISTER_ALL_SIM,
                NETWORK_CID, datacall_event, NULL) == LIOT_DATACALL_SUCCESS;
        if (!s_callback_registered) liot_trace("[network10] event registration failed; polling remains active");
    }
    for (;;) {
        unsigned requests = take_requests();
        /* Try before modem I/O, and throttle retries even when user actions
         * repeatedly wake this task. The time API is itself single-flight. */
        ensure_time_service();
        network_poll_once((requests & REQUEST_RECONNECT) != 0);
        (void)liot_rtos_semaphore_wait(s_wake, NETWORK_POLL_TICKS);
    }
}

int tirtc_network_start(void)
{
    if (s_task != NULL) return 0;
    /* F6D_A semaphore_delete waits for a token before freeing it. Keep this
     * empty wake semaphore on creation failure so a later start can retry. */
    if (s_wake == NULL && liot_rtos_semaphore_create(&s_wake, 0U) != LIOT_OSI_SUCCESS) return -1;
    if (liot_rtos_task_create(&s_task, NETWORK_TASK_STACK, NETWORK_TASK_PRIORITY,
                             "tirtc_net", network_task, NULL) != LIOT_OSI_SUCCESS || s_task == NULL) {
        s_task = NULL; return -1;
    }
    tirtc_ui_platform_lock(); s_running = true; tirtc_ui_platform_unlock();
    return 0;
}

int tirtc_network_action(const tirtc_ui_action_t *action, void *context)
{
    unsigned request;
    bool running;
    (void)context;
    if (!action) return -1;
    if (action->type == TIRTC_ACTION_NETWORK_REFRESH) request = REQUEST_REFRESH;
    else if (action->type == TIRTC_ACTION_NETWORK_RECONNECT) request = REQUEST_RECONNECT;
    else return -1;
    tirtc_ui_platform_lock();
    running = s_running;
    if (running) s_requests |= request;
    tirtc_ui_platform_unlock();
    if (!running) return -1;
    /* Wake semaphore coalesces repeated taps; release never waits for modem I/O. */
    (void)liot_rtos_semaphore_release(s_wake);
    return 0;
}
