#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ui_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static lv_color_t pixels[320*40];
static bool down;
static int x,y,presses,releases,opens,closes,gain_changes;
static char last_press[128],last_release[128];
void tirtc_ui_platform_lock(void) {}
void tirtc_ui_platform_unlock(void) {}
void tirtc_port_set_brightness(uint8_t n) {(void)n;}
static int backend(const tirtc_ui_action_t *a,void *ctx) {
 (void)ctx;
 if(a->type==TIRTC_ACTION_ROOM_PTT){
  if(a->value){++presses;strcpy(last_press,a->extra);}
  else{++releases;strcpy(last_release,a->extra);}
 }
 if(a->type==TIRTC_ACTION_ROOM_OPEN){if(a->value)++opens;else++closes;}
 if(a->type==TIRTC_ACTION_SET_MIC_GAIN){
  ++gain_changes;
  assert(!tirtc_ui_publish_audio_settings(ui_state.volume,(uint8_t)a->value,
                                         ui_state.speaker_enabled,ui_state.mic_enabled));
 }
 return 0;
}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){
 (void)a;(void)p;tirtc_ui_video_flush_done(true,lv_disp_flush_is_last(d));lv_disp_flush_ready(d);
}
static void read_pointer(lv_indev_drv_t *d,lv_indev_data_t *p){
 (void)d;p->point.x=x;p->point.y=y;p->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void pump(int ms){for(int i=0;i<ms;i+=5){lv_tick_inc(5);tirtc_ui_process();lv_timer_handler();}}
static lv_obj_t *label(lv_obj_t *o,const char *text){
 if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return o;
 for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i){lv_obj_t *r=label(lv_obj_get_child(o,i),text);if(r)return r;}
 return NULL;
}
static void point_at(const char *text){
 lv_obj_t *l=label(ui_screen,text);assert(l);lv_obj_t *b=lv_obj_get_parent(l);lv_area_t a;
 lv_obj_get_coords(b,&a);x=(a.x1+a.x2)/2;y=(a.y1+a.y2)/2;
}
static void set_gain_in_settings(unsigned value){
 tirtc_ui_request_page(TIRTC_PAGE_SETTINGS);pump(100);assert(ui_page==TIRTC_PAGE_SETTINGS);
 lv_obj_t *caption=label(ui_screen,"麦克风音量");assert(caption);
 lv_obj_t *row=lv_obj_get_parent(caption),*choice=NULL;
 for(uint32_t i=0;i<lv_obj_get_child_cnt(row);++i){
  lv_obj_t *child=lv_obj_get_child(row,i);
  if(lv_obj_check_type(child,&lv_dropdown_class)){choice=child;break;}
 }
 assert(choice);int before=gain_changes;
 lv_dropdown_set_selected(choice,(uint16_t)value);
 assert(lv_event_send(choice,LV_EVENT_VALUE_CHANGED,NULL)==LV_RES_OK);pump(50);
 assert(gain_changes==before+1&&ui_state.mic_gain==value&&ui_state.mic_enabled);
 tirtc_ui_request_page(TIRTC_PAGE_ROOM);pump(100);assert(ui_page==TIRTC_PAGE_ROOM);
}
int main(void){
 lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,pixels,NULL,320*40);
 lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=320;d.ver_res=240;d.draw_buf=&db;d.flush_cb=flush;assert(lv_disp_drv_register(&d));
 lv_indev_drv_t in;lv_indev_drv_init(&in);in.type=LV_INDEV_TYPE_POINTER;in.read_cb=read_pointer;assert(lv_indev_drv_register(&in));
 tirtc_ui_set_backend(backend,NULL);tirtc_ui_init();pump(100);
 tirtc_ui_room_t room={0};room.known=room.assigned=room.connected=true;room.generation=9;
 strcpy(room.code,"001234");strcpy(room.message,"房间已连接");
 tirtc_ui_publish_audio_settings(6,9,true,true);assert(!tirtc_ui_publish_room(&room));
 tirtc_ui_request_page(TIRTC_PAGE_ROOM);pump(120);assert(ui_page==TIRTC_PAGE_ROOM);assert(opens==1);
 point_at("按下讲话");down=true;pump(350);assert(presses>=2);assert(!strcmp(last_press,"9"));
 int n=presses;room.generation=10;strcpy(room.code,"001235");assert(!tirtc_ui_publish_room(&room));pump(300);
 assert(releases==1);assert(!strcmp(last_release,"9"));assert(presses==n);
 down=false;pump(100);point_at("按下讲话");down=true;pump(80);assert(presses>n);assert(!strcmp(last_press,"10"));
 room.connected=false;assert(!tirtc_ui_publish_room(&room));pump(100);assert(releases==2);assert(!strcmp(last_release,"10"));
 n=presses;pump(900);assert(presses==n);down=false;pump(100);
 assert(ui_state.volume==6&&ui_state.mic_gain==9);room.member_count=13;assert(tirtc_ui_publish_room(&room)<0);
 room.member_count=0;room.connected=true;room.generation=11;assert(!tirtc_ui_publish_room(&room));pump(100);
 point_at("按下讲话");down=true;pump(80);tirtc_ui_publish_audio_settings(6,9,true,false);pump(100);assert(releases==3);
 n=presses;pump(400);assert(presses==n);down=false;pump(80);tirtc_ui_publish_audio_settings(6,9,true,true);pump(80);
 point_at("按下讲话");down=true;pump(80);down=false;pump(80);assert(releases==4);
 /* A held pointer defers call-page navigation. A complete short call can be
  * coalesced into ENDED without ever leaving ROOM or emitting another OPEN.
  * The owner must retain the room intent across call preemption; the UI still
  * releases the old PTT immediately and never resumes that physical press. */
 {
  int prior_opens=opens,prior_closes=closes;
  tirtc_ui_call_t quick_call={0};quick_call.state=TIRTC_CALL_INCOMING;
  quick_call.type=TIRTC_CALL_AUDIO;quick_call.session_id=90;
  strcpy(quick_call.peer_name,"Short call");
  point_at("按下讲话");down=true;pump(80);n=presses;
  tirtc_ui_publish_call(&quick_call);room.connected=false;
  assert(!tirtc_ui_publish_room(&room));pump(100);
  assert(ui_page==TIRTC_PAGE_ROOM);assert(releases==5);
  quick_call.state=TIRTC_CALL_ENDED;tirtc_ui_publish_call(&quick_call);pump(100);
  assert(ui_page==TIRTC_PAGE_ROOM);assert(presses==n);
  down=false;pump(100);assert(ui_page==TIRTC_PAGE_ROOM);
  assert(opens==prior_opens&&closes==prior_closes);
  /* The backend's resumed session requires a fresh press. */
  room.connected=true;room.generation=12;assert(!tirtc_ui_publish_room(&room));pump(100);
  assert(presses==n);point_at("按下讲话");down=true;pump(80);
  assert(presses>n);assert(!strcmp(last_press,"12"));
  down=false;pump(80);assert(releases==6);
 }
 tirtc_ui_request_page(TIRTC_PAGE_MENU);pump(100);assert(closes==1);assert(ui_page==TIRTC_PAGE_MENU);
 tirtc_ui_request_page(TIRTC_PAGE_ROOM);pump(100);
 tirtc_ui_call_t call={0};call.state=TIRTC_CALL_INCOMING;call.type=TIRTC_CALL_AUDIO;call.session_id=91;strcpy(call.peer_name,"Peer");
 tirtc_ui_publish_call(&call);pump(100);assert(ui_page==TIRTC_PAGE_CALL);
 call.state=TIRTC_CALL_ENDED;tirtc_ui_publish_call(&call);pump(100);assert(ui_page==TIRTC_PAGE_ROOM);
 tirtc_ui_request_page(TIRTC_PAGE_MENU);pump(100);call.state=TIRTC_CALL_OUTGOING;call.session_id=92;tirtc_ui_publish_call(&call);pump(100);assert(ui_page==TIRTC_PAGE_CALL);
 call.state=TIRTC_CALL_ENDED;tirtc_ui_publish_call(&call);pump(100);assert(ui_page==TIRTC_PAGE_CALL_RESULT);
 /* A cleanup timeout leaves the call page without marking media reusable.
  * Held touch still fences widget deletion. A later terminal publication
  * must not pull the user out of HOME or a subsequently opened settings page. */
 tirtc_ui_request_page(TIRTC_PAGE_ROOM);pump(100);
 call.state=TIRTC_CALL_INCOMING;call.session_id=93;tirtc_ui_publish_call(&call);pump(100);
 assert(ui_page==TIRTC_PAGE_CALL);
 call.state=TIRTC_CALL_CLOSING;tirtc_ui_publish_call(&call);pump(100);assert(ui_page==TIRTC_PAGE_CALL);
 x=160;y=100;down=true;pump(80);
 call.state=TIRTC_CALL_ERROR;call.error=TIRTC_CALL_CLEANUP_PENDING;
 strcpy(call.message,"正在后台结束通话，请稍后重试");tirtc_ui_publish_call(&call);pump(100);
 assert(ui_page==TIRTC_PAGE_CALL);
 down=false;pump(100);assert(ui_page==TIRTC_PAGE_HOME);
 assert(label(lv_layer_top(),"正在后台结束通话，请稍后重试"));
 call.state=TIRTC_CALL_ENDED;call.error=0;tirtc_ui_publish_call(&call);pump(100);
 assert(ui_page==TIRTC_PAGE_HOME);
 tirtc_ui_request_page(TIRTC_PAGE_SETTINGS);pump(100);
 call.state=TIRTC_CALL_ERROR;call.error=-1234;tirtc_ui_publish_call(&call);pump(100);
 assert(ui_page==TIRTC_PAGE_SETTINGS);
 /* A later, distinct incoming session still opens the normal call page. */
 call.state=TIRTC_CALL_INCOMING;call.error=0;call.session_id=94;tirtc_ui_publish_call(&call);pump(100);
 assert(ui_page==TIRTC_PAGE_CALL);
 call.state=TIRTC_CALL_ENDED;tirtc_ui_publish_call(&call);pump(100);assert(ui_page==TIRTC_PAGE_CALL_RESULT);
 puts("PASS: delayed call cleanup returns HOME, defers held-touch navigation, keeps later terminal results in the background, and admits a new call page normally.");
 /* L0 is a valid setting with the microphone switch still on. The room UI
  * must match the backend's gain-zero gate instead of offering a failing PTT. */
 set_gain_in_settings(0);
 lv_obj_t *muted=label(ui_screen,"麦克风静音（L0）");assert(muted);
 assert(lv_obj_has_state(lv_obj_get_parent(muted),LV_STATE_DISABLED));
 n=presses;point_at("麦克风静音（L0）");down=true;pump(300);down=false;pump(80);
 assert(presses==n);
 set_gain_in_settings(7);
 lv_obj_t *ready=label(ui_screen,"按下讲话");assert(ready);
 assert(!lv_obj_has_state(lv_obj_get_parent(ready),LV_STATE_DISABLED));
 int before_release=releases;
 point_at("按下讲话");down=true;pump(80);assert(presses==n+1);
 /* A settings publication while held must revoke this physical gesture.
  * Restoring gain before releasing must not renew or reopen the old press. */
 assert(!tirtc_ui_publish_audio_settings(6,0,true,true));pump(100);
 assert(releases==before_release+1);
 n=presses;
 assert(!tirtc_ui_publish_audio_settings(6,7,true,true));pump(900);
 assert(presses==n&&releases==before_release+1);
 down=false;pump(100);point_at("按下讲话");down=true;pump(80);
 assert(presses==n+1);down=false;pump(80);assert(releases==before_release+2);
 puts("PASS: native LVGL room UI: PTT renew/release, generation change under held touch, disconnect, no stale retransmit, preferences isolation, member bounds, navigation, coalesced short call while PTT held.");
 puts("PASS: settings L0 disables room PTT with mute feedback; L7 restores fresh presses; gain-zero publication cancels held gesture until physical release.");
 return 0;
}
