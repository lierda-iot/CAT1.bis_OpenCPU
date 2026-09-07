#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_nv.h"
#include "liot_os.h"
#include "liot_rtc.h"

#include "demo_salary_calculator.h"
#ifdef HWDEMO_MP3_EN
#include "demo_mp3.h"
#endif
#include "gui_guider.h"

#define SALARY_NVM_FILE "salary_calc.cfg"
#define SALARY_NVM_MAGIC 0x53414C31u
#define SALARY_WORK_DAYS 22u
#define SALARY_COIN_WAV_PATH "/extflash/salary/coin_burst.wav"
#define SALARY_COIN_COUNT 18
#define SALARY_TIMEZONE_HOURS 8
#define SALARY_BURST_COIN_COUNT 10
#define SALARY_REENTER_BLOCK_MS 2000
#define SALARY_EXIT_HOLD_MS 1200U
#define SALARY_EXIT_MOVE_LIMIT 12

typedef struct {
    uint32_t magic;
    uint32_t monthly_salary;
    uint32_t sound_interval;
    uint8_t start_hour;
    uint8_t start_min;
    uint8_t end_hour;
    uint8_t end_min;
    uint32_t checksum;
} salary_config_t;

typedef struct {
    lv_obj_t *settings_scr;
    lv_obj_t *earning_scr;
    lv_obj_t *rtc_label;
    lv_obj_t *salary_ta;
    lv_obj_t *interval_ta;
    lv_obj_t *kb;
    lv_obj_t *start_hour;
    lv_obj_t *start_min;
    lv_obj_t *end_hour;
    lv_obj_t *end_min;
    lv_obj_t *rtc_year;
    lv_obj_t *rtc_mon;
    lv_obj_t *rtc_day;
    lv_obj_t *rtc_hour;
    lv_obj_t *rtc_min;
    lv_obj_t *amount_label;
    lv_obj_t *coin[SALARY_COIN_COUNT];
    lv_timer_t *earning_timer;
    lv_timer_t *first_sound_timer;
    lv_timer_t *exit_timer;
    salary_config_t cfg;
    uint32_t last_sound_level;
    uint32_t last_exit_tick;
    uint32_t exit_press_tick;
    lv_point_t exit_press_point;
    bool exit_press_candidate;
    bool audio_ready;
    bool audio_warmed;
    bool page_active;
    bool page_exiting;
    bool page_del;
    volatile bool audio_playing;
} salary_ui_t;

extern void lvgl_init(void);
extern uint32_t liot_get_free_heap_size(void) __attribute__((weak));
extern uint32_t liot_get_total_heap_size(void) __attribute__((weak));
extern const uint8_t g_coin_burst_wav[];
extern const unsigned int g_coin_burst_wav_len;
extern const uint8_t g_salary_coin_mp3_start[];
extern const uint8_t g_salary_coin_mp3_end[];

static salary_ui_t g_salary;

static void salary_diag_log(const char *stage)
{
    uint32_t free_heap = liot_get_free_heap_size ? liot_get_free_heap_size() : 0;
    uint32_t total_heap = liot_get_total_heap_size ? liot_get_total_heap_size() : 0;
    liot_trace("[salary] %s free_heap=%lu total_heap=%lu settings=%p earning=%p active=%d audio=%d/%d",
               stage,
               (unsigned long)free_heap,
               (unsigned long)total_heap,
               g_salary.settings_scr,
               g_salary.earning_scr,
               g_salary.page_active ? 1 : 0,
               g_salary.audio_ready ? 1 : 0,
               g_salary.audio_warmed ? 1 : 0);
}

static const char *hours_opts(void) { return "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23"; }
static const char *mins_opts(void) { return "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59"; }
static const char *year_opts(void) { return "2024\n2025\n2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033\n2034\n2035"; }
static const char *mon_opts(void) { return "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12"; }
static const char *day_opts(void) { return "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31"; }

static bool is_leap_year(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    if (month < 1 || month > 12) return 31;
    return days[month - 1];
}

