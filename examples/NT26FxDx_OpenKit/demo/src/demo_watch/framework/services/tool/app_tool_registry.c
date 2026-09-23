#include "app_tool_registry.h"

#include <stddef.h>
#include <string.h>

#define APP_TOOL_MAX 24U

static app_tool_t s_tools[APP_TOOL_MAX];
static uint32_t s_tool_count;

static int tool_find(const char *name)
{
    uint32_t i;

    if (name == NULL) {
        return -1;
    }

    for (i = 0; i < s_tool_count; i++) {
        if (strncmp(s_tools[i].name, name, APP_TOOL_NAME_MAX) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int app_tool_registry_init(void)
{
    memset(s_tools, 0, sizeof(s_tools));
    s_tool_count = 0;
    return APP_OK;
}

int app_tool_register(const char *name,
                      const char *desc,
                      app_tool_handler_t handler,
                      void *user)
{
    app_tool_t *tool;

    if (name == NULL || name[0] == '\0' || handler == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (tool_find(name) >= 0) {
        return APP_ERR_BUSY;
    }
    if (s_tool_count >= APP_TOOL_MAX) {
        return APP_ERR_NO_MEMORY;
    }

    tool = &s_tools[s_tool_count++];
    strncpy(tool->name, name, APP_TOOL_NAME_MAX - 1U);
    tool->name[APP_TOOL_NAME_MAX - 1U] = '\0';
    if (desc != NULL) {
        strncpy(tool->desc, desc, APP_TOOL_DESC_MAX - 1U);
        tool->desc[APP_TOOL_DESC_MAX - 1U] = '\0';
    }
    tool->handler = handler;
    tool->user = user;
    return APP_OK;
}

int app_tool_call(const char *name,
                  const char *params_json,
                  char *result_json,
                  uint32_t result_size)
{
    int index = tool_find(name);

    if (index < 0) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_tools[index].handler(params_json, result_json, result_size, s_tools[index].user);
}

const app_tool_t *app_tool_get(uint32_t index)
{
    if (index >= s_tool_count) {
        return NULL;
    }
    return &s_tools[index];
}

uint32_t app_tool_count(void)
{
    return s_tool_count;
}
