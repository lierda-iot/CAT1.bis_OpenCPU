#include "app_menu.h"

#include "baji_photo/baji_photo_page.h"
#include "baji_photo/baji_photo_mqtt.h"
#include "liot_log.h"
#include "liot_os.h"
#include "lvgl.h"
#include "sc7a20h_page.h"

#if APP_SALARY_EN
#include "demo_salary_calculator.h"
#endif

#if APP_MP3_EN
#include "demo_mp3.h"
#endif

#define APP_MENU_TOUCH_MOVE_LIMIT        32
#define APP_MENU_LONG_PRESS_GUARD_MS     600u

#if APP_WATCHFACE_EN
#define APP_MENU_HAS_TIME 1
#else
#define APP_MENU_HAS_TIME 0
#endif

#if APP_BAJI_EN
#define APP_MENU_HAS_BAJI 1
#else
#define APP_MENU_HAS_BAJI 0
#endif

#if APP_ATTITUDE_EN
#define APP_MENU_HAS_ATTUITION 1
#else
#define APP_MENU_HAS_ATTUITION 0
#endif

#if APP_ATTFUN_EN
#define APP_MENU_HAS_ATTFUN 1
#else
#define APP_MENU_HAS_ATTFUN 0
#endif

#if APP_SALARY_EN
#define APP_MENU_HAS_SALARY 1
#else
#define APP_MENU_HAS_SALARY 0
#endif

#if APP_MAP_EN
#define APP_MENU_HAS_POSITIONING 1
#else
#define APP_MENU_HAS_POSITIONING 0
#endif

#if APP_MP3_EN
#define APP_MENU_HAS_MP3 1
#else
#define APP_MENU_HAS_MP3 0
#endif

typedef enum {
  APP_MENU_SCREEN_TIME = 0,
  APP_MENU_SCREEN_BAJI,
  APP_MENU_SCREEN_ATTUITION,
  APP_MENU_SCREEN_ATTFUN,
  APP_MENU_SCREEN_SALARY,
  APP_MENU_SCREEN_POSITIONING,
  APP_MENU_SCREEN_MP3,
  APP_MENU_SCREEN_COUNT
} app_menu_screen_id_t;

typedef enum {
  APP_MENU_STATE_IDLE = 0,
  APP_MENU_STATE_TRANSITIONING,
} app_menu_state_t;

static uint32_t g_app_menu_baji_long_press_tick;
static lv_point_t g_app_menu_press_point;
static bool g_app_menu_touch_moved;
static bool g_app_menu_long_press_consumed;
static app_menu_state_t g_app_menu_state = APP_MENU_STATE_IDLE;

static void app_menu_load(lv_obj_t **new_scr, bool new_scr_del,
                          bool *old_scr_del, ui_setup_scr_t setup_scr,
                          lv_scr_load_anim_t anim_type);

static void app_menu_screen_loaded_event(lv_event_t *event);
static void app_menu_consume_gesture(lv_event_t *event);

static const app_menu_screen_id_t g_app_menu_enabled_order[] = {
#if APP_MENU_HAS_TIME
    APP_MENU_SCREEN_TIME,
#endif
#if APP_MENU_HAS_BAJI
    APP_MENU_SCREEN_BAJI,
#endif
#if APP_MENU_HAS_ATTUITION
    APP_MENU_SCREEN_ATTUITION,
#endif
#if APP_MENU_HAS_ATTFUN
    APP_MENU_SCREEN_ATTFUN,
#endif
#if APP_MENU_HAS_SALARY
    APP_MENU_SCREEN_SALARY,
#endif
#if APP_MENU_HAS_POSITIONING
    APP_MENU_SCREEN_POSITIONING,
#endif
#if APP_MENU_HAS_MP3
    APP_MENU_SCREEN_MP3,
#endif
};

static unsigned int app_menu_enabled_count(void) {
  return (unsigned int)(sizeof(g_app_menu_enabled_order) /
                        sizeof(g_app_menu_enabled_order[0]));
}

