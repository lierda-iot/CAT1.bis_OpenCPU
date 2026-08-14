#include "app_state.h"

#include <stdbool.h>
#include <stddef.h>

#include "app_osal.h"

#define APP_STATE_LISTENER_MAX 6U

typedef struct {
    app_state_listener_t cb;
    void *user;
} app_state_listener_entry_t;

static app_state_t s_state = APP_STATE_BOOTING;
static app_state_listener_entry_t s_listeners[APP_STATE_LISTENER_MAX];

static bool app_state_is_valid(app_state_t state)
{
    return state >= APP_STATE_BOOTING && state < APP_STATE_MAX;
}

const char *app_state_name(app_state_t state)
{
    switch (state) {
    case APP_STATE_BOOTING:
        return "booting";
    case APP_STATE_IDLE:
        return "idle";
    case APP_STATE_CONNECTING:
        return "connecting";
    case APP_STATE_CAMERA:
        return "camera";
    case APP_STATE_SCAN:
        return "scan";
    case APP_STATE_LISTENING:
        return "listening";
    case APP_STATE_THINKING:
        return "thinking";
    case APP_STATE_SPEAKING:
        return "speaking";
    case APP_STATE_ACTIVATING:
        return "activating";
    case APP_STATE_UPGRADING:
        return "upgrading";
    case APP_STATE_SLEEP:
        return "sleep";
    case APP_STATE_ERROR:
        return "error";
    case APP_STATE_SHUTDOWN:
        return "shutdown";
    default:
        return "unknown";
    }
}

static bool app_state_can_transition(app_state_t from, app_state_t to)
{
    if (!app_state_is_valid(to)) {
        return false;
    }
    if (from == to) {
        return true;
    }
    if (to == APP_STATE_ERROR || to == APP_STATE_SHUTDOWN) {
        return true;
    }

    switch (from) {
    case APP_STATE_BOOTING:
        return to == APP_STATE_IDLE || to == APP_STATE_ACTIVATING;
    case APP_STATE_IDLE:
        return to == APP_STATE_CONNECTING ||
               to == APP_STATE_CAMERA ||
               to == APP_STATE_SCAN ||
               to == APP_STATE_LISTENING ||
               to == APP_STATE_UPGRADING ||
               to == APP_STATE_SLEEP;
    case APP_STATE_CONNECTING:
        return to == APP_STATE_IDLE || to == APP_STATE_LISTENING;
    case APP_STATE_CAMERA:
        return to == APP_STATE_IDLE;
    case APP_STATE_SCAN:
        return to == APP_STATE_IDLE;
    case APP_STATE_LISTENING:
        return to == APP_STATE_THINKING || to == APP_STATE_SPEAKING || to == APP_STATE_IDLE;
    case APP_STATE_THINKING:
        return to == APP_STATE_SPEAKING || to == APP_STATE_IDLE;
    case APP_STATE_SPEAKING:
        return to == APP_STATE_LISTENING || to == APP_STATE_IDLE;
    case APP_STATE_ACTIVATING:
        return to == APP_STATE_IDLE || to == APP_STATE_UPGRADING;
    case APP_STATE_UPGRADING:
        return to == APP_STATE_IDLE || to == APP_STATE_ACTIVATING;
    case APP_STATE_SLEEP:
        return to == APP_STATE_IDLE;
    case APP_STATE_ERROR:
        return to == APP_STATE_IDLE;
    case APP_STATE_SHUTDOWN:
    default:
        return false;
    }
}

static void app_state_notify(app_state_t old_state, app_state_t new_state, const app_event_t *event)
{
    uint32_t i;

    for (i = 0; i < APP_STATE_LISTENER_MAX; i++) {
        if (s_listeners[i].cb != NULL) {
            s_listeners[i].cb(old_state, new_state, event, s_listeners[i].user);
        }
    }
}

int app_state_init(void)
{
    s_state = APP_STATE_BOOTING;
    for (uint32_t i = 0; i < APP_STATE_LISTENER_MAX; i++) {
        s_listeners[i].cb = NULL;
        s_listeners[i].user = NULL;
    }
    app_log("app_state initialized: %s", app_state_name(s_state));
    return APP_OK;
}

app_state_t app_state_get(void)
{
    return s_state;
}

