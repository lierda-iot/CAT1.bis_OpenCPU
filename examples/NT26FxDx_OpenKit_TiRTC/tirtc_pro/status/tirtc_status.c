/* Read-only board facts. The modem worker cannot hold up the fast publisher. */
#include "tirtc_status.h"
#include "../ui/tirtc_ui.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "liot_os.h"
#include "liot_rtc.h"
#include "liot_dev.h"
#include "liot_audio2.h"
#include "liot_log.h"

#define STATUS_TASK_STACK 4096U
#define STATUS_TASK_PRIORITY 8U
#define STATUS_RETRY_MS 5000U
#define STATUS_EVENT_COUNT 8U
#define STATUS_EVENT_TEXT 72U
#define STATUS_BAD_VALUE (-1)

extern void tirtc_ui_platform_lock(void);
extern void tirtc_ui_platform_unlock(void);

typedef struct { uint32_t seconds; char text[STATUS_EVENT_TEXT]; } status_event_t;
typedef struct {
    bool imei_valid, version_valid;
    char imei[24], version[64];
} status_identity_t;
static status_identity_t s_identity;
static status_event_t s_events[STATUS_EVENT_COUNT];
static unsigned s_event_count, s_event_next;
static liot_task_t s_task;
static bool s_starting, s_uptime_started;
static bool s_start_reported;
static int s_start_result;
static uint32_t s_previous_ms;
static uint64_t s_elapsed_ms;
/* Each pair below has one owner: the fast publisher or the identity worker. */
static int s_clock_result = INT_MIN, s_audio_result = INT_MIN, s_audio_volume = -1;
static int s_imei_result = INT_MIN, s_version_result = INT_MIN;

/* Copy complete UTF-8 characters only; remove line/control characters from an
 * event or identity value. Invalid byte sequences become '?', never raw bytes. */
static size_t copy_line(char *out, size_t capacity, const char *in, size_t limit)
{
    size_t source = 0, dest = 0;
    if (!capacity) return 0;
    while (source < limit && in[source]) {
        unsigned char lead = (unsigned char)in[source];
        size_t width = lead < 0x80U ? 1U : (lead >= 0xC2U && lead <= 0xDFU ? 2U :
                       (lead >= 0xE0U && lead <= 0xEFU ? 3U :
                       (lead >= 0xF0U && lead <= 0xF4U ? 4U : 0U)));
        size_t i;
        bool valid = width != 0U && source + width <= limit;
        for (i = 1; valid && i < width; i++) {
            unsigned char next = (unsigned char)in[source + i];
            if (next < 0x80U || next > 0xBFU) valid = false;
        }
        if (valid && width >= 3U) {
            unsigned char second = (unsigned char)in[source + 1U];
            if ((lead == 0xE0U && second < 0xA0U) || (lead == 0xEDU && second > 0x9FU) ||
                (lead == 0xF0U && second < 0x90U) || (lead == 0xF4U && second > 0x8FU)) valid = false;
        }
        if (!valid) { if (dest + 1U >= capacity) break; out[dest++] = '?'; source++; continue; }
        if (dest + width >= capacity) break;
        if (width == 1U && (lead < 0x20U || lead == 0x7FU)) out[dest++] = ' ';
        else { memcpy(out + dest, in + source, width); dest += width; }
        source += width;
    }
    out[dest] = '\0';
    return dest;
}

/* Caller holds the short mailbox lock. Ignore a time captured by a different
 * task just before a newer sample; main publishes every five seconds, so the
 * wrap extension never has to distinguish a gap of 24 days from preemption. */
static uint32_t uptime_locked(uint32_t now)
{
    if (!s_uptime_started) {
        s_uptime_started = true; s_previous_ms = now; s_elapsed_ms = now;
    } else if ((int32_t)(now - s_previous_ms) >= 0) {
        s_elapsed_ms += (uint32_t)(now - s_previous_ms); s_previous_ms = now;
    }
    return (uint32_t)(s_elapsed_ms / 1000U);
}

void tirtc_status_event(const char *message)
{
    char text[STATUS_EVENT_TEXT];
    uint32_t now, seconds;
    unsigned previous;
    bool added = false;
    if (!message || !*message) return;
    copy_line(text, sizeof(text), message, STATUS_EVENT_TEXT - 1U);
    if (!text[0]) return;
    now = liot_rtos_get_running_time();
    tirtc_ui_platform_lock();
    seconds = uptime_locked(now);
    previous = (s_event_next + STATUS_EVENT_COUNT - 1U) % STATUS_EVENT_COUNT;
    if (!s_event_count || strcmp(s_events[previous].text, text)) {
        s_events[s_event_next].seconds = seconds;
        memcpy(s_events[s_event_next].text, text, strlen(text) + 1U);
        s_event_next = (s_event_next + 1U) % STATUS_EVENT_COUNT;
        if (s_event_count < STATUS_EVENT_COUNT) s_event_count++;
        added = true;
    }
    tirtc_ui_platform_unlock();
    if (added) liot_trace("[status09] uptime=%lu %s", (unsigned long)seconds, text);
}