static int app_menu_find_index(app_menu_screen_id_t screen) {
  unsigned int i;
  for (i = 0; i < app_menu_enabled_count(); ++i) {
    if (g_app_menu_enabled_order[i] == screen) return (int)i;
  }
  return -1;
}

static app_menu_screen_id_t app_menu_adjacent(app_menu_screen_id_t current,
                                              bool go_right) {
  unsigned int count = app_menu_enabled_count();
  int index = app_menu_find_index(current);
  if (count == 0u || index < 0) return current;
  if (go_right) {
    index = (index + 1) % (int)count;
  } else {
    index = (index + (int)count - 1) % (int)count;
  }
  return g_app_menu_enabled_order[index];
}

static lv_obj_t **app_menu_screen_ptr(lv_ui *ui, app_menu_screen_id_t screen) {
  switch (screen) {
  case APP_MENU_SCREEN_TIME:
    return &ui->time;
  case APP_MENU_SCREEN_BAJI:
    return &ui->baji;
  case APP_MENU_SCREEN_ATTUITION:
    return &ui->attuition;
  case APP_MENU_SCREEN_ATTFUN:
    return &ui->attfun;
  case APP_MENU_SCREEN_SALARY:
    return &ui->salary;
  case APP_MENU_SCREEN_POSITIONING:
    return &ui->positioning;
  case APP_MENU_SCREEN_MP3:
    return &ui->mp3;
  default:
    return NULL;
  }
}

static bool app_menu_screen_del_flag(lv_ui *ui, app_menu_screen_id_t screen) {
  switch (screen) {
  case APP_MENU_SCREEN_TIME:
    return ui->time_del;
  case APP_MENU_SCREEN_BAJI:
    return ui->baji_del;
  case APP_MENU_SCREEN_ATTUITION:
    return ui->attuition_del;
  case APP_MENU_SCREEN_ATTFUN:
    return ui->attfun_del;
  case APP_MENU_SCREEN_SALARY:
    return ui->salary_del;
  case APP_MENU_SCREEN_POSITIONING:
    return ui->positioning_del;
  case APP_MENU_SCREEN_MP3:
    return ui->mp3_del;
  default:
    return true;
  }
}

static bool *app_menu_old_del_flag(lv_ui *ui, app_menu_screen_id_t screen) {
  switch (screen) {
  case APP_MENU_SCREEN_TIME:
    return &ui->time_del;
  case APP_MENU_SCREEN_BAJI:
    return &ui->baji_del;
  case APP_MENU_SCREEN_ATTUITION:
    return &ui->attuition_del;
  case APP_MENU_SCREEN_ATTFUN:
    return &ui->attfun_del;
  case APP_MENU_SCREEN_SALARY:
    return &ui->salary_del;
  case APP_MENU_SCREEN_POSITIONING:
    return &ui->positioning_del;
  case APP_MENU_SCREEN_MP3:
    return &ui->mp3_del;
  default:
    return NULL;
  }
}

static ui_setup_scr_t app_menu_setup_fn(app_menu_screen_id_t screen) {
  switch (screen) {
#if APP_WATCHFACE_EN
  case APP_MENU_SCREEN_TIME:
    return setup_scr_time;
#endif
#if APP_BAJI_EN
  case APP_MENU_SCREEN_BAJI:
    return setup_scr_baji;
#endif
#if APP_ATTITUDE_EN
  case APP_MENU_SCREEN_ATTUITION:
    return setup_scr_attuition;
#endif
#if APP_ATTFUN_EN
  case APP_MENU_SCREEN_ATTFUN:
    return setup_scr_attfun;
#endif
#if APP_SALARY_EN
  case APP_MENU_SCREEN_SALARY:
    return setup_scr_salary;
#endif
#if APP_MAP_EN
  case APP_MENU_SCREEN_POSITIONING:
    return setup_scr_positioning;
#endif
#if APP_MP3_EN
  case APP_MENU_SCREEN_MP3:
    return setup_scr_mp3;
#endif
  default:
    return NULL;
  }
}