static void rtc_add_hours(liot_rtc_time_s *tm, int hours)
{
    tm->tm_hour += hours;
    while (tm->tm_hour >= 24) {
        tm->tm_hour -= 24;
        tm->tm_mday++;
        if (tm->tm_mday > days_in_month(tm->tm_year, tm->tm_mon)) {
            tm->tm_mday = 1;
            tm->tm_mon++;
            if (tm->tm_mon > 12) {
                tm->tm_mon = 1;
                tm->tm_year++;
            }
        }
    }
    while (tm->tm_hour < 0) {
        tm->tm_hour += 24;
        tm->tm_mday--;
        if (tm->tm_mday < 1) {
            tm->tm_mon--;
            if (tm->tm_mon < 1) {
                tm->tm_mon = 12;
                tm->tm_year--;
            }
            tm->tm_mday = days_in_month(tm->tm_year, tm->tm_mon);
        }
    }
}

static bool rtc_get_local_time(liot_rtc_time_s *tm)
{
    if (liot_rtc_get_time(tm) != LIOT_RTC_SUCCESS) return false;
    rtc_add_hours(tm, SALARY_TIMEZONE_HOURS);
    return true;
}

static void rtc_local_to_device_time(liot_rtc_time_s *tm)
{
    rtc_add_hours(tm, -SALARY_TIMEZONE_HOURS);
}

static uint32_t checksum(const salary_config_t *cfg)
{
    return cfg->magic ^ cfg->monthly_salary ^ cfg->sound_interval ^
           ((uint32_t)cfg->start_hour << 24) ^ ((uint32_t)cfg->start_min << 16) ^
           ((uint32_t)cfg->end_hour << 8) ^ cfg->end_min;
}

static void load_config(void)
{
    int ret = liot_nvm_fread(SALARY_NVM_FILE, &g_salary.cfg, sizeof(g_salary.cfg), 1);
    if (ret <= 0 || g_salary.cfg.magic != SALARY_NVM_MAGIC ||
        g_salary.cfg.checksum != checksum(&g_salary.cfg) || g_salary.cfg.sound_interval == 0) {
        memset(&g_salary.cfg, 0, sizeof(g_salary.cfg));
        g_salary.cfg.magic = SALARY_NVM_MAGIC;
        g_salary.cfg.monthly_salary = 10000;
        g_salary.cfg.sound_interval = 1;
        g_salary.cfg.start_hour = 9;
        g_salary.cfg.end_hour = 18;
        g_salary.cfg.checksum = checksum(&g_salary.cfg);
    }
}


static void save_config(void)
{
    if (g_salary.cfg.sound_interval == 0) g_salary.cfg.sound_interval = 1;
    g_salary.cfg.magic = SALARY_NVM_MAGIC;
    g_salary.cfg.checksum = checksum(&g_salary.cfg);
    liot_nvm_fwrite(SALARY_NVM_FILE, &g_salary.cfg, sizeof(g_salary.cfg), 1);
}

static int ta_int(lv_obj_t *ta, int fallback)
{
    const char *s = lv_textarea_get_text(ta);
    int value = 0;
    if (s == NULL || s[0] == 0) return fallback;
    while (*s) {
        if (*s >= '0' && *s <= '9') value = value * 10 + *s - '0';
        s++;
    }
    return value > 0 ? value : fallback;
}

static void ta_set_int(lv_obj_t *ta, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    lv_textarea_set_text(ta, buf);
}

static void salary_delete_timer(lv_timer_t **timer)
{
    if (*timer) {
        lv_timer_del(*timer);
        *timer = NULL;
    }
}

static void salary_cleanup_inactive_scr(lv_obj_t **scr)
{
    if (*scr && *scr != lv_scr_act()) {
        salary_diag_log("cleanup inactive screen");
        lv_obj_del_async(*scr);
    }
    *scr = NULL;
}

static uint32_t earned_cent(const liot_rtc_time_s *tm)
{
    int start = g_salary.cfg.start_hour * 3600 + g_salary.cfg.start_min * 60;
    int end = g_salary.cfg.end_hour * 3600 + g_salary.cfg.end_min * 60;
    int now = tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    int elapsed;
    uint64_t day_cent;
    if (end <= start) return 0;
    if (now <= start) elapsed = 0;
    else if (now >= end) elapsed = end - start;
    else elapsed = now - start;
    day_cent = ((uint64_t)g_salary.cfg.monthly_salary * 100u) / SALARY_WORK_DAYS;
    return (uint32_t)(day_cent * (uint32_t)elapsed / (uint32_t)(end - start));
}

