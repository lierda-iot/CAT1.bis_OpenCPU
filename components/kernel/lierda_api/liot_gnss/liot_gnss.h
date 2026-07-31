

#ifndef __LIOT_GNSS_H__
#define __LIOT_GNSS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "liot_api_common.h"

#define LIERDA_GNSS_APGS_URL_MAX            (253+1)

/*
 * The following interfaces are applicable to NT26KCNE20GNA and NT26KCNE20GNB series modules, 
 * other models are not supported at this time.
 */

/*!
 * @brief GNSS error code enumeration type
 */
typedef enum
{
    LIOT_GNSS_SUCCESS = LIOT_SUCCESS,        /*!< Operation successful */
    LIOT_GNSS_EXECUTE_ERR = 1,               /*!< Execution error */
    LIOT_GNSS_PARAM_ERR,                     /*!< Parameter error */
} liot_gnss_errcode_e;


/*!
 * @brief GNSS chip model enumeration
 * @details This enumeration defines the GNSS chip models supported by the system.
 *          Each value represents a specific GNSS chip model with its unique characteristics and capabilities.
 *          The configuration must be consistent with the actual hardware to ensure proper system operation.
 */
typedef enum
{
    GNSS_IC_CC1161W = 0,    /*!< CC1161W GNSS chip model */
    GNSS_IC_CC1177W,        /*!< CC1177W GNSS chip model */
} liot_gnss_ic_model_e;

/*!
 * @brief GNSS configuration structure
 * @details This structure contains the configuration parameters for the GNSS module,
 *          including chip model, NMEA output type, positioning system configuration,
 *          apflash switch, and agnss switch settings.
 */
typedef struct
{
    liot_gnss_ic_model_e gnss_ic_model; /*!< Configure GNSS chip model */
    uint8_t   gnssnmea_type;            /*!< Configure NMEA sentence output type */
    uint8_t   gnsscfg_type;             /*!< Configure system combination for positioning */
    uint8_t   apflash;                  /*!< apflash switch */
    uint8_t   agnss_mode;               /*!< agnss switch */
} liot_gnss_config_t;

/*!
 * @brief AGNSS configuration structure
 * @details This structure contains the configuration parameters required for Assisted GNSS (AGNSS) functionality.
 *          It includes the AGNSS server URL and authentication credentials (CID, MID, Password) needed to 
 *          establish a connection with the AGNSS service for enhanced positioning performance.
 */
typedef struct
{
    uint8_t   agnss_url[LIERDA_GNSS_APGS_URL_MAX]; /*!< AGNSS URL */
    uint8_t   cid[64];                             /*!< CID */
    uint8_t   mid[64];                             /*!< MID */
    uint8_t   pw[64];                              /*!< Password */
} liot_agnss_config_t;

/*!
 * @brief GNSS location information structure
 * @details This structure contains all the positioning information obtained from the GNSS module,
 *          including position coordinates, speed, satellite information, and positioning status.
 */
typedef struct {
    uint8_t fs;                  /*!< Positioning status: 0-No positioning, 1-Single point positioning, 2-Differential positioning */
    char utc[20];                /*!< UTC time YYMMDDhhmmss.sss */
    char latitude[14];           /*!< Latitude */
    char longitude[14];          /*!< Longitude */
    char hdop[8];                /*!< HDOP horizontal precision factor */
    char altitude[10];           /*!< Altitude in meters */
    uint8_t fix;                 /*!< Positioning status: 1-Not positioned, 2-2D positioning, 3-3D positioning */
    char cog[10];                /*!< Ground course based on true north */
    char speedkm[10];            /*!< Speed in kilometers per hour */
    char speedkn[10];            /*!< Speed in knots */
    uint8_t satcount;            /*!< Number of satellites */
} liot_gnss_loc_info_t;

/*!
 * @brief GNSS event type enumeration
 * @details This enumeration defines the various event types that can be reported by the GNSS module.
 *          These events indicate the current status of the GNSS module, including error conditions,
 *          readiness states, NMEA data availability, firmware update status, and AGNSS operation results.
 */
typedef enum
{
    LIOT_GNSS_EVENT_GNSS_ERROR = 0,    /*!< GNSS module error event */
    LIOT_GNSS_EVENT_GNSS_READY,        /*!< GNSS module ready event */
    LIOT_GNSS_EVENT_GNSS_CLOSED,       /*!< GNSS module closed event */
    LIOT_GNSS_EVENT_GNSS_NMEA,         /*!< GNSS NMEA data available event */
    LIOT_GNSS_EVENT_FARMWARE_OK,       /*!< GNSS firmware update successful event */
    LIOT_GNSS_EVENT_FARMWARE_ERROR,    /*!< GNSS firmware update error event */
    LIOT_GNSS_EVENT_AGNSS_OK,          /*!< AGNSS operation successful event */
    LIOT_GNSS_EVENT_AGNSS_ERROR,       /*!< AGNSS operation error event */
} liot_gnss_event_type_e;

