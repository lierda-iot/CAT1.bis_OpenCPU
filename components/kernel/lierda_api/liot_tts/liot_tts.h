/**
 * @File Name: liot_tts.h
 * @brief
 * @Author : Chenhz
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef _LIOT_TTS_H_
#define _LIOT_TTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_osi_def.h"
#include "liot_type.h"



/*========================================================================
 *	Enum
 *========================================================================*/




/**
 * @brief TTS error code enumeration definition
 * 
 * Defines the error codes used by the TTS module
 */
typedef enum
{
    LIOT_TTS_SUCCESS               = LIOT_SUCCESS,                    /*!< Operation successful */
    LIOT_TTS_UNKNOWN_ERROR         = 901 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Unknown error */
    LIOT_TTS_INVALID_PARAM         = 902 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Invalid parameter */
    LIOT_TTS_OPERATION_NOT_SUPPORT = 903 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Operation not supported */
    LIOT_TTS_DEVICE_BUSY           = 904 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Device busy */
    LIOT_TTS_INIT_ENGINE_ERR       = 2001 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Engine initialization error */
    LIOT_TTS_INIT_SOURCE_ERR       = 2002 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Resource initialization error */
    LIOT_TTS_START_ERR             = 2003 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Start error */
    LIOT_TTS_STOP_ERR              = 2004 | (LIOT_COMPONENT_AUDIO_TTS << 16), /*!< Stop error */
    LIOT_TTS_EXIT_ERR              = 2005 | (LIOT_COMPONENT_AUDIO_TTS << 16)  /*!< Exit error */
} liot_tts_errcode_e;

/**
 * @brief TTS configuration parameter enumeration definition
 * 
 * Defines the configuration parameter types supported by TTS
 */
typedef enum
{
    LIOT_TTS_CONFIG_SPEED = 1,  /*!< Speech speed configuration */
    LIOT_TTS_CONFIG_VOLUME,     /*!< Volume configuration */
    LIOT_TTS_CONFIG_ENCODING,   /*!< Encoding format configuration */
    LIOT_TTS_CONFIG_ROLE,       /*!< Play character configuration */
    LIOT_TTS_CONFIG_READ_DIGIT, /*!< Read digit configuration */
    LIOT_TTS_CONFIG_MAX         /*!< Maximum value of configuration parameters */
} liot_tts_config_e;

/**
 * @brief TTS encoding format enumeration definition
 * 
 * Defines the text encoding formats supported by TTS
 */
typedef enum
{
    LIOT_TTS_GBK = 0,  /*!< GBK encoding format */
    LIOT_TTS_UTF8,     /*!< UTF-8 encoding format */
    LIOT_TTS_UCS2      /*!< UCS-2 encoding format */
} liot_tts_encoding_e;

/**
 * @brief TTS output callback function
 * 
 * This callback function is called when the TTS engine generates audio data, 
 * used to process and pass the synthesized audio data.
 * 
 * @param[in] context User context pointer, consistent with the callback context passed during initialization
 * @param[in] msg Message type (reserved parameter)
 * @param[in] ds Data status (reserved parameter)
 * @param[in] param2 Additional parameter 2 (reserved parameter)
 * @param[in] dSize Audio data size (bytes)
 * @param[in] dBuffer Audio data buffer pointer
 * 
 * @return int Returns processing result, usually returning 0 indicates successful processing
 * 
 * @note This callback function is called internally by the TTS engine. Users need to implement 
 *       this function to handle audio output data. Users only need to focus on the dBuffer and dSize parameters.
 */
typedef int (*liot_pUserCallback)(void *context, int msg, int ds, int param2, int dSize, const void *dBuffer);

/**
 * @brief TTS resource read callback function type definition
 * 
 * This callback function is used by the TTS engine to read the required resource data.
 * The resource data is set through the liot_tts_set_resource interface.
 * Users need to implement the specific resource reading logic.
 * 
 * @param[in] pParameter User-provided parameter pointer, usually pointing to the start address of resource data
 * @param[out] pBuffer Buffer to store the read resource data
 * @param[in] iPos Starting position of the resource data to be read
 * @param[in] nSize Size of the resource data to be read
 * 
 * @note This callback function is called internally by the TTS engine.
 *       The resource data set by liot_tts_set_resource interface is the data required by this callback.
 *       Users need to implement the data reading logic according to the actual resource storage method.
 */
