#include <stdio.h>
#include <stdlib.h>

#include "liot_at_cmd.h"
#include "liot_log.h"

int demo_tone_set_volume(int volume);
int demo_tone_get_volume(void);

static liot_at_result_enum_type tone_volume_atcmd(const liot_atCommand_Input *input)
{
    char response[40] = {0};

    if (input == NULL) return LIOT_AT_ERROR;

    switch (input->op) {
    case LIOT_AT_CMD_SET: {
        char *end = NULL;
        long volume;

        if (input->args_count != 1 || input->arg[0] == '\0')
            return liot_atcmd_reply(input->atHandle, LIOT_AT_ERROR, NULL);
        volume = strtol(input->arg, &end, 10);
        if (end == input->arg || *end != '\0' || demo_tone_set_volume((int)volume) != 0)
            return liot_atcmd_reply(input->atHandle, LIOT_AT_ERROR, NULL);
        snprintf(response, sizeof(response), "+TONEVOL: %d", demo_tone_get_volume());
        liot_trace("[tone-1khz] AT volume set=%d", demo_tone_get_volume());
        return liot_atcmd_reply(input->atHandle, LIOT_AT_OK, response);
    }
    case LIOT_AT_CMD_READ:
        snprintf(response, sizeof(response), "+TONEVOL: %d", demo_tone_get_volume());
        return liot_atcmd_reply(input->atHandle, LIOT_AT_OK, response);
    case LIOT_AT_CMD_TEST:
        return liot_atcmd_reply(input->atHandle, LIOT_AT_OK, "+TONEVOL: (0-100)");
    default:
        return liot_atcmd_reply(input->atHandle, LIOT_AT_ERROR, NULL);
    }
}

static const liot_atCommand g_tone_atcmd_table[] = {
    LIOT_ATCMD("+TONEVOL", tone_volume_atcmd)
};

void demo_tone_atcmd_init(void)
{
    liot_atcmd_register((liot_atCommandP)g_tone_atcmd_table,
                        sizeof(g_tone_atcmd_table) / sizeof(g_tone_atcmd_table[0]));
    liot_open_atcmd_init();
    liot_trace("[tone-1khz] AT ready: AT+TONEVOL=<0-100>, AT+TONEVOL?");
}
