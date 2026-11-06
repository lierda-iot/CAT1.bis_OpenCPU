#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H
#include "tirtc_ui.h"
#include "lvgl.h"
#include "font/tirtc_font_14.h"
#define UI_BG 0x141719
#define UI_TEXT 0xF1F3F4
#define UI_MUTED 0xA6ADB4
#define UI_ACCENT 0x9BD8CF
#define UI_SURFACE 0x22272B
#define UI_BUTTON 0x292E32
#define UI_DANGER 0xAE3947
#define UI_ACCEPT 0x246F53
extern tirtc_ui_state_t ui_state;
extern tirtc_ui_resources_t ui_resources;
extern tirtc_ui_system_t ui_system;
extern bool ui_system_live;
extern tirtc_ui_platform_t ui_platform;
extern bool ui_platform_live;
bool ui_platform_setup_active(void);
extern lv_obj_t *ui_screen;
extern tirtc_ui_page_t ui_page;
void tirtc_ui_platform_lock(void);
void tirtc_ui_platform_unlock(void);
void ui_copy(char *dst, size_t capacity, const char *src);
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, int x, int y, int w, uint32_t color);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, lv_event_cb_t cb, uintptr_t value);
lv_obj_t *ui_panel(lv_obj_t *parent, int x, int y, int w, int h, bool scroll);
void ui_header(const char *title, tirtc_ui_page_t back);
void ui_navigate(tirtc_ui_page_t page);
void ui_redraw(void);
void ui_notice(const char *message);
int ui_action(tirtc_ui_action_type_t type, uint8_t index, int32_t value, const char *text, const char *extra);
void ui_enabled(lv_obj_t *obj, bool enabled);
void ui_note_interaction(void);
bool ui_pointer_busy(void);
/* Secondary product pages: settings, network, diagnostics, room/form. */
bool ui_pages_render(tirtc_ui_page_t page);
void ui_pages_reset(void);
void ui_pages_process(bool changed, uint32_t now);
void ui_pages_navigation(tirtc_ui_page_t from, tirtc_ui_page_t to);
void ui_pages_cancel_ptt(void);
#endif
