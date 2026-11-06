/* Controlled SNTP for the bundled F6D_A SDK. Public socket/RTC APIs only.
 * One persistent worker owns DNS/socket operations. No UI calls or busy waits.
 * Unlike the SDK NTP client, errors cannot report success and timezone is kept.
 */
#include "tirtc_network.h"
#include "tirtc_network_time.h"
#include "../status/tirtc_status.h"
#include "liot_os.h"
#include "liot_rtc.h"
#include "liot_sockets.h"
#include "liot_log.h"
#include <string.h>

#define TIME_STACK_BYTES 6144U
#define TIME_POLL_MS 5000U
#define TIME_RETRY_MS 60000U
#define TIME_IO_MS 4000U
#define UNIX_2024 1704067200ULL
#define UNIX_2100 4102444800ULL
#define NTP_EPOCH_DELTA 2208988800ULL
static liot_task_t s_time_task;
static bool s_time_starting;
static bool s_time_create_failed;
static unsigned s_server;
static bool s_time_attempted;
static uint32_t s_time_attempt_ms;
static int s_time_result = 1000;

static bool leap(unsigned year)
{ return year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U); }
static unsigned month_days(unsigned year, unsigned month)
{
    static const unsigned char days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1U] + (month == 2U && leap(year) ? 1U : 0U);
}
static bool calendar_epoch(const liot_rtc_time_s *time, uint64_t *epoch)
{
    unsigned y, m; uint64_t days = 0;
    if (time->tm_year < 2024 || time->tm_year >= 2100 || time->tm_mon < 1 || time->tm_mon > 12 ||
        time->tm_mday < 1 || (unsigned)time->tm_mday > month_days((unsigned)time->tm_year, (unsigned)time->tm_mon) ||
        time->tm_hour < 0 || time->tm_hour > 23 || time->tm_min < 0 || time->tm_min > 59 ||
        time->tm_sec < 0 || time->tm_sec > 59) return false;
    for (y = 1970; y < (unsigned)time->tm_year; y++) days += leap(y) ? 366U : 365U;
    for (m = 1; m < (unsigned)time->tm_mon; m++) days += month_days(y, m);
    days += (unsigned)time->tm_mday - 1U;
    *epoch = days * 86400ULL + (unsigned)time->tm_hour * 3600U + (unsigned)time->tm_min * 60U + (unsigned)time->tm_sec;
    return true;
}
static bool epoch_calendar(uint64_t epoch, liot_rtc_time_s *time)
{
    uint64_t days; unsigned year = 1970, month = 1, count;
    if (epoch < UNIX_2024 || epoch >= UNIX_2100) return false;
    memset(time, 0, sizeof(*time));
    days = epoch / 86400U;
    time->tm_wday = (int)((days + 4U) % 7U);
    time->tm_sec = (int)(epoch % 60U); time->tm_min = (int)((epoch / 60U) % 60U);
    time->tm_hour = (int)((epoch / 3600U) % 24U);
    while (days >= (count = leap(year) ? 366U : 365U)) { days -= count; year++; }
    while (days >= (count = month_days(year, month))) { days -= count; month++; }
    time->tm_year = (int)year; time->tm_mon = (int)month; time->tm_mday = (int)days + 1;
    return true;
}
static uint32_t get_be32(const unsigned char *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static void put_be32(unsigned char *p, uint32_t v)
{ p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }
static bool validate_reply(const unsigned char *reply, size_t length,
                           const unsigned char *request, uint64_t *epoch)
{
    uint64_t ntp;
    unsigned version;
    if (length < 48U || length > 512U) return false;
    version = (reply[0] >> 3) & 7U;
    if ((reply[0] >> 6) == 3U || (version != 3U && version != 4U) ||
        (reply[0] & 7U) != 4U || reply[1] == 0U || reply[1] > 15U ||
        memcmp(reply + 24, request + 40, 8) != 0) return false;
    ntp = get_be32(reply + 40);
    if (!ntp && !get_be32(reply + 44)) return false;
    /* Resolve the 2036 NTP rollover into the explicitly supported 2024..2099 era. */
    if (ntp < NTP_EPOCH_DELTA + UNIX_2024) ntp += 0x100000000ULL;
    *epoch = ntp - NTP_EPOCH_DELTA;
    return *epoch >= UNIX_2024 && *epoch < UNIX_2100;
}
static bool same_route(const tirtc_network_snapshot_t *route)
{
    tirtc_network_snapshot_t now;
    tirtc_network_get_snapshot(&now);
    return now.ready && now.sim_id == route->sim_id && now.generation == route->generation;
}
static bool rtc_valid(void)
{
    liot_rtc_time_s time; uint64_t epoch;
    memset(&time, 0, sizeof(time));
    return liot_rtc_get_time(&time) == LIOT_RTC_SUCCESS && calendar_epoch(&time, &epoch);
}
static bool same_peer(const struct sockaddr *expected, const struct sockaddr *actual)
{
    if (expected->sa_family != actual->sa_family) return false;
    if (actual->sa_family == AF_INET) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)actual;
        const struct sockaddr_in *e = (const struct sockaddr_in *)expected;
        return a->sin_port == e->sin_port && a->sin_addr.s_addr == e->sin_addr.s_addr;
    }
    if (actual->sa_family == AF_INET6) {
        const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)actual;
        const struct sockaddr_in6 *e = (const struct sockaddr_in6 *)expected;
        return a->sin6_port == e->sin6_port && a->sin6_scope_id == e->sin6_scope_id &&
               memcmp(&a->sin6_addr, &e->sin6_addr, sizeof(a->sin6_addr)) == 0;
    }
    return false;
}
static int sync_time(const tirtc_network_snapshot_t *route)
{
    static const char *const servers[] = {"ntp.aliyun.com", "time.cloudflare.com"};
    struct addrinfo hints, *addresses = NULL, *address;
    unsigned char request[48], reply[512];
    struct sockaddr_storage peer;
    struct timeval timeout;
    liot_rtc_time_s target, verify;
    uint64_t epoch, actual;
    int result = -1, fd = -1, received;
    uint32_t started;
    socklen_t peer_size;
    memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; hints.ai_protocol = IPPROTO_UDP;
    /* DNS may wait inside the base stack. There is only this worker: no timeout
     * path starts another DNS task or destroys a task holding SDK resources. */
    if (lwip_getaddrinfowithcid(servers[s_server++ % 2U], "123", &hints, &addresses, 1) != 0 || !addresses) {
        if (addresses) lwip_freeaddrinfo(addresses);
        return -2;
    }
    if (!same_route(route)) { result = -3; goto done; }
    for (address = addresses; address; address = address->ai_next) {
        if (address->ai_family != AF_INET && address->ai_family != AF_INET6) continue;
        if (!address->ai_addr || address->ai_addrlen > sizeof(peer)) continue;
        if ((address->ai_family == AF_INET && address->ai_addrlen < sizeof(struct sockaddr_in)) ||
            (address->ai_family == AF_INET6 && address->ai_addrlen < sizeof(struct sockaddr_in6))) continue;
        fd = lwip_socket(address->ai_family, SOCK_DGRAM, IPPROTO_UDP);
        if (fd >= 0) break;
    }
    if (fd < 0 || !address) { result = -4; goto done; }
    if (lwip_bind_cid(fd, 1) != 0) { result = -5; goto done; }
    timeout.tv_sec = TIME_IO_MS / 1000U; timeout.tv_usec = 0;
    if (lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) { result = -6; goto done; }
    memset(request, 0, sizeof(request)); request[0] = 0x23; /* Version 4, client. */
    put_be32(request + 40, liot_true_rand()); put_be32(request + 44, liot_true_rand());
    if (!get_be32(request + 40) && !get_be32(request + 44)) request[47] = 1;
    started = liot_rtos_get_system_tick();
    if (lwip_sendto(fd, request, sizeof(request), 0, address->ai_addr, address->ai_addrlen) != (int)sizeof(request)) {
        result = -7; goto done;
    }
    peer_size = sizeof(peer); memset(&peer, 0, sizeof(peer));
    received = lwip_recvfrom(fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer, &peer_size);
    if (received < 0 || (uint32_t)(liot_rtos_get_system_tick() - started) > TIME_IO_MS * 2U) { result = -8; goto done; }
    if (peer_size < address->ai_addrlen || !same_peer(address->ai_addr, (const struct sockaddr *)&peer) ||
        !validate_reply(reply, (size_t)received, request, &epoch) || !same_route(route)) { result = -9; goto done; }
    if (!epoch_calendar(epoch, &target)) { result = -10; goto done; }
    /* F6D liot_rtc_set_time takes UTC and preserves the stored timezone;
     * get_localtime applies that offset separately. Never call set_timezone. */
    if (liot_rtc_set_time(&target) != LIOT_RTC_SUCCESS ||
        liot_rtc_get_time(&verify) != LIOT_RTC_SUCCESS || !calendar_epoch(&verify, &actual) ||
        actual < epoch || actual - epoch > 5U) { result = -11; goto done; }
    result = 0;
done:
    if (fd >= 0) (void)lwip_close(fd);
    lwip_freeaddrinfo(addresses);
    return result;
}
static void time_poll_once(void)
{
    tirtc_network_snapshot_t route;
    bool valid = rtc_valid();
    uint32_t now = liot_rtos_get_system_tick();
    int result = valid ? 0 : (s_time_result == 1000 || s_time_result == 0 ? -1 : s_time_result);
    tirtc_network_set_time_valid(valid);
    tirtc_network_get_snapshot(&route);
    if (!valid && route.ready && (!s_time_attempted || (uint32_t)(now - s_time_attempt_ms) >= TIME_RETRY_MS)) {
        s_time_attempted = true; s_time_attempt_ms = now;
        result = sync_time(&route);
        s_time_attempt_ms = liot_rtos_get_system_tick();
        valid = result == 0 && rtc_valid();
        tirtc_network_set_time_valid(valid);
    }
    if (result != s_time_result) {
        s_time_result = result;
        tirtc_status_event(valid ? "系统时间已就绪" : "系统时间未同步，等待网络校时");
        liot_trace("[network10] time ready=%u result=%d", (unsigned)valid, result);
    }
}
static void time_task(void *context)
{
    (void)context;
    for (;;) { time_poll_once(); liot_rtos_task_sleep_ms(TIME_POLL_MS); }
}
int tirtc_network_time_start(void)
{
    liot_task_t task = NULL; int result;
    bool failed, report;
    liot_rtos_enter_critical();
    if (s_time_task || s_time_starting) { liot_rtos_exit_critical(); return 0; }
    s_time_starting = true; liot_rtos_exit_critical();
    result = liot_rtos_task_create(&task, TIME_STACK_BYTES, 8, "tirtc_time", time_task, NULL);
    failed = result != LIOT_OSI_SUCCESS || !task;
    liot_rtos_enter_critical();
    if (!failed) s_time_task = task;
    report = failed && !s_time_create_failed;
    s_time_create_failed = failed;
    s_time_starting = false; liot_rtos_exit_critical();
    if (report) liot_trace("[network10] time worker creation failed");
    if (failed) return -1;
    return 0;
}