int app_state_transition(app_state_t next, const app_event_t *event)
{
    app_state_t old_state = s_state;

    if (!app_state_can_transition(old_state, next)) {
        app_log("state transition rejected: %s -> %s",
                app_state_name(old_state),
                app_state_name(next));
        return APP_ERR_INVALID_ARG;
    }
    if (old_state == next) {
        return APP_OK;
    }

    s_state = next;
    app_log("state: %s -> %s", app_state_name(old_state), app_state_name(next));
    app_state_notify(old_state, next, event);
    return APP_OK;
}

int app_state_handle_event(const app_event_t *event)
{
    if (event == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    switch (event->id) {
    case APP_EV_BOOT_DONE:
        return app_state_transition(APP_STATE_IDLE, event);
    case APP_EV_NETWORK_CONNECTED:
        if (s_state == APP_STATE_BOOTING || s_state == APP_STATE_IDLE) {
            return app_state_transition(APP_STATE_CONNECTING, event);
        }
        return APP_OK;
    case APP_EV_PROTOCOL_CONNECTED:
        if (s_state == APP_STATE_CONNECTING) {
            return app_state_transition(APP_STATE_IDLE, event);
        }
        return APP_OK;
    case APP_EV_CAMERA_START:
        if (s_state == APP_STATE_IDLE) {
            return app_state_transition(APP_STATE_CAMERA, event);
        }
        return APP_OK;
    case APP_EV_CAMERA_STOP:
        if (s_state == APP_STATE_CAMERA) {
            return app_state_transition(APP_STATE_IDLE, event);
        }
        return APP_OK;
    case APP_EV_CAMERA_ERROR:
        return app_state_transition(APP_STATE_ERROR, event);
    case APP_EV_SCAN_START:
        if (s_state == APP_STATE_IDLE) {
            return app_state_transition(APP_STATE_SCAN, event);
        }
        return APP_OK;
    case APP_EV_SCAN_STOP:
        if (s_state == APP_STATE_SCAN) {
            return app_state_transition(APP_STATE_IDLE, event);
        }
        return APP_OK;
    case APP_EV_SCAN_RESULT:
        return APP_OK;
    case APP_EV_TOUCH_ACTION:
    case APP_EV_DISPLAY_ACTION:
    case APP_EV_RECORDER_DONE:
        return APP_OK;
    case APP_EV_WAKEUP:
    case APP_EV_BUTTON_PRESS:
        if (s_state == APP_STATE_IDLE) {
            return app_state_transition(APP_STATE_CONNECTING, event);
        }
        if (s_state == APP_STATE_SPEAKING || s_state == APP_STATE_LISTENING) {
            return app_state_transition(APP_STATE_IDLE, event);
        }
        return APP_OK;
    case APP_EV_VAD_START:
        if (s_state == APP_STATE_IDLE || s_state == APP_STATE_CONNECTING) {
            return app_state_transition(APP_STATE_LISTENING, event);
        }
        return APP_OK;
    case APP_EV_VAD_END:
        if (s_state == APP_STATE_LISTENING) {
            return app_state_transition(APP_STATE_THINKING, event);
        }
        return APP_OK;
    case APP_EV_AUDIO_RECV:
        if (s_state == APP_STATE_THINKING || s_state == APP_STATE_LISTENING) {
            return app_state_transition(APP_STATE_SPEAKING, event);
        }
        return APP_OK;
    case APP_EV_PROTOCOL_DISCONNECTED:
    case APP_EV_NETWORK_DISCONNECTED:
        return app_state_transition(APP_STATE_IDLE, event);
    case APP_EV_OTA_PROGRESS:
        return app_state_transition(APP_STATE_UPGRADING, event);
    case APP_EV_ERROR:
        return app_state_transition(APP_STATE_ERROR, event);
    case APP_EV_SHUTDOWN:
        return app_state_transition(APP_STATE_SHUTDOWN, event);
    default:
        return APP_OK;
    }
}

int app_state_add_listener(app_state_listener_t listener, void *user)
{
    uint32_t i;

    if (listener == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    for (i = 0; i < APP_STATE_LISTENER_MAX; i++) {
        if (s_listeners[i].cb == NULL) {
            s_listeners[i].cb = listener;
            s_listeners[i].user = user;
            app_log("app_state listener added: slot=%u", (unsigned int)i);
            return APP_OK;
        }
    }
    app_log("app_state listener add failed: no free slot");
    return APP_ERR_NO_MEMORY;
}
