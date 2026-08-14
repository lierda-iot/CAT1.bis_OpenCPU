#ifndef APP_ERROR_H
#define APP_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_OK = 0,
    APP_ERR_FAIL = -1,
    APP_ERR_INVALID_ARG = -2,
    APP_ERR_NO_MEMORY = -3,
    APP_ERR_NOT_READY = -4,
    APP_ERR_NOT_SUPPORTED = -5,
    APP_ERR_TIMEOUT = -6,
    APP_ERR_BUSY = -7,
} app_err_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_ERROR_H */
