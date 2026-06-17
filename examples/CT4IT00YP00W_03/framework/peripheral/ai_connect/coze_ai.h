#ifndef COZE_AI_H
#define COZE_AI_H

#include <stdint.h>

int coze_ai_chat_update(const char *imei, const char *voiceid);
int coze_ai_chat_upload_audio(const char *imei, uint8_t *data, uint32_t len);
int coze_ai_chat_upload_complete(const char *imei);
int coze_ai_chat_cancel(const char *imei);
int coze_ai_recv_message(uint8_t *data, uint32_t len);

#endif /* COZE_AI_H */
