/****************************************************************************
 *
 * Copyright (c) 2017–2025 EigenComm Ltd.
 * File name:    tgt_app.h
 * Description:  提供tgt app的调用接口
 * History:      Rev1.0   2025-11-19
 *
 ****************************************************************************/

void tgt_app_service_init(void);

typedef void (*softSimEvNotifyCb)(int event, char* param, int param_len);
int softSimEvNotifyRegisterCallback(softSimEvNotifyCb ev_cb);
