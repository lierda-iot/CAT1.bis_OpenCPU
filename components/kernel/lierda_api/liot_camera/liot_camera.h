/**
 * @file liot_camera.h
 * @brief Camera module API header file
 * @details Defines interfaces and data structures for camera initialization, configuration, image capture and other operations
 * @author L ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef _LIOT_CAMERA_H_
#define _LIOT_CAMERA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_type.h"
#include "liot_api_common.h"

#include "liot_i2c.h"
#include "liot_cspi.h"

/**
 * @brief Camera device declaration macro
 * @param camDev Camera device structure variable name
 */
#define LIOT_ADD_CAMERA(camDev)    extern liot_camera_sensor_t camDev

/**
 * @brief Camera module error code enumeration
 * @details Defines various error status codes that may be returned during camera operations
 */
typedef enum {
    LIOT_CAMERA_SUCCESS = LIOT_SUCCESS,                ///< Operation successful
    LIOT_CAMERA_INIT_ERR = (0x10) | LIOT_COMPONENT_STATE_INFO, ///< Initialization error
    LIOT_CAMERA_POWER_ON_ERR,                          ///< Power on error
    LIOT_CAMERA_CLOSE_ERR,                             ///< Close error
    LIOT_CAMERA_PREVIEW_ERR,                           ///< Preview error
    LIOT_CAMERA_STOP_PREVIEW_ERR,                      ///< Stop preview error
    LIOT_CAMERA_CAPTURE_ERR,                           ///< Image capture error
    LIOT_CAMERA_CAPTURE_TIMEOUT,                       ///< Image capture timeout
    LIOT_CAMERA_GET_INFO_ERR,                          ///< Get information error
    LIOT_CAMERA_PRINT_ERR,                             ///< Print error
    LIOT_CAMERA_BUF_ERR,                               ///< Buffer error
    LIOT_CAMERA_SET_BUF_ERR,                           ///< Set buffer error
} liot_errcode_camera_e;

/**
 * @brief Camera device handle type
 * @details Opaque pointer used to identify and operate camera devices
 */
typedef void * liot_camera_handle_t;

typedef struct
{
    uint8_t regAddr;                            ///< Sensor I2C register address
    uint8_t regVal;                             ///< Sensor I2C register value
} liot_camI2cCfg_t;

typedef enum
{
    LIOT_CAM_LSB_MODE    = 0,                            ///< Little endian
    LIOT_CAM_MSB_MODE    = 1,                            ///< Big endian
} liot_endianMode_e;

typedef enum
{
    LIOT_WIRE_1      = 0,                            ///< 1 wire
    LIOT_WIRE_2      = 1,                            ///< 2 wire
} liot_wireNum_e;


typedef enum
{
    LIOT_SEQ_0       = 0,                            ///< rxd[0] 6 4 2 0
                                                ///< rxd[1] 7 5 3 1    
    LIOT_SEQ_1       = 1,                            ///< rxd[1] 6 4 2 0
                                                ///< rxd[0] 7 5 3 1
} liot_rxSeq_e;

typedef struct
{
    liot_endianMode_e    endianMode;                 ///< Endian mode
    liot_wireNum_e       wireNum;                    ///< Wire numbers
    liot_rxSeq_e         rxSeq;                      ///< Bit sequence in 2 wire mode
    uint8_t 		     cpol;
    uint8_t			     cpha;
    uint8_t              ddrMode;
    uint8_t              wordIdSeq;
	uint8_t              yOnly;
    uint8_t              rowScaleRatio;
    uint8_t              colScaleRatio;
    uint8_t              scaleBytes;
    uint8_t              dummyAllowed;
} liot_camParamCfg_t;

/**
 * @brief Camera output format enumeration
 * @details Defines image output formats supported by the camera
 */
typedef enum
{
    LIOT_CAMERA_OUTPUT_GRAY = 0,    ///< 256-level grayscale color format, derived from Y component of YUYV by the module
    LIOT_CAMERA_OUTPUT_YUYV,        ///< YUYV color format
    LIOT_CAMERA_OUTPUT_RGB565,      ///< RGB565 color format
    // LIOT_CAMERA_OUTPUT_RAW,       ///< RAW format (not enabled)
    // LIOT_CAMERA_OUTPUT_JPEG,      ///< JPEG format (not enabled)
} liot_camera_output_format_e;

