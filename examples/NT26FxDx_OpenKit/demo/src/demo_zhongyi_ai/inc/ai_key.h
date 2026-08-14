/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO20 (KEY_USER0) button handler with debounce.
 */

#ifndef AI_KEY_H
#define AI_KEY_H

typedef void (*ai_key_callback_t)(void);

int ai_key_init(ai_key_callback_t on_press);

#endif /* AI_KEY_H */