static void result_event(int *previous, int result, const char *success, const char *failure)
{
    char message[STATUS_EVENT_TEXT];
    if (*previous == result) return;
    *previous = result;
    if (!result) tirtc_status_event(success);
    else {
        (void)snprintf(message, sizeof(message), "%s (%d)", failure, result);
        tirtc_status_event(message);
    }
}

static bool valid_calendar(const liot_rtc_time_s *time)
{
    static const unsigned char days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned max_day;
    if (time->tm_year < 2024 || time->tm_year > 2100 || time->tm_mon < 1 || time->tm_mon > 12 ||
        time->tm_hour < 0 || time->tm_hour > 23 || time->tm_min < 0 || time->tm_min > 59 ||
        time->tm_sec < 0 || time->tm_sec > 59 || time->tm_wday < 0 || time->tm_wday > 6) return false;
    max_day = days[time->tm_mon - 1];
    if (time->tm_mon == 2 && !(time->tm_year % 4) &&
        (time->tm_year % 100 || !(time->tm_year % 400))) max_day++;
    return time->tm_mday >= 1 && (unsigned)time->tm_mday <= max_day;
}

void tirtc_status_publish(void)
{
    tirtc_ui_system_t snapshot;
    status_event_t events[STATUS_EVENT_COUNT];
    status_identity_t identity;
    liot_rtc_time_s time;
    unsigned count, i;
    size_t used;
    uint32_t now;
    int clock_result, audio_result, volume = -1;
    memset(&snapshot, 0, sizeof(snapshot));
    memset(&time, 0, sizeof(time));
    /* get_localtime reads the SDK RTC and its configured timezone; no time
     * setters, NTP, UART, modem request, audio init, codec I2C or file I/O here. */
    clock_result = (int)liot_rtc_get_localtime(&time);
    if (!clock_result && !valid_calendar(&time)) clock_result = STATUS_BAD_VALUE;
    snapshot.clock_valid = clock_result == 0;
    if (snapshot.clock_valid) {
        (void)snprintf(snapshot.clock_text, sizeof(snapshot.clock_text), "%02d:%02d", time.tm_hour, time.tm_min);
        (void)snprintf(snapshot.date_text, sizeof(snapshot.date_text), "%04d-%02d-%02d", time.tm_year, time.tm_mon, time.tm_mday);
    } else {
        (void)snprintf(snapshot.clock_text, sizeof(snapshot.clock_text), "--:--");
        (void)snprintf(snapshot.date_text, sizeof(snapshot.date_text), "时间未同步");
    }
    result_event(&s_clock_result, clock_result, "RTC 本地时间可用", "RTC 未同步或读取失败");

    /* F6D_A Liot_AudioGetVolume loads audio_volumn_scale (software value).
     * Liot_AudioGetCodecVolume would call hardware and must not be used here. */
    audio_result = (int)Liot_AudioGetVolume(&volume);
    if (!audio_result && (volume < 0 || volume > 100)) audio_result = STATUS_BAD_VALUE;
    snapshot.audio_valid = audio_result == 0;
    result_event(&s_audio_result, audio_result, "音量设置读取成功", "音量设置读取失败");
    if (snapshot.audio_valid) {
        (void)snprintf(snapshot.diagnostic_audio, sizeof(snapshot.diagnostic_audio),
                       "音量设置：%d%%（软件值）\n录音 / 播放：尚未接入", volume);
        if (s_audio_volume >= 0 && s_audio_volume != volume) {
            char message[STATUS_EVENT_TEXT];
            (void)snprintf(message, sizeof(message), "音量设置变更：%d%%", volume);
            tirtc_status_event(message);
        }
        s_audio_volume = volume;
    } else {
        (void)snprintf(snapshot.diagnostic_audio, sizeof(snapshot.diagnostic_audio),
                       "音量设置：未获取（%d）\n录音 / 播放：尚未接入", audio_result);
        s_audio_volume = -1;
    }

    now = liot_rtos_get_running_time();
    tirtc_ui_platform_lock();
    (void)uptime_locked(now);
    identity = s_identity;
    count = s_event_count;
    for (i = 0; i < count; i++)
        events[i] = s_events[(s_event_next + STATUS_EVENT_COUNT - count + i) % STATUS_EVENT_COUNT];
    tirtc_ui_platform_unlock();
    snapshot.modem_imei_valid = identity.imei_valid;
    snapshot.modem_version_valid = identity.version_valid;
    memcpy(snapshot.modem_imei, identity.imei, sizeof(snapshot.modem_imei));
    memcpy(snapshot.modem_version, identity.version, sizeof(snapshot.modem_version));
    used = (size_t)snprintf(snapshot.diagnostic_events, sizeof(snapshot.diagnostic_events), "最近事件（最多 8 条）\n");
    if (!count) (void)snprintf(snapshot.diagnostic_events + used, sizeof(snapshot.diagnostic_events) - used, "暂无事件");
    for (i = 0; i < count && used < sizeof(snapshot.diagnostic_events) - 1U; i++) {
        int length = snprintf(snapshot.diagnostic_events + used, sizeof(snapshot.diagnostic_events) - used,
                              "%lus  %s%s", (unsigned long)events[i].seconds, events[i].text, i + 1U < count ? "\n" : "");
        if (length < 0 || (size_t)length >= sizeof(snapshot.diagnostic_events) - used) break;
        used += (size_t)length;
    }
    (void)tirtc_ui_publish_system(&snapshot);
}

