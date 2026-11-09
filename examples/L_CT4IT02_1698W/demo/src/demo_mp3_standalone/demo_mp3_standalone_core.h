#ifndef DEMO_MP3_STANDALONE_CORE_H
#define DEMO_MP3_STANDALONE_CORE_H

/*
 * Pure volume-mapping logic extracted from demo_mp3_standalone.c.
 * Header-only on purpose: the production call site keeps identical behavior
 * with no liot audio dependency, so the same source is host-testable as-is.
 */

#define DEMO_MP3_STANDALONE_USER_VOLUME_MAX 100
#define DEMO_MP3_STANDALONE_HW_VOLUME_MAX 70

/*
 * Map a user volume (0..100) onto the safe hardware volume range.
 * Returns -1 when the user volume is out of range.
 */
static inline int demo_mp3_standalone_hw_volume(int user_volume)
{
    if (user_volume < 0 || user_volume > DEMO_MP3_STANDALONE_USER_VOLUME_MAX) {
        return -1;
    }
    return (user_volume * DEMO_MP3_STANDALONE_HW_VOLUME_MAX + 50) / 100;
}

#endif /* DEMO_MP3_STANDALONE_CORE_H */