/*!
 * @brief GNSS NMEA sentence type enumeration
 * @details This enumeration defines the different NMEA sentence types supported by the GNSS module.
 *          These values are bit flags that can be combined to specify the desired NMEA sentence types.
 *          Each type corresponds to a standard NMEA 0183 protocol sentence format.
 */
typedef enum
{
    LIOT_GNSS_NMEA_TYPE_GGA = 0x0001,  /*!< GGA sentence type - Global Positioning System Fix Data */
    LIOT_GNSS_NMEA_TYPE_RMC = 0x0002,  /*!< RMC sentence type - Recommended Minimum Specific GPS/Transit Data */
    LIOT_GNSS_NMEA_TYPE_GSV = 0x0004,  /*!< GSV sentence type - Satellites in View */
    LIOT_GNSS_NMEA_TYPE_GSA = 0x0008,  /*!< GSA sentence type - GPS DOP and Active Satellites */
    LIOT_GNSS_NMEA_TYPE_VTG = 0x0010,  /*!< VTG sentence type - Course Over Ground and Ground Speed */
    LIOT_GNSS_NMEA_TYPE_GLL = 0x0020,  /*!< GLL sentence type - Geographic Position - Latitude/Longitude */
} liot_gnss_nmea_type_e;

/*!
 * @brief GNSS positioning system configuration type enumeration
 * @details This enumeration defines the different satellite positioning system configuration types supported by the GNSS module.
 *          These values are bit flags that can be combined to specify the satellite positioning systems to be enabled.
 *          Each type corresponds to a satellite navigation system.
 */
typedef enum
{
    LIOT_GNSS_CFG_TYPE_GPS = 0x01,     /*!< GPS positioning system */
    LIOT_GNSS_CFG_TYPE_BEIDOU = 0x02,  /*!< BeiDou positioning system */
    LIOT_GNSS_CFG_TYPE_GLONASS = 0x04, /*!< GLONASS positioning system */
    LIOT_GNSS_CFG_TYPE_GALILEO = 0x08, /*!< Galileo positioning system */
} liot_gnss_cfg_type_e;

/*!
 * @brief GNSS event callback function pointer type
 * @details This type defines the function pointer format for GNSS module event callback functions.
 *          When the GNSS module generates an event, it will notify the application layer through 
 *          a callback function of this type.
 *          The callback function includes three parameters: event type, related data, and data length.
 * @param[in] event Event type, indicating the type of GNSS event that occurred
 * @param[in] data Pointer to event-related data, pointing to the data content related to the event
 * @param[in] datalen Data length, indicating the byte length of the data pointed to by data
 */
typedef void (*LiotGnssEventCb) (liot_gnss_event_type_e event, uint8_t *data, uint16_t datalen);

/**
 * @brief  Configure GNSS module parameters
 * @details This function configures the basic parameters of the GNSS module, including chip model, NMEA sentence type,
 *          positioning system configuration, apflash switch, and agnss switch parameters.
 *          
 * @param[in] config Pointer to the GNSS configuration parameter structure
 *                   - gnss_ic_model: GNSS chip model
 *                   - gnssnmea_type: NMEA sentence output type
 *                   - gnsscfg_type: Positioning system combination configuration
 *                   - apflash: apflash switch
 *                   - agnss_mode: agnss switch
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: Configuration successful
 *         - LIOT_GNSS_PARAM_ERR: Parameter error
 */
liot_gnss_errcode_e liot_gnss_config(liot_gnss_config_t *config);

/**
 * @brief  Configure AGNSS parameters
 * @details This function configures the parameters for Assisted GNSS (AGNSS) functionality,
 *          including AGNSS server URL, authentication information (CID, MID, password), etc.
 *          
 * @param[in] config Pointer to the AGNSS configuration parameter structure
 *                   - agnss_url: AGNSS server URL
 *                   - cid: CID authentication information
 *                   - mid: MID authentication information
 *                   - pw: Password authentication information
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: Configuration successful
 * 
 * @note For related information, please refer to the demo example. 
 *       These are typically fixed values provided by the vendor.
 */
liot_gnss_errcode_e liot_agnss_config(liot_agnss_config_t *config);

