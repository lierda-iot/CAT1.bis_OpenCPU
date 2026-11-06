/* Procedural face adapter for LVGL 8 / Lierda CAT.1.
 * Geometry and animation are derived from tangeai/xiaotai-esp32:
 * components/starter_product/src/starter_product_s3_face.inc.
 * All calls, including destruction, belong to the LVGL task.
 */
#ifndef TIRTC_UI_FACE_H
#define TIRTC_UI_FACE_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_FACE_WIDTH 220
#define UI_FACE_HEIGHT 108
#define UI_FACE_VARIANT_AUTO 255U

typedef enum {
    UI_FACE_IDLE = 0,
    UI_FACE_LISTENING = 1,
    UI_FACE_THINKING = 2,
    UI_FACE_SPEAKING = 3,
    UI_FACE_RESTING = 4,
    UI_FACE_ERROR = 5
} ui_face_phase_t;

#ifdef TIRTC_EXTERNAL_UI_ASSETS
/* Sole resource worker before assets READY; all other calls remain UI-owned. */
int ui_face_load_external(void (*progress)(uint32_t));
#endif

/* One active face. Creating another detaches the previous one. No image zoom
 * or display rotation is applied here; x/y use the 320x240 logical display.
 * NULL parent, unavailable timer or insufficient LVGL memory returns NULL. */
lv_obj_t *ui_face_create(lv_obj_t *parent, int32_t x, int32_t y);
/* audio_level is the upstream activity level 0..3, not a percentage.
 * variant 0=A, 1=B; thinking/relaxed fall back to A. AUTO chooses a variant
 * when the expression changes, following upstream behavior. NULL/unknown key
 * becomes neutral. Speaking animates a state accent, not synchronized lips. */
void ui_face_set(const char *key, uint8_t phase, uint8_t audio_level, uint8_t variant);
void ui_face_detach(void);
void ui_face_pause(bool paused);

/* Create the fixed P4 gradient image at (0,0), with no mutable full-screen
 * pixel buffer. Call before creating the face and other foreground objects.
 * Face canvas background is restored from this image to avoid a visible edge. */
lv_obj_t *ui_face_background(lv_obj_t *parent);
lv_color_t ui_face_background_color(void);
uint8_t ui_face_key_count(void);
const char *ui_face_key_at(uint8_t index);
uint8_t ui_face_variant_count(const char *key);

#ifdef __cplusplus
}
#endif
#endif
