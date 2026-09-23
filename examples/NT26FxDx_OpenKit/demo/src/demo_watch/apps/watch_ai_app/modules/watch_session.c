#include "watch_session.h"

#include "app_osal.h"

typedef struct {
    watch_session_t current;
    const char *name;
    watch_session_stop_cb_t stop;
} watch_session_ctx_t;

static watch_session_ctx_t s_session;

const char *watch_session_name(watch_session_t session)
{
    switch (session) {
    case WATCH_SESSION_NONE:
        return "none";
    case WATCH_SESSION_CAMERA:
        return "camera";
    case WATCH_SESSION_SCAN:
        return "scan";
    case WATCH_SESSION_PLAYER:
        return "player";
    case WATCH_SESSION_RECORDER:
        return "recorder";
    case WATCH_SESSION_GIF:
        return "gif";
    case WATCH_SESSION_DRAWING:
        return "drawing";
    default:
        return "unknown";
    }
}

watch_session_t watch_session_current(void)
{
    return s_session.current;
}

int watch_session_open(watch_session_t session,
                       const char *name,
                       watch_session_stop_cb_t stop)
{
    if (session == WATCH_SESSION_NONE || stop == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_session.current == session) {
        return APP_OK;
    }
    if (s_session.current != WATCH_SESSION_NONE) {
        app_log("watch session busy: current=%s request=%s",
                watch_session_name(s_session.current),
                watch_session_name(session));
        return APP_ERR_BUSY;
    }

    s_session.current = session;
    s_session.name = (name != NULL) ? name : watch_session_name(session);
    s_session.stop = stop;
    app_log("watch session open: %s", s_session.name);
    return APP_OK;
}

int watch_session_close(watch_session_t session)
{
    if (s_session.current == WATCH_SESSION_NONE) {
        return APP_OK;
    }
    if (session != WATCH_SESSION_NONE && s_session.current != session) {
        app_log("watch session close ignored: current=%s request=%s",
                watch_session_name(s_session.current),
                watch_session_name(session));
        return APP_ERR_INVALID_ARG;
    }

    app_log("watch session close: %s", s_session.name);
    s_session.current = WATCH_SESSION_NONE;
    s_session.name = NULL;
    s_session.stop = NULL;
    return APP_OK;
}

int watch_session_go_back(void)
{
    if (s_session.current == WATCH_SESSION_NONE || s_session.stop == NULL) {
        app_log("watch session back ignored: none");
        return APP_OK;
    }

    app_log("watch session back: %s", s_session.name);
    return s_session.stop();
}
