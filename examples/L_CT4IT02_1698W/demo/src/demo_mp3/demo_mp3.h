#ifndef DEMO_MP3_H
#define DEMO_MP3_H

#ifdef __cplusplus
extern "C" {
#endif

void liot_mp3_demo_thread(void *argv);
void demo_mp3_page_enter_async(void);
void demo_mp3_page_enter(void);
void demo_mp3_audio_preinit(void);
bool demo_mp3_audio_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif