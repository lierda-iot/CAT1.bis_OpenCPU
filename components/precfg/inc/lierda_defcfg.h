#ifndef __LIOT_DEFCFG_H__
#define __LIOT_DEFCFG_H__

typedef struct 
{
    /*pdp auth, only support cid = 1*/
    uint8_t apniptype;
    uint8_t authType;
    uint8_t apn[100];
    uint8_t userName[64];
    uint8_t pwd[64];
} app_apncfg_t;

typedef struct
{
    uint8_t datacount;
    uint8_t simslot;
} app_pscfg_t;

typedef struct
{
    /*PaltConfig :20bit*/
    uint32_t faultAction :3;
    uint32_t pwrKeyMode :3;
    uint32_t logControl :2;
    uint32_t logPortSel :2;
    uint32_t usbCtrl :2;
    uint32_t usbNet :3;
    uint32_t usbSlpMask :1;
    uint32_t usbVBUSModeEn :1;
    uint32_t usbVBUSWkupPad :3;

    /* NetCfg :8bit*/
    uint32_t netcid :5;
    uint32_t AutoDial :1;
    uint32_t NetDevCtlUrc :1;
    uint32_t NatEnable :1;

    /*Mics :4bit*/
    uint32_t cfun :1;
    uint32_t AutoApn :1;
    uint32_t dataCont :1;
    uint32_t reservedMisc :1;

    /*AT :32bit*/
    uint32_t uart1At :1;
    uint32_t uart2At :1;
    uint32_t atEcho :1;
    uint32_t appLogPort:2;
    uint32_t uartbootloader :24;
    uint32_t reservedAt :3;

    /*IP Type:32bit*/
    uint32_t iptype :3;         /* 1-IP_V4, 2-IP_V6, 3-IP_V4V6, 4-UNUSED, 5-NONIP */
    uint32_t pwrkeyDelay :16;   /* powerkey Debounce */
    uint32_t reserved :13;

    /*PDP Config :32bit*/
    uint32_t pdpcfg_addr;
    uint32_t pscfg_addr;
} appcfg_t;

#endif