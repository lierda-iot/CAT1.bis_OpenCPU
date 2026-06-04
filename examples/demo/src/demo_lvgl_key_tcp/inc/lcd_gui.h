#ifndef _LCD_GUI_H_
#define _LCD_GUI_H_

#include <stdint.h>

/* LCD port init */
void lcd_port_system_init(void);
void lvgl_init(void);

/* LVGL GUI setup & update (defined in demo_tgai_lcd_gui.c) */
void liot_lvgl_gui_setup(void);
void lvgl_status_label_update(char *str);
void lvgl_text_label_update(char *str);
void lvgl_emotion_label_update(char *str);
void lvgl_network_label_update(char *str);
void lvgl_battery_label_update(char *str);
void lvgl_usbInsert_label_update(char *str);
void lvgl_status_label_shot_time(char *str, uint32_t time);
void lvgl_display_logo(void);
void lvgl_display_software_info(void);
void lvgl_display_add_reply_text(const char *str);

/* Signal info GUI (CSQ / RSRP / SNR) */
void liot_lvgl_signal_gui_setup(void);
void lvgl_signal_update(uint8_t csq, int rsrp, int snr);

/* TCP info GUI (Status / Recv bytes) */
void liot_lvgl_tcp_gui_setup(void);
void lvgl_tcp_update(int socket_status, uint32_t recv_bytes);

#endif /* _LCD_GUI_H_ */
