#ifndef DEMO_TONE_CORE_H
#define DEMO_TONE_CORE_H

/*
 * Pure volume state/validation logic extracted from demo_1khz_tone.c.
 * Header-only on purpose so the same source is host-testable without liot
 * RTOS / audio dependency. Production keeps a static state object initialized
 * to 100 and applies the audio call only when the codec is ready.
 */

#define DEMO_TONE_USER_VOLUME_MAX 100

typedef struct {
    volatile int volume;
} demo_tone_core_volume_t;

/*
 * Set a user volume (0..100). Returns -1 when volume is out of range or the
 * state pointer is NULL; on success stores the value and returns 0.
 * Mirrors the range check in demo_tone_set_volume (demo_1khz_tone.c).
 */
static inline int demo_tone_core_volume_set(demo_tone_core_volume_t *state,
                                            int volume)
{
    if ((state == NULL) ||
        (volume < 0) ||
        (volume > DEMO_TONE_USER_VOLUME_MAX)) {
        return -1;
    }
    state->volume = volume;
    return 0;
}

/*
 * Return the last accepted volume. Mirrors demo_tone_get_volume.
 */
static inline int demo_tone_core_volume_get(const demo_tone_core_volume_t *state)
{
    return (state != NULL) ? state->volume : -1;
}

#endif /* DEMO_TONE_CORE_H */