typedef bool (*liot_read_res_cb)(void *pParameter, void *pBuffer, uint32_t iPos, uint32_t nSize);

/**
 * @brief Initialize TTS engine
 * 
 * This function initializes the Text-to-Speech (TTS) engine, including memory allocation, 
 * creating TTS instance, setting various parameters (callback functions, language, volume, 
 * speech speed, voice role, etc.) and getting TTS library version information.
 * 
 * @param[in] mCallback User callback function for TTS output processing
 * 
 * @return liot_tts_errcode_e Return error code
 *         - LIOT_TTS_SUCCESS: Initialization successful
 *         - LIOT_TTS_INVALID_PARAM: Invalid input parameter
 *         - LIOT_TTS_INIT_SOURCE_ERR: Resource not set
 *         - LIOT_TTS_INIT_ENGINE_ERR: Engine initialization failed
 * 
 * @note TTS resources need to be set through liot_tts_set_resource before calling this function.
 *       Usually paired with liot_tts_exit, internal allocated resources need to be released 
 *       through that interface.
 * 
 * @see liot_tts_set_resource
 * @see liot_tts_exit
 */
liot_tts_errcode_e liot_tts_engine_init(liot_pUserCallback mCallback);

/**
 * @brief Start TTS text synthesis
 * 
 * This function starts the text-to-speech synthesis process, converting the input text string 
 * into audio data and outputting it through the liot_pUserCallback callback function.
 * 
 * @param[in] textString Pointer to the text string to be synthesized
 * @param[in] textLen Length of the text string to be synthesized
 * 
 * @return liot_tts_errcode_e Return error code
 *         - LIOT_TTS_SUCCESS: Text synthesis started successfully
 *         - LIOT_TTS_INVALID_PARAM: Invalid input parameter
 *         - LIOT_TTS_START_ERR: Text synthesis failed to start
 * 
 * @note The text encoding format should match the encoding format set by liot_tts_set_config_param.
 *       The synthesized audio data will be output through the callback function registered in liot_tts_engine_init.
 *       This function is synchronous and will only return after text parsing is complete.
 * 
 * @see liot_tts_engine_init
 * @see liot_tts_set_config_param
 */
liot_tts_errcode_e liot_tts_start(const char *textString, unsigned int textLen);

/**
 * @brief Set TTS configuration parameters
 * 
 * This function is used to set the configuration parameters of the TTS engine, including speech speed, volume, and encoding format.
 * 
 * @param[in] type Configuration parameter type, refer to liot_tts_config_e enumeration definition
 * @param[in] value The parameter value to be set
 *              - Speech speed parameter range: -32768 to 32767
 *              - Volume parameter range: -32768 to 32767
 *              - Encoding format parameter: see liot_tts_encoding_e
 * 
 * @return liot_tts_errcode_e Return error code
 *         - LIOT_TTS_SUCCESS: Parameter setting successful
 *         - LIOT_TTS_INVALID_PARAM: Invalid input parameter
 *         - LIOT_TTS_INIT_SOURCE_ERR: Parameter setting failed
 * 
 * @note This function should be called after liot_tts_engine_init initialization is successful
 * 
 * @see liot_tts_config_e
 * @see liot_tts_get_config_param
 */
liot_tts_errcode_e liot_tts_set_config_param(liot_tts_config_e type, int value);

/**
 * @brief Get TTS configuration parameters
 * 
 * This function is used to get the current configuration parameters of the TTS engine, 
 * including speech speed, volume, and encoding format.
 * 
 * @param[in] type Configuration parameter type, refer to liot_tts_config_e enumeration definition
 * 
 * @return int Returns the current parameter value
 *         - For speed and volume parameters: returns the current value
 *         - For encoding format parameter: returns the encoding type value (see liot_tts_encoding_e)
 *         - Error: returns LIOT_TTS_INVALID_PARAM or LIOT_TTS_UNKNOWN_ERROR
 * 
 * @note This function should be called after liot_tts_engine_init initialization is successful
 * 
 * @see liot_tts_config_e
 * @see liot_tts_set_config_param
 */
