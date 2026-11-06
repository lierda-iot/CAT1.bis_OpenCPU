/* Real ThingConnect contacts; no credentials enter UI snapshots. */
#ifndef TIRTC_CONTACTS_H
#define TIRTC_CONTACTS_H
#include "../ui/tirtc_ui.h"
#include "../platform/device_binding.h"
typedef struct {
    char id[96], name[64], wx_app_id[64], wx_model_id[64];
    bool online, wechat;
    demo_binding_service_context_t context;
} tirtc_contact_t;
/* Idempotent task start, retriable after allocation failure. */
int tirtc_contacts_start(void);
/* UI context, RAM intent only; handles refresh and entry to contacts page. */
int tirtc_contacts_action(const tirtc_ui_action_t *action);
/* Cached copy plus current identity/route checks. No HTTP/modem I/O. */
void tirtc_contacts_publish(void);
/* Exact stable (id,wechat) lookup. Rejects stale, refreshing or expired data.
 * WX has no online fact; only device contacts should be gated by online. */
bool tirtc_contacts_lookup(const char *id, bool wechat, tirtc_contact_t *out);
typedef enum { TIRTC_CONTACT_ANY, TIRTC_CONTACT_WECHAT, TIRTC_CONTACT_DEVICE } tirtc_contact_scope_t;
typedef enum { TIRTC_RESOLVE_PENDING, TIRTC_RESOLVE_FOUND,
    TIRTC_RESOLVE_NOT_FOUND, TIRTC_RESOLVE_AMBIGUOUS, TIRTC_RESOLVE_OFFLINE,
    TIRTC_RESOLVE_UNAVAILABLE } tirtc_contact_result_t;
/* One correlated AI lookup; HTTP remains in the existing contacts worker.
 * Fresh exact full name/ID only. Failure is latched until cancel/new request;
 * a later periodic refresh cannot turn an old failed ticket into success. */
uint32_t tirtc_contacts_resolve_begin(const char *target, tirtc_contact_scope_t scope);
tirtc_contact_result_t tirtc_contacts_resolve_poll(uint32_t ticket, tirtc_contact_t *out);
void tirtc_contacts_resolve_cancel(uint32_t ticket);
#endif
