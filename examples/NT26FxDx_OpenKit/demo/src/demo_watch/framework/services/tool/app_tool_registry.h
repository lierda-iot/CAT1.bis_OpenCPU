#ifndef APP_TOOL_REGISTRY_H
#define APP_TOOL_REGISTRY_H

#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_TOOL_NAME_MAX 48U
#define APP_TOOL_DESC_MAX 160U

typedef int (*app_tool_handler_t)(const char *params_json,
                                  char *result_json,
                                  uint32_t result_size,
                                  void *user);

typedef struct {
    char name[APP_TOOL_NAME_MAX];
    char desc[APP_TOOL_DESC_MAX];
    app_tool_handler_t handler;
    void *user;
} app_tool_t;

int app_tool_registry_init(void);
int app_tool_register(const char *name,
                      const char *desc,
                      app_tool_handler_t handler,
                      void *user);
int app_tool_call(const char *name,
                  const char *params_json,
                  char *result_json,
                  uint32_t result_size);
const app_tool_t *app_tool_get(uint32_t index);
uint32_t app_tool_count(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TOOL_REGISTRY_H */