static void app_menu_load_adjacent(app_menu_screen_id_t current,
                                   app_menu_screen_id_t fallback,
                                   bool go_right) {
  app_menu_screen_id_t next = app_menu_adjacent(current, go_right);
  lv_obj_t **new_scr = app_menu_screen_ptr(&guider_ui, next);
  bool *old_del = app_menu_old_del_flag(&guider_ui, current);
  ui_setup_scr_t setup_scr = app_menu_setup_fn(next);
  lv_scr_load_anim_t anim =
      go_right ? LV_SCR_LOAD_ANIM_MOVE_RIGHT : LV_SCR_LOAD_ANIM_MOVE_LEFT;

  if (new_scr == NULL || old_del == NULL || setup_scr == NULL) {
    if (fallback != current) {
      next = fallback;
      new_scr = app_menu_screen_ptr(&guider_ui, next);
      setup_scr = app_menu_setup_fn(next);
    }
  }
  if (new_scr == NULL || old_del == NULL || setup_scr == NULL) return;
  if (next == current) return;

  app_menu_load(new_scr, app_menu_screen_del_flag(&guider_ui, next), old_del,
                setup_scr, anim);
}

static void app_menu_baji_unbind_confirm_event(lv_event_t *event) {
  lv_obj_t *msgbox = lv_event_get_current_target(event);
  int ret;
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
  if (lv_msgbox_get_active_btn(msgbox) == 1) {
    ret = baji_photo_mqtt_request_unbind();
    liot_trace("[menu] baji unbind request ret=%d", ret);
  }
  lv_msgbox_close(msgbox);
}

static void app_menu_baji_unbind_event(lv_event_t *event) {
  static const char *buttons[] = {"Cancel", "Unbind", ""};
  lv_obj_t *msgbox;
  lv_event_stop_bubbling(event);
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  msgbox = lv_msgbox_create(NULL, "Unbind device?",
                            "This removes the current account binding.",
                            buttons, true);
  if (msgbox == NULL) return;
  lv_obj_center(msgbox);
  lv_obj_add_event_cb(msgbox, app_menu_baji_unbind_confirm_event,
                      LV_EVENT_VALUE_CHANGED, NULL);
}
static void app_menu_baji_add_unbind_button(lv_ui *ui) {
  lv_obj_t *btn;
  lv_obj_t *label;
  if (ui == NULL || ui->baji == NULL) return;
  btn = lv_btn_create(ui->baji);
  lv_obj_set_size(btn, 88, 38);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 24);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0xA53232), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_90, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(btn, app_menu_baji_unbind_event, LV_EVENT_ALL, NULL);
  label = lv_label_create(btn);
  lv_label_set_text(label, "Unbind");
  lv_obj_center(label);
  if (baji_photo_mqtt_get_bind_status() != BAJI_PHOTO_BIND_BOUND)
    lv_obj_add_state(btn, LV_STATE_DISABLED);
}
static void app_menu_track_touch(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_indev_get_act();
  lv_point_t point;
  if (code == LV_EVENT_PRESSED) {
    g_app_menu_baji_long_press_tick = 0u;
    g_app_menu_long_press_consumed = false;
    g_app_menu_touch_moved = false;
    if (indev == NULL) return;
    lv_indev_get_point(indev, &g_app_menu_press_point);
  } else if (code == LV_EVENT_PRESSING && !g_app_menu_touch_moved &&
             indev != NULL) {
    lv_indev_get_point(indev, &point);
    if (LV_ABS(point.x - g_app_menu_press_point.x) >= APP_MENU_TOUCH_MOVE_LIMIT ||
        LV_ABS(point.y - g_app_menu_press_point.y) >= APP_MENU_TOUCH_MOVE_LIMIT) {
      g_app_menu_touch_moved = true;
      g_app_menu_baji_long_press_tick = 0u;
    }
  }
}

