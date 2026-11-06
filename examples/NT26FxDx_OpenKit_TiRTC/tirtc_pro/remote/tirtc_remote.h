/* Platform VIEW / remote talkback; one managed LIVE media-session owner. */
#ifndef TIRTC_REMOTE_H
#define TIRTC_REMOTE_H
#include "tirtc_ui.h"

int tirtc_remote_start_service(void);
/* UI actions copy intent only. Controls carry the displayed generation as
 * unsigned decimal action.extra. Remote controls are session-local. */
int tirtc_remote_action(const tirtc_ui_action_t *action);
bool tirtc_remote_has_session(void);
void tirtc_remote_get_snapshot(tirtc_ui_remote_t *out);
#endif
