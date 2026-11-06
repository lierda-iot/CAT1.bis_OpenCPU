/* SPDX-License-Identifier: MIT */
#ifndef TIRTC_AI_CALL_PROTOCOL_H
#define TIRTC_AI_CALL_PROTOCOL_H
#include "../platform/json_guard.h"
#include "../contacts/tirtc_contacts.h"
typedef struct {
    char rpc_id[97], target[257], expected_id[129];
    tirtc_contact_scope_t scope;
    bool video, rpc_id_numeric;
} ai_call_request_t;
/* NULL = valid; otherwise a static protocol error status. Missing/invalid id
 * leaves rpc_id empty, so the caller must neither execute nor reply. */
const char *ai_call_parse(const cJSON *root, ai_call_request_t *out);
bool ai_call_json_complete(const char *data, size_t length);
char *ai_call_reply_json(const char *id, bool ok, const char *status,
                         const char *message, bool wechat, bool video,
                         bool numeric_id);
#endif