static bool app_menu_click_suppressed(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_app_menu_touch_moved) {
    g_app_menu_touch_moved = false;
    return true;
  }
  if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
      g_app_menu_long_press_consumed) {
    g_app_menu_long_press_consumed = false;
    return true;
  }
  return false;
}
#if APP_MAP_EN && defined(HWDEMO_GNSS_EN)
#include "demo_gnss.h"
#endif
#if APP_MAP_EN
#include "demo_location.h"
#endif

static void app_menu_load(lv_obj_t **new_scr, bool new_scr_del,
                          bool *old_scr_del, ui_setup_scr_t setup_scr,
                          lv_scr_load_anim_t anim_type) {
  lv_indev_t *indev = lv_indev_get_act();

  if (g_app_menu_state != APP_MENU_STATE_IDLE) return;

  if (indev != NULL) {
    lv_indev_wait_release(indev);
  }

  g_app_menu_state = APP_MENU_STATE_TRANSITIONING;
  ui_load_scr_animation(&guider_ui, new_scr, new_scr_del, old_scr_del,
                        setup_scr, anim_type, 200, 0, false, true);

  if (*new_scr != NULL && lv_obj_is_valid(*new_scr)) {
    lv_obj_add_event_cb(*new_scr, app_menu_screen_loaded_event,
                        LV_EVENT_SCREEN_LOADED, NULL);
  }
}

static void app_menu_screen_loaded_event(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
    g_app_menu_state = APP_MENU_STATE_IDLE;
  }
}

static void app_menu_consume_gesture(lv_event_t *event) {
  lv_event_stop_bubbling(event);
  lv_event_stop_processing(event);
}

static void app_menu_time_event(lv_event_t *event) {
  app_menu_track_touch(event);
  if (app_menu_click_suppressed(event)) return;
  if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
  switch (lv_indev_get_gesture_dir(lv_indev_get_act())) {
  case LV_DIR_RIGHT:
    app_menu_load_adjacent(APP_MENU_SCREEN_TIME, APP_MENU_SCREEN_BAJI, true);
    app_menu_consume_gesture(event);
    break;
  case LV_DIR_LEFT:
    app_menu_load_adjacent(APP_MENU_SCREEN_TIME, APP_MENU_SCREEN_MP3, false);
    app_menu_consume_gesture(event);
    break;
  default:
    break;
  }
}
static void app_menu_baji_event(lv_event_t *event) {
  app_menu_track_touch(event);
  if (app_menu_click_suppressed(event)) return;
  lv_event_code_t code = lv_event_get_code(event);

  switch (lv_event_get_code(event)) {
  case LV_EVENT_CLICKED:
    g_app_menu_baji_long_press_tick = 0u;
    if (g_app_menu_long_press_consumed) {
      g_app_menu_long_press_consumed = false;
      liot_trace("[menu] baji click ignored after long press");
      break;
    }
    liot_trace("[menu] baji click enter photo page");
    baji_photo_page_enter();
    break;
  case LV_EVENT_LONG_PRESSED: {
    lv_indev_t *indev = lv_indev_get_act();
    lv_obj_t *target = lv_event_get_target(event);

    if (g_app_menu_touch_moved ||
        target != lv_event_get_current_target(event)) {
      break;
    }
    g_app_menu_long_press_consumed = true;
    g_app_menu_baji_long_press_tick = lv_tick_get();
    if (indev != NULL) {
      lv_indev_reset(indev, target);
    } else {
      lv_indev_reset(NULL, target);
    }
  }
    lv_event_stop_bubbling(event);
    lv_event_stop_processing(event);
    liot_trace("[menu] baji long press sync");
    (void)baji_photo_request_sync();
    break;
  case LV_EVENT_GESTURE:
    g_app_menu_baji_long_press_tick = 0u;
    g_app_menu_long_press_consumed = false;
    switch (lv_indev_get_gesture_dir(lv_indev_get_act())) {
    case LV_DIR_RIGHT:
      app_menu_load_adjacent(APP_MENU_SCREEN_BAJI, APP_MENU_SCREEN_ATTUITION,
                             true);
      app_menu_consume_gesture(event);
      break;
    case LV_DIR_LEFT:
      app_menu_load_adjacent(APP_MENU_SCREEN_BAJI, APP_MENU_SCREEN_TIME, false);
      app_menu_consume_gesture(event);
      break;


    default:
      break;
    }
    break;
  default:
    if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) &&
        (g_app_menu_baji_long_press_tick != 0u) &&
        (lv_tick_elaps(g_app_menu_baji_long_press_tick) >=
         APP_MENU_LONG_PRESS_GUARD_MS)) {
      g_app_menu_baji_long_press_tick = 0u;
    }
    break;
  }
}

