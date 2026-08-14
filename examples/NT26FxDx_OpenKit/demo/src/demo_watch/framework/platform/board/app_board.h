#ifndef APP_BOARD_H
#define APP_BOARD_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *board_name;
    int (*init)(void);
} app_board_ops_t;

int app_board_register(const app_board_ops_t *ops);
int app_board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BOARD_H */
