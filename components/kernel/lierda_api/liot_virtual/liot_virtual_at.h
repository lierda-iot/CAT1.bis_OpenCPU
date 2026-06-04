#ifndef __LIOT_VIRTUAL_AT_H__
#define __LIOT_VIRTUAL_AT_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "liot_api_common.h"
#include "liot_type.h"

 /**
 * @brief URC (Unsolicited Result Code) callback function type.
 * @details This function type defines the signature for a callback that handles URC data received from the module.
 *          The callback will be invoked when a URC is received, allowing users to process asynchronous events such as network status updates.
 * @param[in] str Pointer to the URC character data.
 * @param[in] len Length of the URC character data.
 *
 * @return None.
 *
 * @code void URC_RecvData_CallBack(const uint8_t *str, uint32_t len) {
     // Process the received URC data here
 }
 *
 */
typedef void (*liot_urc_func)(const uint8_t *str, uint32_t len);

/**
 * @brief Send a virtual AT command and receive the response.
 * @details This function is used to send the specified AT command string, wait for the module to return the response data, 
 *          and supports setting a timeout period.
 * @param cmd Pointer to the address of the AT command string.
 * @param size Size of the AT command string (in bytes).
 * @param timeout Timeout period (in milliseconds); if no response is received within this time, the operation is considered failed.
 * @param rsp_buffer Pointer to the buffer for storing the AT command response data.
 *
 * @return liot_errcode_e.
 *         LIOT_SUCCESS: Successfully sent the AT command and received a valid response.
 *         Other: Operation failed, possible reasons include timeout or invalid parameters.
 * @code  uint8_t AtRespBuff[128]={0};
 *        liot_VirtualAtCmd("AT+CGATT?", strlen("AT+CGATT?"),3000,AtRespBuff);
 */
liot_errcode_e liot_VirtualAtCmd(const void *cmd, uint32_t size, uint32_t timeout, uint8_t *rsp_buffer);

/**
 * @brief Register a URC (Unsolicited Result Code) callback function.
 * @details This function is used to register a callback function that will be invoked when a URC is received from the module.
 *          The callback function will handle the URC data, allowing users to process asynchronous events such as network status updates.
 * @param[in] cb Pointer to the URC callback function. The function should match the signature defined by `liot_urc_func`.
 *
 * @return None.
 *
 * @code  liot_UrcCallbackRegister(URC_RecvData_CallBack);
 */
void liot_UrcCallbackRegister(liot_urc_func cb);

/**
 * @brief Unregister the URC (Unsolicited Result Code) callback function.
 * @details This function is used to unregister the previously registered URC callback function.
 *          After calling this function, the URC callback will no longer be invoked when a URC is received.
 *
 * @param[in] None.
 *
 * @return None.
 *
 * @code liot_UrcCallbackDeRegister();
 * 
 */
void liot_UrcCallbackDeRegister(void);

#ifdef __cplusplus
}
#endif

#endif //__LIOT_VIRTUAL_AT_H__