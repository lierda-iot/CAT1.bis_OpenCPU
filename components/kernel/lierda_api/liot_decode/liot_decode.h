/**
 * @file liot_decode
 * @brief LIoT protocol decoding module header file
 * @author L ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef _LIOT_DECODE_H_
#define _LIOT_DECODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_type.h"
#include "liot_api_common.h"

/**
 * @brief Authentication key length
 */
#define LIOT_DECODE_AUTH_KEY_LEN (256)

/**
 * @brief Decoder error code enumeration
 */
typedef enum
{
    LIOT_DECODER_SUCCESS = LIOT_SUCCESS,               /**< Success */
    LIOT_DECODER_INIT_ERR = 0x20|LIOT_COMPONENT_STATE_INFO,  /**< Initialization error */
    LIOT_DECODER_ERR,                                  /**< General decoding error */
    LIOT_DECODER_GET_RESULT_ERR,                       /**< Result retrieval error */
    LIOT_DECODER_GET_RESULT_LENGTH_ERR,                /**< Result length error */
    LIOT_DECODER_DESTROY_ERR                           /**< Decoder destruction error */
} liot_errcode_decoder_e;

/**
 * @brief Decoder type enumeration
 */
typedef enum
{
    LIOT_DECODER_TYPE_QY = 0,  /**< Qingyun decoder type */
    LIOT_DECODER_TYPE_MAX = 0xff  /**< Maximum decoder type value */
} liot_decoder_type_e;

/**
 * @brief Set decoder authentication key
 * @param[in] buff Pointer to key buffer
 * @param[in] bufflen Length of key buffer
 * @return Decoding error code
 * @retval LIOT_DECODER_SUCCESS Success
 * @retval LIOT_DECODER_ERR Invalid parameters or setting failed
 */
liot_errcode_decoder_e liot_decoder_set_auth_key(char *buff, uint16_t bufflen);

/**
 * @brief Initialize decoder
 * @param[in] type Decoder type
 * @return Decoding error code
 * @retval LIOT_DECODER_SUCCESS Success
 * @retval LIOT_DECODER_INIT_ERR Initialization failed
 */
liot_errcode_decoder_e liot_decoder_init(liot_decoder_type_e type);

/**
 * @brief Destroy decoder resources
 * @return Decoding error code
 * @retval LIOT_DECODER_SUCCESS Success
 * @retval LIOT_DECODER_DESTROY_ERR Destruction failed
 */
liot_errcode_decoder_e liot_destroy_decoder(void);

/**
 * @brief Image decoding process
 * @param[in] img Pointer to image data
 * @param[in] width Image width
 * @param[in] height Image height
 * @return Decoding error code
 * @retval LIOT_DECODER_SUCCESS Success
 * @retval LIOT_DECODER_ERR Decoding failed
 */
liot_errcode_decoder_e liot_image_decoder(unsigned char *img, int width, int height);

/**
 * @brief Get decoder version information
 * @param[out] version Pointer to version information buffer
 * @return Decoding error code
 * @retval LIOT_DECODER_SUCCESS Success
 * @retval LIOT_DECODER_ERR Retrieval failed
 */
liot_errcode_decoder_e liot_get_decoder_version(char* version);

/**
 * @brief Get decoding result
 * @param[out] result Pointer to result buffer
 * @return Result length, less than 0 indicates failure
 */
int liot_get_decoder_result(unsigned char *result);

#ifdef __cplusplus
}
#endif

#endif