static bool sync_rtc_rollers(void);
static void create_earning_page(void);

static void rtc_label_timer(lv_timer_t *timer)
{
    liot_rtc_time_s tm = {0};
    (void)timer;
    if (g_salary.rtc_label && rtc_get_local_time(&tm)) {
        lv_label_set_text_fmt(g_salary.rtc_label, "%04d-%02d-%02d %02d:%02d:%02d",
                              tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
}

static bool sync_rtc_rollers(void)
{
    liot_rtc_time_s tm = {0};
    if (!g_salary.rtc_year || !rtc_get_local_time(&tm)) return false;
    lv_roller_set_selected(g_salary.rtc_year, tm.tm_year >= 2024 ? tm.tm_year - 2024 : 0, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.rtc_mon, tm.tm_mon > 0 ? tm.tm_mon - 1 : 0, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.rtc_day, tm.tm_mday > 0 ? tm.tm_mday - 1 : 0, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.rtc_hour, tm.tm_hour, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.rtc_min, tm.tm_min, LV_ANIM_OFF);
    return true;
}

static void salary_audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;
    switch (event) {
    case L_AUD_EVT_START:
        liot_trace("salary L_AUD_EVT_START");
        break;
    case L_AUD_EVT_PAUSE:
        liot_trace("salary L_AUD_EVT_PAUSE");
        break;
    case L_AUD_EVT_FINISH:
        liot_trace("salary L_AUD_EVT_FINISH");
        break;
    case L_AUD_EVT_CLOSE:
        liot_trace("salary L_AUD_EVT_CLOSE");
        break;
    case L_AUD_EVT_RESUME:
        liot_trace("salary L_AUD_EVT_RESUME");
        break;
    default:
        break;
    }
}

static void audio_init_once(void)
{
    if (g_salary.audio_ready) return;
    if (demo_mp3_audio_is_ready()) {
        g_salary.audio_ready = true;
        liot_trace("salary reuse shared 44.1k audio");
        return;
    }

    salary_diag_log("audio init begin");
    liot_rtos_task_sleep_ms(2000);
    liot_trace("salary open audio power");
    Liot_AonPowerCtl(TRUE);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    Liot_AudHwConfig_t cfg = {
        .i2cNum = 0,
        .i2sNum = 0,
        .paGpioNum = -1,
        .codecType = L_AUD_ES8375,
        .channel = L_AUD_MONO_RIGHT,
        .role = L_AUD_ROLE_SLAVE,
        .mode = L_AUD_MODE_I2S,
        .frameSize = L_AUD_FRAMESIZE_16_16,
        .samples = L_AUD_16K_SAMPLES,
        .callback = salary_audio_callback,
    };
    liot_trace("salary AudioInit");
    Liot_AudErr_e ret = Liot_AudioInit(&cfg);
    liot_trace("salary AudioInit ret=%d", ret);
    if (ret == L_AUD_ERR_SUCCESS) {
        liot_trace("salary Audio Set Volume");
        Liot_AudioSetVolume(50);
        liot_trace("salary Audio Set Codec Volume");
        Liot_AudioSetCodecVolume(50);
        liot_trace("salary Audio Set Mic Volume");
        Liot_AudioSetMicVolume(8, 200);
        g_salary.audio_ready = true;
    }
    salary_diag_log("audio init end");
}

static void coin_sound_task(void *argv)
{
    Liot_AudErr_e stop_ret;
    Liot_AudErr_e play_ret;

    (void)argv;
    if (!g_salary.page_active) {
        g_salary.audio_playing = false;
        liot_rtos_task_delete(NULL);
        return;
    }
    audio_init_once();
    if (!g_salary.audio_ready) {
        liot_trace("salary coin audio not ready");
        g_salary.audio_playing = false;
        liot_rtos_task_delete(NULL);
        return;
    }

    stop_ret = Liot_AudioStop();
    liot_trace("salary coin AudioStop ret=%d", (int)stop_ret);
    liot_rtos_task_sleep_ms(100);
    {
        uint32_t mp3_len = (uint32_t)(g_salary_coin_mp3_end - g_salary_coin_mp3_start);
        play_ret = Liot_AudioPlayMp3((uint8_t *)g_salary_coin_mp3_start, (int)mp3_len);
        liot_trace("salary coin AudioPlayMp3 ret=%d len=%u", (int)play_ret,
                   (unsigned int)mp3_len);
    }
    if (play_ret == L_AUD_ERR_SUCCESS) {
        Liot_AudErr_e pause_ret;
        Liot_AudErr_e resume_ret;
        Liot_AudErr_e wait_ret;
        liot_rtos_task_sleep_ms(500);
        pause_ret = Liot_AudioPlayPause();
        liot_rtos_task_sleep_ms(80);
        resume_ret = Liot_AudioPlayResume();
        liot_trace("salary coin kick pause=%d resume=%d", (int)pause_ret, (int)resume_ret);
        wait_ret = Liot_AudioWaitPlayFinish(5000);
        liot_trace("salary coin wait ret=%d", (int)wait_ret);
        (void)Liot_AudioStop();
        g_salary.audio_warmed = (wait_ret == L_AUD_ERR_SUCCESS);
    }
    g_salary.audio_playing = false;
    liot_rtos_task_delete(NULL);
}

static void play_coin_sound(void)
{
    liot_task_t task = NULL;
    LiotOSStatus_t ret;
    if (g_salary.audio_playing) return;
    g_salary.audio_playing = true;
    ret = liot_rtos_task_create(&task, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                                "coin_sound", coin_sound_task, NULL);
    liot_trace("salary coin task create ret=%d handle=%p", (int)ret, task);
    if (ret != LIOT_OSI_SUCCESS) {
        g_salary.audio_playing = false;
    }
}

static void first_sound_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    if (g_salary.first_sound_timer == timer) {
        g_salary.first_sound_timer = NULL;
    }
    play_coin_sound();
}

