#ifndef TIRTC_USER_MAIN_H
#define TIRTC_USER_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Called by components/kernel/core/main.c from the application task. */
void user_main(void);

#ifdef __cplusplus
}
#endif

#endif
