#ifndef KEY_MODULE_H
#define KEY_MODULE_H

typedef enum {
    EVT_KEY_POWER_ON = 0,
    EVT_KEY_POWER_OFF,
    EVT_KEY_MAX,
}keyEvt_E;

void keyModuleInit(void);
void keyModuleDeinit(void);

#endif /* KEY_MODULE_H */
