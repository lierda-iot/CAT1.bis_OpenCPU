/**
 * @file liot_router_modules.h
 * @brief Registration entry declarations for the Web API modules
 *
 * @details Extension point: when adding a module, add one register declaration
 *          here and call it from the web entry Liot_WebInit(). Modules are
 *          fully decoupled from each other.
 */

#ifndef __API_MODULES_H__
#define __API_MODULES_H__

#ifdef __cplusplus
extern "C" {
#endif

void Liot_ApiAuthRegister(void);     /**< Login / logout / change password */
void Liot_ApiSysinfoRegister(void);  /**< Device info / system status */
void Liot_ApiNetworkRegister(void);  /**< LAN link / network parameter view */
void Liot_ApiLanRegister(void);      /**< Gateway / DHCP pool / static IP config */
void Liot_ApiSystemRegister(void);   /**< Reboot / factory reset / firmware upgrade */

/* Reserved for future modules (4G CAT1 wired router):
 * void Liot_ApiCellularRegister(void);  APN / network mode / dial-up
 * void Liot_ApiSimRegister(void);       SIM / PIN
 * void Liot_ApiTrafficRegister(void);   traffic statistics and quota
 * void Liot_ApiSmsRegister(void);       SMS
 * void Liot_ApiAclRegister(void);       MAC filter / access control
 * void Liot_ApiRouteRegister(void);     static routes
 * void Liot_ApiDiagRegister(void);      ping / traceroute diagnostics
 */

#ifdef __cplusplus
}
#endif

#endif /* __API_MODULES_H__ */