int liot_tts_get_config_param(liot_tts_config_e type);

/**
 * @brief Check if TTS engine is running
 * 
 * This function checks whether the TTS engine is currently running.
 * 
 * @return int Returns the running status
 *         - 1: TTS engine is running
 *         - 0: TTS engine is not running
 * 
 * @note This function can be used to determine if the TTS engine has been initialized 
 *       and is ready to synthesize text.
 * 
 * @see liot_tts_engine_init
 */
int liot_tts_is_running(void);

/**
 * @brief Convert UTF-8 string to GBK encoding
 * 
 * This function converts a UTF-8 encoded string to GBK encoding format.
 * 
 * @param[in] utf8 Pointer to the input UTF-8 encoded string
 * @param[in] inputlen Length of the input UTF-8 string
 * @param[out] outputlen Pointer to store the length of the converted GBK string
 * @param[out] gbk Pointer to the buffer to store the converted GBK encoded string
 * 
 * @note The output buffer (gbk) should be large enough to hold the converted string.
 *       The maximum size of the output buffer should be considered based on the input string length.
 * 
 * @see liot_tts_set_config_param
 */
void liot_utf8_to_gbk_str(void *utf8, int inputlen, int *outputlen, void *gbk);

/**
 * @brief End TTS text synthesis
 * 
 * This function is used to end the current TTS text synthesis process and release related resources.
 * 
 * @return liot_tts_errcode_e Return error code
 *         - LIOT_TTS_SUCCESS: Text synthesis ended successfully
 *         - LIOT_TTS_START_ERR: Text synthesis ending failed
 * 
 * @note This function should be called after liot_tts_start text synthesis is completed to ensure proper release of TTS engine resources
 * 
 * @see liot_tts_start
 */
liot_tts_errcode_e liot_tts_end(void);

/**
 * @brief Release TTS engine resources
 * 
 * This function is used to release the resources occupied by the TTS engine, 
 * including freeing the memory heap and destroying the TTS instance.
 * This function should be called when the TTS function is no longer needed to avoid memory leaks.
 * 
 * @return liot_tts_errcode_e Return error code
 *         - LIOT_TTS_SUCCESS: Resource released successfully
 *         - LIOT_TTS_UNKNOWN_ERROR: Resource release failed
 * 
 * @note Internal allocated resources need to be released through this interface. 
 *       This function should be called after all TTS operations are completed and 
 *       the TTS function is no longer needed. Usually paired with liot_tts_engine_init.
 * 
 * @see liot_tts_engine_init
 */
liot_tts_errcode_e liot_tts_exit(void);

/**
 * @brief Set TTS resource data
 * 
 * This function sets the voice resource data required by the TTS engine.
 * 
 * @param[in] pParameter Pointer to the resource data, usually the starting address of the resource data,
 *                       supports external flash and internal file system methods.
 * @param[in] rd_res_cb Callback function for reading resource data
 * 
 * @note The resource data set by this interface is the data required for TTS engine operation.
 *       Users need to ensure the validity and integrity of the resource data.
 * 
 * @see liot_read_res_cb
 */
void liot_tts_set_resource(void *pParameter, liot_read_res_cb rd_res_cb);

/**
 * @brief Set TTS cache parameters
 * 
 * This function is used to set the cache parameters for the TTS engine.
 * 
 * @param[in] cache_para Cache parameter value to be set
 * 
 * @return liot_tts_errcode_e Return error code
 *         - LIOT_TTS_SUCCESS: Cache parameter setting successful
 *         - Other error codes: Cache parameter setting failed
 * 
 * @note This function is currently not implemented and will return an error code indicating
 *       that the operation is not supported.
 * 
 * @warning This function is not yet implemented and should not be used in production code.
 */
liot_tts_errcode_e liot_tts_setcache(int32_t cache_para);

/**
 * @brief Get TTS resource data pointer
 * This function retrieves the pointer to the TTS resource data.
 * @return char* Returns the pointer to the TTS resource data
 * @note This function can be used to obtain the current TTS resource data pointer for further processing or analysis.
 */
char * liot_get_tts_resource (void);

#ifdef __cplusplus
} /*"C" */
#endif

#endif