static void coin_done(lv_anim_t *anim)
{
    lv_obj_add_flag((lv_obj_t *)anim->var, LV_OBJ_FLAG_HIDDEN);
}

static void coin_anim(int index, bool fast)
{
    lv_anim_t anim;
    lv_obj_t *coin = g_salary.coin[index % SALARY_COIN_COUNT];
    if (!coin) return;
    lv_obj_clear_flag(coin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(coin, 46 + ((index * 43) % 245), -36);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, coin);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&anim, -36, 324);
    lv_anim_set_time(&anim, fast ? 1200 : 2800 + ((index * 97) % 1200));
    lv_anim_set_ready_cb(&anim, coin_done);
    lv_anim_start(&anim);
}

static void coin_burst_anim(uint32_t seed)
{
    for (int i = 0; i < SALARY_BURST_COIN_COUNT; i++) {
        lv_anim_t anim;
        int index = (int)((seed + (uint32_t)i) % SALARY_COIN_COUNT);
        lv_obj_t *coin = g_salary.coin[index];
        int x = 26 + (int)(((seed * 31u) + ((uint32_t)i * 67u)) % 286u);
        int delay = (int)(((seed * 23u) + ((uint32_t)i * 137u)) % 700u);
        int duration = 900 + (int)(((seed * 19u) + ((uint32_t)i * 173u)) % 1500u);
        if (!coin) continue;
        lv_obj_clear_flag(coin, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(coin, x, -42);
        lv_anim_del(coin, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, coin);
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&anim, -42, 324);
        lv_anim_set_delay(&anim, delay);
        lv_anim_set_time(&anim, duration);
        lv_anim_set_ready_cb(&anim, coin_done);
        lv_anim_start(&anim);
    }
}

