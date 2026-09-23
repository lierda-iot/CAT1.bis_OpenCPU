/**
 * @file liot_jpeg.h
 * @brief LIoT JPEG Encode/Decode Interface
 *
 * This header file provides the application programming interface (API) for
 * JPEG encoding and decoding on the LIoT platform. The API is intentionally
 * buffer based so that camera and storage applications can use it without
 * handing ownership of their buffers to the codec. It includes enumerations
 * for error codes and pixel formats, a structure describing JPEG dimensions,
 * and function prototypes for reading JPEG info, decoding, and encoding.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-09-20
 * @version 1.0
 */
#ifndef LIOT_JPEG_H
#define LIOT_JPEG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Enum
 ===========================================================================*/
/**
 * @enum liot_errcode_jpeg_e
 * @brief Enumeration type for JPEG module error codes
 */
typedef enum
{
    LIOT_JPEG_SUCCESS = 0,             /**< Operation succeeded */
    LIOT_JPEG_INVALID_PARAM = -1,      /**< Invalid parameter or NULL pointer */
    LIOT_JPEG_NO_MEMORY = -2,          /**< Memory allocation failed */
    LIOT_JPEG_BUFFER_TOO_SMALL = -3,   /**< Output buffer capacity too small */
    LIOT_JPEG_CODEC_ERROR = -4,        /**< Underlying codec reported error */
    LIOT_JPEG_UNSUPPORTED_FORMAT = -5, /**< Requested format unsupported */
} liot_errcode_jpeg_e;

/**
 * @enum liot_jpeg_enc_fmt_e
 * @brief Enumeration type for JPEG encoder input pixel formats
 */
typedef enum
{
    LIOT_JPEG_ENC_FMT_YUV422 = 0, /**< Input is packed YUYV, encoded as 4:2:2 */
    LIOT_JPEG_ENC_FMT_YUV420,     /**< 4:2:0 input; NOT supported by codec */
    LIOT_JPEG_ENC_FMT_Y,          /**< Input is grayscale, one byte per pixel */
    LIOT_JPEG_ENC_FMT_RGB565,     /**< Input is RGB565, converted to YUYV */
    LIOT_JPEG_ENC_FMT_MAX,        /**< Guard value, not a valid format */
} liot_jpeg_enc_fmt_e;

/**
 * @enum liot_jpeg_dec_fmt_e
 * @brief Enumeration type for JPEG decoder output pixel formats
 */
typedef enum
{
    LIOT_JPEG_DEC_FMT_RGB565 = 0, /**< Output is RGB565, two bytes per pixel */
    LIOT_JPEG_DEC_FMT_Y,          /**< Grayscale out; NOT supported by codec */
    LIOT_JPEG_DEC_FMT_MAX,        /**< Guard value, not a valid format */
} liot_jpeg_dec_fmt_e;

/*===========================================================================
 * Struct
 ===========================================================================*/
/**
 * @struct liot_jpeg_info_t
 * @brief Dimensions parsed from a JPEG bitstream
 */
typedef struct
{
    uint32_t width;  /**< Image width in pixels */
    uint32_t height; /**< Image height in pixels */
} liot_jpeg_info_t;

/*===========================================================================
 * Function
 ===========================================================================*/
/**
 * @brief Read the dimensions stored in a JPEG bitstream
 *
 * Parses the JPEG header only, without decoding pixel data, and returns the
 * image width and height.
 *
 * @param[in]  jpeg_data  Pointer to the JPEG bitstream
 * @param[in]  jpeg_size  Size of the JPEG bitstream in bytes
 * @param[out] info       Pointer to store the parsed dimensions
 *
 * @return @ref liot_errcode_jpeg_e indicating success or failure
 * @retval LIOT_JPEG_SUCCESS        Dimensions parsed successfully
 * @retval LIOT_JPEG_INVALID_PARAM  NULL pointer or zero jpeg_size
 * @retval LIOT_JPEG_CODEC_ERROR    Failed to parse the JPEG header
 */
liot_errcode_jpeg_e liot_jpeg_get_info(const uint8_t *jpeg_data,
                                       uint32_t jpeg_size,
                                       liot_jpeg_info_t *info);

