/**
 * @file lcd_gui.h
 * @brief LVGL GUI interface for SIM signal info display demo.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#ifndef _LCD_GUI_H_
#define _LCD_GUI_H_

#include <stdint.h>

/* LCD and LVGL init */
void lcd_port_system_init(void);
void lvgl_init(void);

/* Signal info GUI (CSQ / RSRP / SNR) */
void liot_lvgl_signal_gui_setup(void);
void lvgl_signal_update(uint8_t csq, int rsrp, int snr);

#endif /* _LCD_GUI_H_ */