static void earning_timer(lv_timer_t *timer)
{
    liot_rtc_time_s tm = {0};
    uint32_t cent;
    uint32_t level;
    (void)timer;
    if (!rtc_get_local_time(&tm)) return;
    cent = earned_cent(&tm);
    lv_label_set_text_fmt(g_salary.amount_label, "%u.%02u",
                          (unsigned int)(cent / 100u), (unsigned int)(cent % 100u));
    level = cent / (g_salary.cfg.sound_interval * 100u);
    if (level > g_salary.last_sound_level) {
        g_salary.last_sound_level = level;
        play_coin_sound();
        coin_burst_anim(level + (uint32_t)tm.tm_sec);
    }
    coin_anim(tm.tm_sec % SALARY_COIN_COUNT, false);
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    return obj;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, lv_event_cb_t cb, void *data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, data);
    lv_obj_t *txt = lv_label_create(btn);
    lv_label_set_text(txt, text);
    lv_obj_center(txt);
    return btn;
}

static lv_obj_t *textarea(lv_obj_t *parent, int value, int x, int y, int w)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, w, 38);
    lv_obj_set_pos(ta, x, y);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_accepted_chars(ta, "0123456789");
    lv_textarea_set_max_length(ta, 8);
    ta_set_int(ta, value);
    lv_obj_add_flag(ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    return ta;
}

static lv_obj_t *roller(lv_obj_t *parent, const char *opts, int x, int y, int w)
{
    lv_obj_t *r = lv_roller_create(parent);
    lv_roller_set_options(r, opts, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(r, w, 48);
    lv_obj_set_pos(r, x, y);
    lv_roller_set_visible_row_count(r, 2);
    lv_obj_add_flag(r, LV_OBJ_FLAG_EVENT_BUBBLE);
    return r;
}

static void adjust_cb(lv_event_t *event)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(event);
    lv_obj_t *target = (delta == 1000 || delta == -1000) ? g_salary.salary_ta : g_salary.interval_ta;
    int value = ta_int(target, target == g_salary.salary_ta ? 10000 : 1) + delta;
    if (target == g_salary.salary_ta && value < 0) value = 0;
    if (target == g_salary.interval_ta && value < 1) value = 1;
    ta_set_int(target, value);
}

static void focus_cb(lv_event_t *event)
{
    lv_obj_clear_flag(g_salary.kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_salary.kb, (lv_obj_t *)lv_event_get_target(event));
}

static void kb_close_cb(lv_event_t *event)
{
    (void)event;
    lv_obj_add_flag(g_salary.kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_salary.kb, NULL);
}

static void salary_calculator_page_exit(void)
{
    lv_obj_t *active_scr = lv_scr_act();

    if (!g_salary.page_active && !g_salary.settings_scr && !g_salary.earning_scr) {
        liot_trace("[salary] exit ignored inactive active=%p settings=%p earning=%p",
                   active_scr, g_salary.settings_scr, g_salary.earning_scr);
        return;
    }

    liot_trace("[salary] exit begin active=%p settings=%p earning=%p salary_menu=%p page_del=%d",
               active_scr,
               g_salary.settings_scr,
               g_salary.earning_scr,
               guider_ui.salary,
               g_salary.page_del ? 1 : 0);

    g_salary.page_active = false;
    g_salary.last_exit_tick = lv_tick_get();
    salary_delete_timer(&g_salary.earning_timer);
    salary_delete_timer(&g_salary.first_sound_timer);
    salary_delete_timer(&g_salary.exit_timer);

    ui_load_scr_animation(&guider_ui,
                          &guider_ui.salary,
                          guider_ui.salary_del,
                          &g_salary.page_del,
                          setup_scr_salary,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          0,
                          false,
                          false);

    if (g_salary.settings_scr && g_salary.settings_scr != active_scr) {
        salary_cleanup_inactive_scr(&g_salary.settings_scr);
    }
    if (g_salary.earning_scr && g_salary.earning_scr != active_scr) {
        salary_cleanup_inactive_scr(&g_salary.earning_scr);
    }

    if (active_scr && active_scr != guider_ui.salary) {
        liot_trace("[salary] schedule old active screen delete %p", active_scr);
        lv_obj_del_delayed(active_scr, 800);
    }

    g_salary.settings_scr = NULL;
    g_salary.earning_scr = NULL;
    g_salary.page_del = true;
    liot_trace("[salary] exit end menu=%p", guider_ui.salary);
}

