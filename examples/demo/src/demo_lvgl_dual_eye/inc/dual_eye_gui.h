#ifndef _DUAL_EYE_GUI_H_
#define _DUAL_EYE_GUI_H_

#include "lvgl.h"

void dual_eye_lvgl_init(void);
void dual_eye_gui_setup(lv_disp_t *left_disp, lv_disp_t *right_disp);

#endif /* _DUAL_EYE_GUI_H_ */
