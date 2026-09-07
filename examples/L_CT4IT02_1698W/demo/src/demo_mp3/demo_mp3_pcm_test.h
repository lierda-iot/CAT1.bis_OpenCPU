#ifndef DEMO_MP3_PCM_TEST_H
#define DEMO_MP3_PCM_TEST_H

#include "liot_audio2.h"

Liot_AudErr_e demo_mp3_play_pcm_test(void);

/* Temporarily route the player through raw PCM to isolate the MP3 decoder. */
#define Liot_AudioPlayMp3(data, length) demo_mp3_play_pcm_test()

#endif