static void salary_calculator_page_exit_timer_cb(lv_timer_t *timer)
{
    if (g_salary.exit_timer == timer) {
        g_salary.exit_timer = NULL;
    }
    liot_trace("[salary] exit timer fired active=%p", lv_scr_act());

    lv_timer_del(timer);

    salary_calculator_page_exit();
}

static void salary_calculator_page_exit_request(void)
{
    lv_indev_t *indev;

    if (g_salary.page_exiting) {


        liot_trace("[salary] exit request ignored, exiting");


        return;


    }


    liot_trace("[salary] exit request active=%p settings=%p earning=%p", lv_scr_act(), g_salary.settings_scr, g_salary.earning_scr);


    g_salary.page_exiting = true;

    indev = lv_indev_get_act();
    if (indev) {
        lv_indev_wait_release(indev);
    }

    if (!g_salary.exit_timer) {
        g_salary.exit_timer = lv_timer_create(salary_calculator_page_exit_timer_cb, 300, NULL);
        lv_timer_set_repeat_count(g_salary.exit_timer, 1);
    }
}

static void salary_page_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev;
    lv_point_t point;
    int32_t dx;
    int32_t dy;

    if (code == LV_EVENT_PRESSED) {
        g_salary.exit_press_candidate =
            (lv_event_get_target(event) == lv_event_get_current_target(event));
        if (!g_salary.exit_press_candidate) return;
        indev = lv_indev_get_act();
        if (indev == NULL) {
            g_salary.exit_press_candidate = false;
            return;
        }
        lv_indev_get_point(indev, &g_salary.exit_press_point);
        g_salary.exit_press_tick = lv_tick_get();
        return;
    }

    if (code == LV_EVENT_PRESSING && g_salary.exit_press_candidate) {
        indev = lv_indev_get_act();
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        dx = point.x - g_salary.exit_press_point.x;
        dy = point.y - g_salary.exit_press_point.y;
        if (dx > SALARY_EXIT_MOVE_LIMIT || dx < -SALARY_EXIT_MOVE_LIMIT ||
            dy > SALARY_EXIT_MOVE_LIMIT || dy < -SALARY_EXIT_MOVE_LIMIT) {
            g_salary.exit_press_candidate = false;
            return;
        }
        if (lv_tick_elaps(g_salary.exit_press_tick) >= SALARY_EXIT_HOLD_MS) {
            g_salary.exit_press_candidate = false;
            lv_event_stop_bubbling(event);
            lv_event_stop_processing(event);
            salary_calculator_page_exit_request();
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_salary.exit_press_candidate = false;
    }
}