static void app_menu_attuition_event(lv_event_t *event) {
  app_menu_track_touch(event);
  if (app_menu_click_suppressed(event)) return;
  switch (lv_event_get_code(event)) {
  case LV_EVENT_CLICKED:
    liot_trace("[menu] attuition click enter sc7a20h");
    sc7a20h_page_enter_async();
    break;
  case LV_EVENT_GESTURE:
    switch (lv_indev_get_gesture_dir(lv_indev_get_act())) {
    case LV_DIR_RIGHT:
      app_menu_load_adjacent(APP_MENU_SCREEN_ATTUITION, APP_MENU_SCREEN_SALARY,
                             true);
      app_menu_consume_gesture(event);
      break;
    case LV_DIR_LEFT:
      app_menu_load_adjacent(APP_MENU_SCREEN_ATTUITION, APP_MENU_SCREEN_BAJI,
                             false);
      app_menu_consume_gesture(event);
      break;

    default:
      break;
    }
    break;
  default:
    break;
  }
}

static void app_menu_attfun_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_LONG_PRESSED)
    return;
  app_menu_load_adjacent(APP_MENU_SCREEN_ATTFUN, APP_MENU_SCREEN_ATTUITION,
                         false);
}

static void app_menu_salary_event(lv_event_t *event) {
  app_menu_track_touch(event);
  if (app_menu_click_suppressed(event)) return;
  switch (lv_event_get_code(event)) {
#if APP_SALARY_EN
  case LV_EVENT_CLICKED:
    liot_trace("[menu] salary click enter calculator");
    salary_calculator_page_enter_async();
    break;
#endif
  case LV_EVENT_GESTURE:
    switch (lv_indev_get_gesture_dir(lv_indev_get_act())) {
    case LV_DIR_RIGHT:
      app_menu_load_adjacent(APP_MENU_SCREEN_SALARY,
                             APP_MENU_SCREEN_POSITIONING, true);
      app_menu_consume_gesture(event);
      break;
    case LV_DIR_LEFT:
      app_menu_load_adjacent(APP_MENU_SCREEN_SALARY, APP_MENU_SCREEN_ATTUITION,
                             false);
      app_menu_consume_gesture(event);
      break;

    default:
      break;
    }
    break;
  default:
    break;
  }
}

static void app_menu_positioning_event(lv_event_t *event) {
  app_menu_track_touch(event);
  if (app_menu_click_suppressed(event)) return;
  switch (lv_event_get_code(event)) {
#if APP_MAP_EN
  case LV_EVENT_CLICKED:
    liot_trace("[menu] positioning click enter location");
    demo_location_page_enter_async();
    break;
#endif
  case LV_EVENT_GESTURE:
    switch (lv_indev_get_gesture_dir(lv_indev_get_act())) {
    case LV_DIR_RIGHT:
      app_menu_load_adjacent(APP_MENU_SCREEN_POSITIONING, APP_MENU_SCREEN_MP3,
                             true);
      app_menu_consume_gesture(event);
      break;
    case LV_DIR_LEFT:
      app_menu_load_adjacent(APP_MENU_SCREEN_POSITIONING, APP_MENU_SCREEN_SALARY,
                             false);
      app_menu_consume_gesture(event);
      break;

    default:
      break;
    }
    break;
  default:
    break;
  }
}