/**
 * @brief  Open GNSS module
 * @details This function initializes and opens the GNSS module, registering the event callback function.
 *          After successful opening, the GNSS module starts working and reports relevant events through the callback function.
 *          
 * @param[in] event_cb GNSS event callback function pointer
 *                     This callback will be invoked when GNSS events occur, including:
 *                     - GNSS module ready event
 *                     - GNSS module closed event
 *                     - NMEA data available event
 *                     - Firmware update status event
 *                     - AGNSS operation result event
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: GNSS module opened successfully
 *         - LIOT_GNSS_EXECUTE_ERR: Failed to open GNSS module
 */
liot_gnss_errcode_e liot_gnss_open(LiotGnssEventCb event_cb);

/**
 * @brief  Close GNSS module
 * @details This function closes the opened GNSS module and releases related resources.
 *          After successful closing, the GNSS module stops working.
 *          
 * @param[in] void No parameters
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: GNSS module closed successfully
 *         - LIOT_GNSS_EXECUTE_ERR: Failed to close GNSS module
 */
liot_gnss_errcode_e liot_gnss_close(void);

/**
 * @brief  Get GNSS location information
 * @details This function retrieves the positioning information from the GNSS module, 
 *          including position coordinates, speed, satellite information, and positioning status.
 *          The obtained raw data is parsed and stored in the user-provided location information structure.
 *          
 * @param[out] loc_info Pointer to the GNSS location information structure for storing the retrieved positioning data
 *                      - fs: Positioning status (0-No positioning, 1-Single point positioning, 2-Differential positioning)
 *                      - utc: UTC time (YYMMDDhhmmss.sss format)
 *                      - latitude: Latitude (format: ddmm.mmmmN)
 *                      - longitude: Longitude (format: dddmm.mmmmE)
 *                      - hdop: HDOP horizontal precision factor
 *                      - altitude: Altitude in meters
 *                      - fix: Positioning status (1-Not positioned, 2-2D positioning, 3-3D positioning)
 *                      - cog: Ground course based on true north
 *                      - speedkm: Speed in kilometers per hour
 *                      - speedkn: Speed in knots
 *                      - satcount: Number of satellites
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: Successfully retrieved location information
 */
liot_gnss_errcode_e liot_gnss_get_location(liot_gnss_loc_info_t *loc_info);

/**
 * @brief  Get GNSS NMEA sentences
 * @details This function retrieves the specified types of GNSS NMEA sentence data.
 *          Users can specify the required NMEA sentence types, and the function will return the corresponding NMEA data.
 *          
 * @param[in] nmea_type The NMEA sentence types to retrieve, can be a combination of the following values:
 *                      - LIOT_GNSS_NMEA_TYPE_GGA: GGA sentence type - Global Positioning System Fix Data
 *                      - LIOT_GNSS_NMEA_TYPE_RMC: RMC sentence type - Recommended Minimum Specific GPS/Transit Data
 *                      - LIOT_GNSS_NMEA_TYPE_GSV: GSV sentence type - Satellites in View
 *                      - LIOT_GNSS_NMEA_TYPE_GSA: GSA sentence type - GPS DOP and Active Satellites
 *                      - LIOT_GNSS_NMEA_TYPE_VTG: VTG sentence type - Course Over Ground and Ground Speed
 *                      - LIOT_GNSS_NMEA_TYPE_GLL: GLL sentence type - Geographic Position - Latitude/Longitude
 * @param[out] respBuf Pointer to string array for storing returned NMEA sentence data
 * @param[in] respMaxNumber Maximum capacity of the respBuf array
 * @param[out] respNumber Actual number of returned NMEA sentences
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: Successfully retrieved NMEA sentences
 *         - LIOT_GNSS_PARAM_ERR: Parameter error (no valid NMEA type specified)
 *         - LIOT_GNSS_EXECUTE_ERR: Failed to execute NMEA sentence retrieval
 */
liot_gnss_errcode_e liot_gnss_get_nmea(uint16_t nmea_type, char *respBuf[], uint16_t respMaxNumber, uint16_t *respNumber);

/**
 * @brief  Force close GNSS backup power
 * @details This function is used to force close the backup power of the GNSS module.
 *          The close operation can only be successfully executed when the backup power is in the on state.
 *          
 * @param[in] void No parameters
 * 
 * @return liot_gnss_errcode_e Error code
 *         - LIOT_GNSS_SUCCESS: Successfully closed backup power
 *         - LIOT_GNSS_EXECUTE_ERR: Failed to close backup power or backup power is not enabled
 */
liot_gnss_errcode_e liot_gnss_close_backup_power(void);

#endif

