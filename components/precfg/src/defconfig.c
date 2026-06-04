
#include "liot_type.h"
#include "lierda_defcfg.h"
#include "defaultini.h"

#define __L_CFG_MAP_ __attribute__((section(".cfgmap")))

#ifndef __INICFG_FAULTACTION__
#define __INICFG_FAULTACTION__ 4
#endif

#ifndef __INICFG_PWRKEYMODE__
#define __INICFG_PWRKEYMODE__ 0
#endif

#ifndef __INICFG_LOGCONTROL__
#define __INICFG_LOGCONTROL__ 2
#endif

#ifndef __INICFG_LOGPORTSEL__
#define __INICFG_LOGPORTSEL__ 0
#endif

#ifndef __INICFG_USBCTRL__
#define __INICFG_USBCTRL__ 0
#endif

#ifndef __INICFG_USBNET__
#define __INICFG_USBNET__ 0
#endif

#ifndef __INICFG_USBSLPMASK__
#define __INICFG_USBSLPMASK__ 0
#endif

#ifndef __INICFG_USBVBUSMODEEN__
#define __INICFG_USBVBUSMODEEN__ 1
#endif

#ifndef __INICFG_VBUSWKUPPAD__
#define __INICFG_VBUSWKUPPAD__ 1
#endif

#ifndef __INICFG_NETCID__
#define __INICFG_NETCID__ 0
#endif

#ifndef __INICFG_AUTODIAL__
#define __INICFG_AUTODIAL__ 0
#endif

#ifndef __INICFG_NETDEVCTLURC__
#define __INICFG_NETDEVCTLURC__ 0
#endif

#ifndef __INICFG_NATENABLE__
#define __INICFG_NATENABLE__ 0
#endif

#ifndef __INICFG_CFUN__
#define __INICFG_CFUN__ 1
#endif

#ifndef __INICFG_AUTOAPN__
#define __INICFG_AUTOAPN__ 0
#endif

#ifndef __INICFG_UART1AT__
#define __INICFG_UART1AT__ 0
#endif

#ifndef __INICFG_UART2AT__
#define __INICFG_UART2AT__ 0
#endif

#ifndef __INICFG_ATECHO__
#define __INICFG_ATECHO__ 0
#endif

#ifndef __INICFG_APPLOGPORT__
#define __INICFG_APPLOGPORT__ 0
#endif

#ifndef __INICFG_UARTBOOTLOADER__
#define __INICFG_UARTBOOTLOADER__ 115200
#endif

#ifndef __INICFG_DATACONT__
#define __INICFG_DATACONT__ 0
#endif

#ifndef __INICFG_IPTYPE__
#define __INICFG_IPTYPE__ 0
#endif

#ifndef __INICFG_PWRKEYDELAY__
#define __INICFG_PWRKEYDELAY__ 50
#endif

#if defined __INICFG_APNIPTYPE__ || defined __INICFG_APN__ || defined __INICFG_USERNAME__ || defined __INICFG_PWD__
__L_CFG_MAP_ app_apncfg_t apncfg =
{
    #ifndef __INICFG_APNIPTYPE__
    .apniptype = 0,
    #else
    .apniptype = __INICFG_APNIPTYPE__,
    #endif
    #ifndef __INICFG_APN__
    .apn = {0},
    #else
    .apn = __INICFG_APN__,
    #endif
    #ifndef __INICFG_USERNAME__
    .userName = {0},
    #else
    .userName = __INICFG_USERNAME__,
    #endif
    #ifndef __INICFG_PWD__
    .pwd = {0},
    #else
    .pwd = __INICFG_PWD__,
    #endif
};
#define __INICFG_PDPADDR__ (uint32_t)&apncfg
#else
#define __INICFG_PDPADDR__ 0
#endif

#if defined __INICFG_DATACOUNT__ || defined __INICFG_SIMSLOT__
__L_CFG_MAP_ app_pscfg_t pscfg =
{
    #ifndef __INICFG_DATACOUNT__
    .datacount = 0,
    #else
    .datacount = __INICFG_DATACOUNT__,
    #endif
    #ifndef __INICFG_SIMSLOT__
    .simslot = 0,
    #else
    .simslot = __INICFG_SIMSLOT__,
    #endif
};
#define __INICFG_PSCFG_ADDR__ (uint32_t)&pscfg
#else
#define __INICFG_PSCFG_ADDR__ 0
#endif

__L_CFG_MAP_ appcfg_t lsdk_cfg =
{
    /*PaltConfig*/
    .faultAction = __INICFG_FAULTACTION__,
    .pwrKeyMode = __INICFG_PWRKEYMODE__,
    .logControl = __INICFG_LOGCONTROL__,
    .logPortSel = __INICFG_LOGPORTSEL__,
    .usbCtrl = __INICFG_USBCTRL__,
    .usbNet = __INICFG_USBNET__,
    .usbSlpMask = __INICFG_USBSLPMASK__,
    .usbVBUSModeEn = __INICFG_USBVBUSMODEEN__,
    .usbVBUSWkupPad = __INICFG_USBVBUSMODEEN__,
    
    /* NetCfg */
    .netcid = __INICFG_NETCID__,
    .AutoDial = __INICFG_AUTODIAL__,
    .NetDevCtlUrc = __INICFG_NETDEVCTLURC__,
    .NatEnable = __INICFG_NATENABLE__,

    /*Mics*/
    .cfun = __INICFG_CFUN__,
    .AutoApn = __INICFG_AUTOAPN__,
    .dataCont = __INICFG_DATACONT__,

    /*AT*/
    .uart1At = __INICFG_UART1AT__,
    .uart2At = __INICFG_UART2AT__,
    .atEcho = __INICFG_ATECHO__,
    .appLogPort = __INICFG_APPLOGPORT__,
    .uartbootloader = __INICFG_UARTBOOTLOADER__,

    /*IP Type:32bit*/
    .iptype = __INICFG_IPTYPE__,
    .pwrkeyDelay = __INICFG_PWRKEYDELAY__,
    .pdpcfg_addr = __INICFG_PDPADDR__,
    .pscfg_addr = __INICFG_PSCFG_ADDR__,
};