static void app_menu_mp3_event(lv_event_t *event) {
  app_menu_track_touch(event);
  if (app_menu_click_suppressed(event)) return;
  switch (lv_event_get_code(event)) {
#if APP_MP3_EN
  case LV_EVENT_CLICKED:
    liot_trace("[menu] mp3 click enter player");
    demo_mp3_page_enter_async();
    break;
#endif
  case LV_EVENT_GESTURE:
    switch (lv_indev_get_gesture_dir(lv_indev_get_act())) {
    case LV_DIR_RIGHT:
      app_menu_load_adjacent(APP_MENU_SCREEN_MP3, APP_MENU_SCREEN_TIME, true);
      app_menu_consume_gesture(event);
      break;
    case LV_DIR_LEFT:
      app_menu_load_adjacent(APP_MENU_SCREEN_MP3, APP_MENU_SCREEN_POSITIONING,
                             false);
      app_menu_consume_gesture(event);
      break;

    default:
      break;
    }
    break;
  default:
    break;
  }
}

void app_menu_init(void) {
  liot_trace("[app_menu] init");
#if APP_MAP_EN
  {
    liot_task_t network_task = NULL;
    LiotOSStatus_t network_ret;

#if defined(HWDEMO_GNSS_EN)
    liot_task_t gnss_task = NULL;
    LiotOSStatus_t gnss_ret;
    gnss_ret =
        liot_rtos_task_create(&gnss_task, 10240, LIOT_APP_TASK_PRIORITY + 1,
                              "gnss_service", liot_gnss_demo_thread, NULL);
#endif
    network_ret = liot_rtos_task_create(
        &network_task, 16 * 1024, LIOT_APP_TASK_PRIORITY + 1, "location_network",
        demo_location_service_thread, NULL);
#if defined(HWDEMO_GNSS_EN)
    liot_trace("[app_menu] location services gnss=%d network=%d", (int)gnss_ret,
               (int)network_ret);
#else
    liot_trace("[app_menu] location network service=%d (mock GNSS)",
               (int)network_ret);
#endif
  }
#endif
}

void app_menu_setup(lv_ui *ui) {
  ui_setup_scr_t setup_scr;
  lv_obj_t **first_screen;
  app_menu_screen_id_t first_id;

  if (ui == NULL) return;
  init_scr_del_flag(ui);
  init_keyboard(ui);

  if (app_menu_enabled_count() == 0u) return;

  first_id = g_app_menu_enabled_order[0];
  setup_scr = app_menu_setup_fn(first_id);
  first_screen = app_menu_screen_ptr(ui, first_id);
  if (setup_scr == NULL || first_screen == NULL) return;

  setup_scr(ui);
  lv_scr_load(*first_screen);
}

void app_menu_bind_time(lv_ui *ui) {
  lv_obj_add_event_cb(ui->time, app_menu_time_event, LV_EVENT_ALL, ui);
}

void app_menu_bind_baji(lv_ui *ui) {
  lv_obj_add_event_cb(ui->baji, app_menu_baji_event, LV_EVENT_ALL, ui);
  app_menu_baji_add_unbind_button(ui);
}

void app_menu_bind_attuition(lv_ui *ui) {
  lv_obj_add_event_cb(ui->attuition, app_menu_attuition_event, LV_EVENT_ALL,
                      ui);
}

void app_menu_bind_attfun(lv_ui *ui) {
  lv_obj_add_event_cb(ui->attfun, app_menu_attfun_event, LV_EVENT_ALL, ui);
}

void app_menu_bind_salary(lv_ui *ui) {
  lv_obj_add_event_cb(ui->salary, app_menu_salary_event, LV_EVENT_ALL, ui);
}

void app_menu_bind_positioning(lv_ui *ui) {
  lv_obj_add_event_cb(ui->positioning, app_menu_positioning_event, LV_EVENT_ALL,
                      ui);
}

void app_menu_bind_mp3(lv_ui *ui) {
  lv_obj_add_event_cb(ui->mp3, app_menu_mp3_event, LV_EVENT_ALL, ui);
}