static bool valid_imei(const char *imei, size_t capacity)
{
    unsigned i, nonzero = 0;
    if (capacity < 16U) return false;
    for (i = 0; i < 15U; i++) {
        if (imei[i] < '0' || imei[i] > '9') return false;
        nonzero |= (unsigned)(imei[i] - '0');
    }
    return nonzero != 0U && imei[15] == '\0';
}

static void identity_poll_once(void)
{
    bool need_imei, need_version;
    int result;
    tirtc_ui_platform_lock();
    need_imei = !s_identity.imei_valid; need_version = !s_identity.version_valid;
    tirtc_ui_platform_unlock();
    if (need_version) {
        char raw[256], version[64];
        size_t length = 0;
        memset(raw, 0, sizeof(raw)); memset(version, 0, sizeof(version));
        result = (int)liot_dev_get_firmware_version(raw, sizeof(raw));
        while (length < sizeof(raw) && raw[length]) length++;
        if (!result && (!length || length == sizeof(raw))) result = STATUS_BAD_VALUE;
        if (!result) {
            if (length < sizeof(version)) copy_line(version, sizeof(version), raw, length);
            else {
                size_t copied = copy_line(version, sizeof(version) - 3U, raw, length);
                memcpy(version + copied, "...", 4U);
            }
            tirtc_ui_platform_lock();
            memcpy(s_identity.version, version, sizeof(version)); s_identity.version_valid = true;
            tirtc_ui_platform_unlock();
        }
        result_event(&s_version_result, result, "模组版本读取成功", "模组版本读取失败");
    }
    if (need_imei) {
        char imei[24];
        memset(imei, 0, sizeof(imei));
        /* SDK appGetImeiNumSync can wait for the modem; never run in main/UI. */
        result = (int)liot_dev_get_imei(imei, sizeof(imei), 0);
        if (!result && !valid_imei(imei, sizeof(imei))) result = STATUS_BAD_VALUE;
        if (!result) {
            tirtc_ui_platform_lock();
            memcpy(s_identity.imei, imei, sizeof(imei)); s_identity.imei_valid = true;
            tirtc_ui_platform_unlock();
        }
        result_event(&s_imei_result, result, "模组 IMEI 读取成功", "模组 IMEI 读取失败");
    }
}

static void identity_worker(void *unused)
{
    (void)unused;
    for (;;) { identity_poll_once(); liot_rtos_task_sleep_ms(STATUS_RETRY_MS); }
}

int tirtc_status_start(void)
{
    liot_task_t task = NULL;
    int result;
    bool report;
    tirtc_ui_platform_lock();
    if (s_task || s_starting) { tirtc_ui_platform_unlock(); return 0; }
    s_starting = true;
    tirtc_ui_platform_unlock();
    result = (int)liot_rtos_task_create(&task, STATUS_TASK_STACK, STATUS_TASK_PRIORITY,
                                      "tirtc_status", identity_worker, NULL);
    if (!result && !task) result = STATUS_BAD_VALUE;
    tirtc_ui_platform_lock();
    if (!result) s_task = task;
    s_starting = false;
    report = !s_start_reported || s_start_result != result;
    s_start_reported = true; s_start_result = result;
    tirtc_ui_platform_unlock();
    if (report && !result) tirtc_status_event("系统信息任务已启动");
    else if (report) {
        char message[STATUS_EVENT_TEXT];
        (void)snprintf(message, sizeof(message), "系统信息任务创建失败 (%d)", result);
        tirtc_status_event(message);
    }
    return result ? -1 : 0;
}
