#include "app_board.h"

#include "app_osal.h"

static int board_stub_init(void)
{
    return APP_OK;
}

static const app_board_ops_t s_board_stub = {
    .board_name = "generic-stub",
    .init = board_stub_init,
};

static const app_board_ops_t *s_board = &s_board_stub;

int app_board_register(const app_board_ops_t *ops)
{
    if (ops == NULL || ops->board_name == NULL || ops->init == NULL) {
        app_log("board register rejected: invalid ops");
        return APP_ERR_INVALID_ARG;
    }
    s_board = ops;
    app_log("board registered: %s", s_board->board_name);
    return APP_OK;
}

int app_board_init(void)
{
    int ret;

    app_log("board init dispatch: %s", s_board->board_name);
    ret = s_board->init();
    if (ret != APP_OK) {
        app_log("board init failed: %s ret=%d", s_board->board_name, ret);
        return ret;
    }
    app_log("board init done: %s", s_board->board_name);
    return APP_OK;
}
