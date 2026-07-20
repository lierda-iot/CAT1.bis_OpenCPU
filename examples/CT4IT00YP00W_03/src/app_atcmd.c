#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_at_cmd.h"
#include "liot_os.h"
#include "liot_type.h"
#include "app_nv.h"

liot_at_result_enum_type liot_ai_atcmd_LAITESTMODE(const liot_atCommand_Input *pAtInputCtx)
{
    liot_at_result_enum_type result = LIOT_AT_OK;
    CHAR resp_buf[256]              = {0};

    uint8_t mode = 0;

    switch (pAtInputCtx->op)
    {
        case LIOT_AT_CMD_SET:
        {
            int mode_val = 0;
            sscanf(pAtInputCtx->arg, "%d", &mode_val);
            mode = (uint8_t)mode_val;
            app_nv_set_boot_mode(mode);
            app_nv_save();
            snprintf((char *)resp_buf, 256, "+LAITESTMODE: %d", app_nv_get_boot_mode());
            break;
        }
        case LIOT_AT_CMD_READ:
        { 
            snprintf((char *)resp_buf, 256, "+LAITESTMODE: %d", app_nv_get_boot_mode());
            break;
        }
        case LIOT_AT_CMD_TEST:
        case LIOT_AT_CMD_EXE:
        default:
            break;
    }
    result = liot_atcmd_reply(pAtInputCtx->atHandle, LIOT_AT_OK, resp_buf);
    return result;
}

const liot_atCommand liot_atcmd_tab[] = {
    LIOT_ATCMD("+LAITESTMODE", liot_ai_atcmd_LAITESTMODE)
};

int LIOT_GetCustomer_ATtable_Size(void)
{
    return sizeof(liot_atcmd_tab) / sizeof(liot_atcmd_tab[0]);
}

void app_atcmd_init(void *argv)
{
    //Retrieves the size of the AT command table using
    UINT32 cmd_tab_size = LIOT_GetCustomer_ATtable_Size();
    //egisters the AT command table using
    liot_atcmd_register((liot_atCommandP)&liot_atcmd_tab[0], cmd_tab_size);
    //Initializes the AT command module using
    liot_open_atcmd_init();
    liot_trace("app_atcmd_init_running...");
}