/**
 * @brief Decode a JPEG bitstream into the requested raw pixel format
 *
 * On input, @p out_size holds the capacity of @p out_data. On success it is
 * replaced with the number of bytes written, and @p width / @p height contain
 * the decoded image dimensions.
 *
 * @param[in]     jpeg_data  Pointer to the JPEG bitstream
 * @param[in]     jpeg_size  Size of the JPEG bitstream in bytes
 * @param[out]    out_data   Buffer that receives the decoded pixels
 * @param[in,out] out_size   In: capacity of @p out_data; Out: bytes written
 * @param[out]    width      Pointer to store the decoded image width
 * @param[out]    height     Pointer to store the decoded image height
 * @param[in]     fmt        Output pixel format (@ref liot_jpeg_dec_fmt_e)
 *
 * @return @ref liot_errcode_jpeg_e indicating success or failure
 * @retval LIOT_JPEG_SUCCESS            Decode completed successfully
 * @retval LIOT_JPEG_INVALID_PARAM      NULL pointer or zero jpeg_size
 * @retval LIOT_JPEG_UNSUPPORTED_FORMAT Output format not supported
 * @retval LIOT_JPEG_BUFFER_TOO_SMALL   out_data too small for the image
 * @retval LIOT_JPEG_CODEC_ERROR        Underlying codec failed
 */
liot_errcode_jpeg_e liot_jpeg_decode(const uint8_t *jpeg_data,
                                     uint32_t jpeg_size, uint8_t *out_data,
                                     uint32_t *out_size, uint32_t *width,
                                     uint32_t *height,
                                     liot_jpeg_dec_fmt_e fmt);

/**
 * @brief Encode raw image data into a JPEG bitstream
 *
 * On input, @p jpeg_size holds the capacity of @p jpeg_data. On success it is
 * replaced with the encoded size. A @p quality of zero or greater than 100
 * falls back to the default quality (80).
 *
 * @param[in]     raw_data   Pointer to the raw image data
 * @param[in]     raw_size   Size of the raw image data in bytes
 * @param[in]     width      Image width in pixels
 * @param[in]     height     Image height in pixels
 * @param[in]     quality    Encode quality 1..100, or 0 for the default
 * @param[in]     fmt        Input pixel format (@ref liot_jpeg_enc_fmt_e)
 * @param[out]    jpeg_data  Buffer that receives the JPEG bitstream
 * @param[in,out] jpeg_size  In: capacity of @p jpeg_data; Out: encoded size
 *
 * @return @ref liot_errcode_jpeg_e indicating success or failure
 * @retval LIOT_JPEG_SUCCESS            Encode completed successfully
 * @retval LIOT_JPEG_INVALID_PARAM      NULL, zero size, or raw too small
 * @retval LIOT_JPEG_UNSUPPORTED_FORMAT Input format not supported
 * @retval LIOT_JPEG_NO_MEMORY          Temporary conversion buffer alloc failed
 * @retval LIOT_JPEG_BUFFER_TOO_SMALL   jpeg_data too small for the output
 * @retval LIOT_JPEG_CODEC_ERROR        Underlying codec failed
 */
liot_errcode_jpeg_e liot_jpeg_encode(const uint8_t *raw_data,
                                     uint32_t raw_size, uint32_t width,
                                     uint32_t height, uint8_t quality,
                                     liot_jpeg_enc_fmt_e fmt,
                                     uint8_t *jpeg_data, uint32_t *jpeg_size);

/* Camel-case aliases match the naming used by the other LIoT HAL APIs. */

/**
 * @brief Camel-case alias of @ref liot_jpeg_get_info
 * @see liot_jpeg_get_info
 */
liot_errcode_jpeg_e Liot_JpegGetInfo(const uint8_t *jpeg_data,
                                     uint32_t jpeg_size,
                                     liot_jpeg_info_t *info);

/**
 * @brief Camel-case alias of @ref liot_jpeg_decode
 * @see liot_jpeg_decode
 */
liot_errcode_jpeg_e Liot_JpegDecode(const uint8_t *jpeg_data,
                                    uint32_t jpeg_size, uint8_t *out_data,
                                    uint32_t *out_size, uint32_t *width,
                                    uint32_t *height,
                                    liot_jpeg_dec_fmt_e fmt);

/**
 * @brief Camel-case alias of @ref liot_jpeg_encode
 * @see liot_jpeg_encode
 */
liot_errcode_jpeg_e Liot_JpegEncode(const uint8_t *raw_data,
                                    uint32_t raw_size, uint32_t width,
                                    uint32_t height, uint8_t quality,
                                    liot_jpeg_enc_fmt_e fmt,
                                    uint8_t *jpeg_data, uint32_t *jpeg_size);

#ifdef __cplusplus
}
#endif

#endif /* LIOT_JPEG_H */