static void back_cb(lv_event_t *event)
{
    (void)event;
    if (g_salary.page_exiting) return;
    salary_diag_log("back settings begin");
    salary_delete_timer(&g_salary.earning_timer);
    lv_scr_load_anim(g_salary.settings_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
    sync_rtc_rollers();
    rtc_label_timer(NULL);
    salary_diag_log("back settings end");
}

static void confirm_cb(lv_event_t *event)
{
    liot_rtc_time_s tm = {0};
    (void)event;
    if (g_salary.page_exiting) return;
    salary_diag_log("confirm begin");
    g_salary.cfg.monthly_salary = (uint32_t)ta_int(g_salary.salary_ta, 10000);
    g_salary.cfg.sound_interval = (uint32_t)ta_int(g_salary.interval_ta, 1);
    g_salary.cfg.start_hour = (uint8_t)lv_roller_get_selected(g_salary.start_hour);
    g_salary.cfg.start_min = (uint8_t)lv_roller_get_selected(g_salary.start_min);
    g_salary.cfg.end_hour = (uint8_t)lv_roller_get_selected(g_salary.end_hour);
    g_salary.cfg.end_min = (uint8_t)lv_roller_get_selected(g_salary.end_min);
    save_config();

    if (!g_salary.earning_scr) {
        salary_diag_log("create earning before");
        create_earning_page();
        salary_diag_log("create earning after");
    }

    tm.tm_year = 2024 + (int)lv_roller_get_selected(g_salary.rtc_year);
    tm.tm_mon = 1 + (int)lv_roller_get_selected(g_salary.rtc_mon);
    tm.tm_mday = 1 + (int)lv_roller_get_selected(g_salary.rtc_day);
    tm.tm_hour = (int)lv_roller_get_selected(g_salary.rtc_hour);
    tm.tm_min = (int)lv_roller_get_selected(g_salary.rtc_min);
    tm.tm_sec = 0;
    g_salary.last_sound_level = earned_cent(&tm) / (g_salary.cfg.sound_interval * 100u);
    rtc_local_to_device_time(&tm);
    liot_rtc_set_time(&tm);

    lv_scr_load_anim(g_salary.earning_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    earning_timer(NULL);
    g_salary.first_sound_timer = lv_timer_create(first_sound_timer_cb, 1500, NULL);
    g_salary.earning_timer = lv_timer_create(earning_timer, 1000, NULL);
    salary_diag_log("confirm end");
}

static void create_settings_page(void)
{
    salary_diag_log("create settings begin");
    g_salary.settings_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_salary.settings_scr, lv_color_hex(0x182235), 0);
    lv_obj_set_style_bg_opa(g_salary.settings_scr, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(g_salary.settings_scr, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_salary.settings_scr, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(g_salary.settings_scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_salary.settings_scr, salary_page_event_cb, LV_EVENT_ALL, NULL);
    label(g_salary.settings_scr, "Salary Calculator", 76, 24);
    g_salary.rtc_label = label(g_salary.settings_scr, "RTC --", 52, 54);

    label(g_salary.settings_scr, "Salary", 42, 104);
    g_salary.salary_ta = textarea(g_salary.settings_scr, g_salary.cfg.monthly_salary, 112, 96, 96);
    button(g_salary.settings_scr, "-1k", 216, 96, 48, 36, adjust_cb, (void *)(intptr_t)-1000);
    button(g_salary.settings_scr, "+1k", 270, 96, 48, 36, adjust_cb, (void *)(intptr_t)1000);

    label(g_salary.settings_scr, "Sound", 42, 158);
    g_salary.interval_ta = textarea(g_salary.settings_scr, g_salary.cfg.sound_interval, 112, 150, 96);
    button(g_salary.settings_scr, "-1", 216, 150, 48, 36, adjust_cb, (void *)(intptr_t)-1);
    button(g_salary.settings_scr, "+1", 270, 150, 48, 36, adjust_cb, (void *)(intptr_t)1);

    label(g_salary.settings_scr, "Work", 42, 232);
    g_salary.start_hour = roller(g_salary.settings_scr, hours_opts(), 96, 210, 42);
    g_salary.start_min = roller(g_salary.settings_scr, mins_opts(), 142, 210, 42);
    label(g_salary.settings_scr, "-", 190, 232);
    g_salary.end_hour = roller(g_salary.settings_scr, hours_opts(), 208, 210, 42);
    g_salary.end_min = roller(g_salary.settings_scr, mins_opts(), 254, 210, 42);
    lv_roller_set_selected(g_salary.start_hour, g_salary.cfg.start_hour, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.start_min, g_salary.cfg.start_min, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.end_hour, g_salary.cfg.end_hour, LV_ANIM_OFF);
    lv_roller_set_selected(g_salary.end_min, g_salary.cfg.end_min, LV_ANIM_OFF);

    label(g_salary.settings_scr, "RTC", 42, 316);
    g_salary.rtc_year = roller(g_salary.settings_scr, year_opts(), 104, 294, 58);
    g_salary.rtc_mon = roller(g_salary.settings_scr, mon_opts(), 170, 294, 38);
    g_salary.rtc_day = roller(g_salary.settings_scr, day_opts(), 216, 294, 38);
    g_salary.rtc_hour = roller(g_salary.settings_scr, hours_opts(), 104, 382, 38);
    g_salary.rtc_min = roller(g_salary.settings_scr, mins_opts(), 150, 382, 38);
    sync_rtc_rollers();

    button(g_salary.settings_scr, "Confirm", 110, 468, 140, 36, confirm_cb, NULL);
    g_salary.kb = lv_keyboard_create(g_salary.settings_scr);
    lv_keyboard_set_mode(g_salary.kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(g_salary.kb, 220, 100);
    lv_obj_align(g_salary.kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_salary.kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_salary.salary_ta, focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_salary.interval_ta, focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_salary.kb, kb_close_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_salary.kb, kb_close_cb, LV_EVENT_CANCEL, NULL);
    rtc_label_timer(NULL);
    salary_diag_log("create settings end");
}

static void create_earning_page(void)
{
    salary_diag_log("create earning begin");
    g_salary.earning_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_salary.earning_scr, lv_color_hex(0x103018), 0);
    lv_obj_set_style_bg_opa(g_salary.earning_scr, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_salary.earning_scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_salary.earning_scr, salary_page_event_cb, LV_EVENT_ALL, NULL);
    button(g_salary.earning_scr, "< Settings", 44, 28, 108, 34, back_cb, NULL);
    label(g_salary.earning_scr, "Earned Today", 110, 82);
    g_salary.amount_label = lv_label_create(g_salary.earning_scr);
    lv_obj_set_style_text_color(g_salary.amount_label, lv_color_hex(0xFFD54A), 0);
    lv_obj_set_style_text_font(g_salary.amount_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(g_salary.amount_label, "0.00");
    lv_obj_align(g_salary.amount_label, LV_ALIGN_CENTER, 0, -28);

    for (int i = 0; i < SALARY_COIN_COUNT; i++) {
        g_salary.coin[i] = lv_obj_create(g_salary.earning_scr);
        lv_obj_remove_style_all(g_salary.coin[i]);
        lv_obj_add_flag(g_salary.coin[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(g_salary.coin[i], 34, 34);
        lv_obj_set_style_radius(g_salary.coin[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(g_salary.coin[i], lv_color_hex(0xFFD54A), 0);
        lv_obj_set_style_bg_opa(g_salary.coin[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_salary.coin[i], 4, 0);
        lv_obj_set_style_border_color(g_salary.coin[i], lv_color_hex(0xC98500), 0);

        lv_obj_t *coin_label = lv_label_create(g_salary.coin[i]);
        lv_label_set_text(coin_label, "$");
        lv_obj_set_style_text_color(coin_label, lv_color_hex(0x7A4A00), 0);
        lv_obj_set_style_text_font(coin_label, &lv_font_montserrat_16, 0);
        lv_obj_center(coin_label);

        lv_obj_add_flag(g_salary.coin[i], LV_OBJ_FLAG_HIDDEN);
    }
    salary_diag_log("create earning end");
}

void salary_calculator_page_enter(void)
{
    if (g_salary.page_active) {
        salary_diag_log("enter ignored active");
        return;
    }
    if (g_salary.last_exit_tick != 0 && lv_tick_elaps(g_salary.last_exit_tick) < SALARY_REENTER_BLOCK_MS) {
        salary_diag_log("enter ignored guard");
        return;
    }

    salary_diag_log("enter begin");
    g_salary.page_exiting = false;
    salary_cleanup_inactive_scr(&g_salary.settings_scr);
    salary_cleanup_inactive_scr(&g_salary.earning_scr);
    salary_delete_timer(&g_salary.earning_timer);
    salary_delete_timer(&g_salary.first_sound_timer);
    salary_delete_timer(&g_salary.exit_timer);

    g_salary.page_active = true;
    g_salary.page_del = false;
    load_config();
    create_settings_page();
    lv_scr_load_anim(g_salary.settings_scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    guider_ui.salary_del = true;
    salary_diag_log("enter end");
}

static void salary_calculator_page_enter_async_cb(void *arg)
{
    (void)arg;
    salary_calculator_page_enter();
}

void salary_calculator_page_enter_async(void)
{
    salary_diag_log("enter async post");
    lv_async_call(salary_calculator_page_enter_async_cb, NULL);
}

void liot_salary_calculator_demo_thread(void *argv)
{
    (void)argv;
    liot_trace("salary calculator demo start");
    lvgl_init();
    salary_calculator_page_enter();
    while (1) liot_rtos_task_sleep_s(10);
}