/**
 * @brief Camera sensor function interface structure
 * @details Defines functional interfaces that camera sensors need to implement
 */
typedef struct 
{
    /**
     * @brief Sensor initialization function
     * @param handle Camera device handle
     * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
     */
    int (*init)(liot_camera_handle_t handle);   
    /**
     * @brief Set output format function
     * @param handle Camera device handle
     * @param format Output format
     * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
     */
    int (*set_output_format)(liot_camera_handle_t handle, liot_camera_output_format_e format);  
    /**
     * @brief Set output resolution and offset
     * @param handle Camera device handle
     * @param width Width
     * @param height Height
     * @param width_offset Width offset
     * @param height_offset Height offset
     * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
     */
    int (*set_framesize)(liot_camera_handle_t handle, uint16_t width, uint16_t height, uint16_t width_offset, uint16_t height_offset);  
}liot_camera_sensor_func_t;

/**
 * @brief Camera sensor configuration structure
 * @details Contains hardware configuration and functional interfaces of the sensor
 */
typedef struct 
{
    liot_camParamCfg_t param;                ///< CSPI configuration parameters
    uint8_t addr;                       ///< Sensor configuration I2C address
    liot_camI2cCfg_t *reg;                   ///< Sensor configuration initialization list
    liot_camera_sensor_func_t func;     ///< Sensor configuration functions
} liot_camera_sensor_t;

/**
 * @brief Image frame size configuration structure
 * @details Defines pixel quantity, offset and decimation amount
 */
typedef struct
{
    uint16_t size;          ///< Pixel quantity
    uint16_t offset;        ///< Pixel offset
    uint8_t scale;          ///< Pixel decimation amount
} liot_camera_framesize_t;

/**
 * @brief Image resolution configuration structure
 * @details Defines image width and height configuration
 */
typedef struct 
{
    liot_camera_framesize_t width;      ///< Image width configuration item
    liot_camera_framesize_t height;     ///< Image height configuration item
} liot_camera_resolution_t;

/**
 * @brief Camera output information structure
 * @details Contains image output format and resolution information
 */
typedef struct 
{
    liot_camera_output_format_e format;     ///< Image output color format
    liot_camera_resolution_t resolution;    ///< Image output resolution
}liot_camera_info_t;

typedef enum
{
	LIOT_CAM_6_5_M	= 0,  ///< camera 6.5M HZ
	LIOT_CAM_13_M	= 1,  ///< camera 13M HZ
	LIOT_CAM_25_5_M	= 2,  ///< camera 25.5M HZ
	LIOT_CAM_24_M	= 3,  ///< camera 24M HZ
} liot_camFrequence_e;

/**
 * @brief CSPI configuration structure
 * @details Defines configuration parameters for CSPI interface
 */
typedef struct
{
    liot_cspi_port_e num;   ///< CSPI number
    liot_camFrequence_e speed;   ///< CSPI speed
}liot_cspi_config_t;

/**
 * @brief Camera I2C configuration structure
 * @details Defines configuration parameters for I2C interface
 */
typedef struct 
{
    liot_i2c_channel_e num; ///< I2C number
    int8_t scl;             ///< I2C SCL pin number
    int8_t sda;             ///< I2C SDA pin number
}liot_camera_i2c_config_t;

/**
 * @brief Camera configuration structure
 * @details Contains complete configuration information for the camera
 */
typedef struct 
{
    liot_camera_info_t info;        ///< Camera output information configuration
    liot_cspi_config_t cspi;        ///< Camera CSPI configuration
    liot_camera_i2c_config_t i2c;   ///< Camera I2C configuration
    liot_camera_sensor_t *sensor;   ///< Camera Sensor configuration
}liot_camera_config_t;

/**
 * @brief Camera data reception callback function type
 * @param event Event type
 */
typedef void (*camRecvCb)(uint32_t event);

/// LCD handle type
typedef void* lcdHandle_t;

/**
 * @brief Camera initialization
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e liot_CamInit(void);

/**
 * @brief Camera deinitialization
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e liot_CamDeInit(void);

/**
 * @brief Capture image
 * @param pFrameBuf Image data buffer pointer
 * @param width Image width
 * @param height Image height
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e liot_CamCaptureImage(uint8_t *pFrameBuf, uint16_t width, uint16_t height);

/**
 * @brief Start camera preview
 * @param lcdHandle LCD device handle
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e liot_CamPreview(lcdHandle_t lcdHandle);

/**
 * @brief Stop camera preview
 * @param lcdHandle LCD device handle
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e liot_CamStopPreview(lcdHandle_t lcdHandle);

/**
 * @brief Write camera I2C register
 * @param handle Camera device handle
 * @param reg Register address
 * @param value Value to write
 * @return Returns LIOT_I2C_SUCCESS on success, I2C error code on failure
 */
liot_errcode_i2c_e liot_camera_i2c_reg_write(liot_camera_handle_t handle, uint8_t reg, uint8_t value);

/**
 * @brief Read camera I2C register
 * @param handle Camera device handle
 * @param reg Register address
 * @param value Pointer to store read value
 * @return Returns LIOT_I2C_SUCCESS on success, I2C error code on failure
 */
liot_errcode_i2c_e liot_camera_i2c_reg_read(liot_camera_handle_t handle, uint8_t reg, uint8_t *value);

/**
 * @brief Write camera I2C register by bits
 * @param handle Camera device handle
 * @param reg Register address
 * @param mask Bit mask
 * @param value Bit value to write
 * @return Returns LIOT_I2C_SUCCESS on success, I2C error code on failure
 */
liot_errcode_i2c_e liot_camera_i2c_reg_write_bit(liot_camera_handle_t handle, uint8_t reg, uint8_t mask, uint8_t value);

/**
 * @brief Initialize camera device
 * @param config Camera configuration structure pointer
 * @return Returns camera handle on success, NULL on failure
 */
liot_camera_handle_t Liot_CameraInit(liot_camera_config_t *config);

/**
 * @brief Deinitialize camera device
 * @param handle Camera device handle
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e Liot_CameraDeinit(liot_camera_handle_t handle);

 /**
 * @brief Capture image data
 * @param handle Camera device handle
 * @param data Data buffer pointer
 * @param timeout Timeout in milliseconds
 * @return Returns LIOT_CAMERA_SUCCESS on success, error code on failure
 */
liot_errcode_camera_e Liot_CameraCaptureImage(liot_camera_handle_t handle, uint8_t *data, uint32_t timeout);

 /**
 * @brief Initialize CSPI hardware interface
 * @param config CSPI configuration structure pointer
 * @return Returns camera handle on success, NULL on failure
 */
liot_camera_handle_t Liot_CspiInit(liot_camera_config_t *config);

/**
 * @brief Deinitialize CSPI hardware interface
 * @param handle Camera device handle
 * @return Returns LIOT_CSPI_SUCCESS on success, error code on failure
 */
liot_errcode_cspi_e Liot_CspiDeinit(liot_camera_handle_t handle);

/**
 * @brief Receive data via CSPI
 * @param handle Camera device handle
 * @param data Data buffer pointer
 * @param timeout Timeout in milliseconds
 * @return Returns LIOT_CSPI_SUCCESS on success, error code on failure
 */
liot_errcode_cspi_e Liot_CspiRecv(liot_camera_handle_t handle, uint8_t *data, uint32_t timeout);


typedef struct 
{
    uint16_t width;   
    uint16_t height;  
} Liot_Framesize_t;

typedef enum
{
    FRAMESIZE_96X96,    
    FRAMESIZE_QQVGA,    
    FRAMESIZE_QCIF,     
    FRAMESIZE_HQVGA,    
    FRAMESIZE_240X240,  
    FRAMESIZE_QVGA,     
    FRAMESIZE_CIF,      
    FRAMESIZE_HVGA,     
    FRAMESIZE_VGA,      
    FRAMESIZE_MAX,      
} Liot_Framesize_e;

#ifdef __cplusplus
}
#endif

#endif