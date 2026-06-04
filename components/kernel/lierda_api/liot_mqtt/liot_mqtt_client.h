/**
 * @File Name: liot_mqtt_client.h
 * @brief
 * @Author : L email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef LIOT_MQTTLCLIENT_H
#define LIOT_MQTTLCLIENT_H

#include "liot_type.h"

#define LIOT_MQTT_VERSION_3 3      // MQTT v3.1
#define LIOT_MQTT_VERSION_4 4      // MQTT v3.1.1

typedef int liot_mqtt_client_t;

/*MQTT的连接状态*/
enum MQTT_STATE
{
    MQTT_CONN_DEFAULT,      /*<!default state*/


    MQTT_CONN_NOT_OPEN,      /*<!not open*/

    MQTT_CONN_IS_OPENING,    /*<!is opening*/
    MQTT_CONN_OPENED,        /*<!opened*/
    MQTT_CONN_OPEN_FAIL,     /*<!open fail*/

    MQTT_CONN_IS_CONNECTING,    /*<!is connecting*/
    MQTT_CONN_CONNECTED,        /*<!connected*/
    MQTT_CONN_CONNECT_FAIL,     /*<!connect fail*/

    MQTT_CONN_IS_CLOSING,    /*<!is closing*/
    MQTT_CONN_CLOSED,        /*<!closed*/
    MQTT_CONN_CLOSED_FAIL,   /*<!closed fail*/

    MQTT_CONN_IS_DISCONNECTING,    /*<!is disconnecting*/
    MQTT_CONN_DISCONNECTED,        /*<!disconnected*/
    MQTT_CONN_DISCONNECTED_FAIL,   /*<!disconnected fail*/

    MQTT_CONN_RECONNECTING,        /*<!reconnecting*/
    MQTT_CONN_RECONNECTING_FAIL,   /*<!reconnecting fail*/
};

enum MQTT_EVENT_STATUS
{
    LIOT_MQTT_OPEN_EVENT,
    LIOT_MQTT_CONNECT_EVENT,
    LIOT_MQTT_RECONNECT_EVENT,
    LIOT_MQTT_PUB_EVENT,
    LIOT_MQTT_SUB_EVENT,
    LIOT_MQTT_UNSUB_EVENT,
    LIOT_MQTT_CLOSE_EVENT,
    LIOT_MQTT_DISCONNECT_EVENT,
    LIOT_MQTT_PUBLISH_EVENT,
};
struct liot_mqtt_ssl_config_t
{
    int ssl_ctx_id;                                 /*!< SSL context id*/    
    int verify_level;                               /*!< SSL verify level*/     
    char *cacert_path;                            /*!< CA certificate file path*/   
    char *client_cert_path;                       /*!< Client certificate file path*/
    char *client_key_path;                        /*!< Client key file path*/
    char *client_key_pwd;                         /*!< Client key password*/
    int ssl_version;                              /*!< SSL version*/
    int sni_enable;                               /*!< SNI enable*/
    int ssl_negotiate_timeout;                    /*!< SSL negotiate timeout*/
    int ignore_invalid_certsign;                  /*!< Ignore invalid certificate signature*/
    int ignore_multi_certchain_verify;           /*!< Ignore multiple certificate chain verification*/
    uint32_t ignore_certitem;                    /*!< Ignore certificate item*/
    char *cacert_buffer;                         /*!< CA certificate buffer*/
    bool client_cert_type;                       /*!< Client certificate type*/
};

typedef struct liot_mqtt_client_option_t
{
    unsigned char version;                        /*!< MQTT protocol version*/
    int pdp_cid;                                  /*!< PDP context id*/
    char *client_id;                             /*!< Client id*/
    char *client_user;                           /*!< Client user*/
    char *client_pass;                           /*!< Client password*/
    unsigned char ssl_enable;                    /*!< SSL enable*/
    void *ssl_ctx;                               /*!< SSL context*/
    struct liot_mqtt_ssl_config_t *ssl_cfg;      /*!< SSL configuration*/
    unsigned char clean_session;                 /*!< Clean session*/
    unsigned short kalive_time;                  /*!< Keep alive time*/     
    unsigned char delivery_time;                 /*!< Delivery time*/
    unsigned char delivery_cnt;                  /*!< Delivery count*/
    unsigned char will_flag;                     /*!< Will flag*/
    unsigned char will_qos;                      /*!< Will QoS*/
    unsigned char will_retain;                   /*!< Will retain*/
    char will_topic[257];                       /*!< Will topic*/
    char will_message[257];                     /*!< Will message*/
    unsigned char ping_timeout;                 /*!< Ping timeout*/
} liot_mqtt_client_option;

typedef enum
{
    LIOT_MQTTCLIENT_SUCCESS          = 0,        /*!< Success*/
    LIOT_MQTTCLIENT_INVALID_PARAM    = -1,       /*!< Invalid parameter*/
    LIOT_MQTTCLIENT_WOUNDBLOCK       = -2,       /*!< Would block*/
    LIOT_MQTTCLIENT_OUT_OF_MEM       = -3,       /*!< Out of memory*/
    LIOT_MQTTCLIENT_ALLOC_FAIL       = -4,       /*!< Allocation fail*/
    LIOT_MQTTCLIENT_TCP_CONNECT_FAIL = -5,       /*!< TCP connect fail*/
    LIOT_MQTTCLIENT_NOT_CONNECT      = -6,       /*!< Not connect*/
    LIOT_MQTTCLIENT_SEND_PKT_FAIL    = -7,       /*!< Send packet fail*/
    LIOT_MQTTCLIENT_BAD_REQUEST      = -8,       /*!< Bad request*/
    LIOT_MQTTCLIENT_TIMEOUT          = -9,       /*!< Timeout*/
} liot_mqtt_error_code_e;


typedef struct liot_mqtt_recvMessage_t
{
    int qos;
    bool retained;
    bool dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
    char *topicName;
    size_t topicLen;
}liot_mqtt_recvMessage;

/**
 * @brief MQTT client connection callback function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] arg Custom argument passed to the callback
 * @param[in] status Connection status
 * @return None
 */
typedef void (*liot_mqtt_connection_cb_t)(liot_mqtt_client_t *client, void *arg, int status);

/**
 * @brief MQTT client request callback function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] arg Custom argument passed to the callback
 * @param[in] err Error code
 * @return None
 */
typedef void (*liot_mqtt_request_cb_t)(liot_mqtt_client_t *client, void *arg, int err);

/**
 * @brief MQTT client incoming publish callback function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] arg Custom argument passed to the callback
 * @param[in] pkt_id Packet ID
 * @param[in] topic Topic name
 * @param[in] payload Payload data
 * @param[in] payload_len Payload length
 * @return None
 */
typedef void (*liot_mqtt_incoming_publish_cb_t)(liot_mqtt_client_t *client,
                                                void *arg,
                                                int pkt_id,
                                                const char *topic,
                                                const unsigned char *payload,
                                                unsigned short payload_len);

/**
 * @brief MQTT client disconnect callback function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] arg Custom argument passed to the callback
 * @param[in] err Error code
 * @return None
 */

typedef void (*liot_mqtt_disconnect_cb_t)(liot_mqtt_client_t *client, void *arg, int err);

/**
 * @brief MQTT client state exception callback function
 * @param[in] client Pointer to the MQTT client instance
 * @return None
 */
typedef void (*liot_mqtt_state_exception_cb_t)(liot_mqtt_client_t *client);

/**
 * @brief MQTT client reconnect callback function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] arg Custom argument passed to the callback
 * @return None
 */
typedef void (*liot_mqtt_event_cb_t)(liot_mqtt_client_t *client, int event, void *arg, void *data);


int liot_mqtt_client_init_ex(liot_mqtt_client_t *client, int cid, liot_mqtt_event_cb_t event_cb, void *arg);

/**
 * @brief MQTT client initialization function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] cid Data channel number
 * @return int: Error code
 */

int liot_mqtt_client_init(liot_mqtt_client_t *client, int cid);

/**
 * @brief MQTT client connection function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] host Host name or IP address  
 * @param[in] cb Connection callback function
 * @param[in] arg Custom argument passed to the callback
 * @param[in] client_info MQTT client options
 * @param[in] exp_cb State exception callback function
 * @return int: Error code 0-- success, other values indicate failure
 * @note mqtt:  mqtt://220.180.239.212:8306
 * @note mqtts: mqtts://220.180.239.212:8307
 */
int liot_mqtt_connect(liot_mqtt_client_t *client,
                      const char *host,
                      liot_mqtt_connection_cb_t cb,
                      void *arg,
                      liot_mqtt_client_option *client_info,
                      liot_mqtt_state_exception_cb_t exp_cb);

/**
 * @brief MQTT client publish function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] topic Topic name
 * @param[in] payload Payload data
 * @param[in] payload_length Payload length
 * @param[in] qos QoS quality of service
 * @param[in] retain Retain flag
 * @param[in] cb Request callback function
 * @param[in] arg Custom argument passed to the callback
 * @return int: Error code 0-- success, other values indicate failure
 */
int liot_mqtt_publish(liot_mqtt_client_t *client,
                      const char *topic,
                      const void *payload,
                      unsigned short payload_length,
                      unsigned char qos,
                      unsigned char retain,
                      liot_mqtt_request_cb_t cb,
                      void *arg);

/**
 * @brief MQTT client subscribe/unsubscribe function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] topic Topic name
 * @param[in] qos Quality of Service
 * @param[in] cb Request callback function
 * @param[in] arg  callback function argument
 * @param[in] sub  0 unsubscribe, non-0 subscribe
 * @return int: Error code 0-- success, other values indicate failure
 */
int liot_mqtt_sub_unsub(liot_mqtt_client_t *client,
                        const char *topic,
                        unsigned char qos,
                        liot_mqtt_request_cb_t cb,
                        void *arg,
                        unsigned char sub);

 /**
  * @brief MQTT dissconnect function
  * 
  * @param[in] client   mqtt client handle 
  * @param[in] cb  mqtt disconnect callback
  * @param[in ] arg callback function argument 
  * @return int: Error code 0-- success, other values indicate failure 
  */
int liot_mqtt_disconnect(liot_mqtt_client_t *client, liot_mqtt_disconnect_cb_t cb, void *arg);

/**
 * @brief Set incoming publish callback for MQTT client
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] inpub_cb Incoming publish callback function
 * @param[in] arg Custom argument passed to the callback
 * @return int: Error code 0-- success, other values indicate failure
 * @note The callback function will be called when a publish message is received
 */
int liot_mqtt_set_inpub_callback(liot_mqtt_client_t *client, liot_mqtt_incoming_publish_cb_t inpub_cb, void *arg);

/**
 * @brief Check if the MQTT client is connected
 * @param[in] client Pointer to the MQTT client instance
 * @return int: Error code 0-- success, other values indicate failure
 */
int liot_mqtt_client_is_connected(liot_mqtt_client_t *client);

/**
 * @brief Get the MQTT client state
 * @param[in] client Pointer to the MQTT client instance
 * @return int: Error code 0-- success, other values indicate failure
 */

int liot_mqtt_client_state(liot_mqtt_client_t *client);

/**
 * @brief Deinitialize the MQTT client
 * @param[in] client Pointer to the MQTT client instance
 * @return int: Error code 0-- success, other values indicate failure
 */
int liot_mqtt_client_deinit(liot_mqtt_client_t *client);

/**
 * @brief Send a MQTT ping request
 * @param[in] client Pointer to the MQTT client instance
 * @return int: Error code 0-- success, other values indicate failure
 */

int liot_mqtt_pingreq(liot_mqtt_client_t *client);

/**
 * @brief Generate OneNET authentication token
 * @param[in] expire_time Expiration time for the token
 * @param[in] product_id Product ID
 * @param[in] device_name Device name
 * @param[in] version Version number (e.g., "2018-10-31")
 * @param[in] device_access_key Device access key
 * @return char*: Pointer to the generated authentication token, or NULL if an error occurs
 */
char *liot_onenet_generate_auth_token(INT64 expire_time,
                                      const char *product_id,
                                      const char *device_name,
                                      const char *version,
                                      const char *device_access_key);

/**
 * @brief MQTT client reconnect callback function
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] arg Custom argument passed to the callback
 * @return None
 */
typedef void (*liot_mqtt_reconnect_cb_t)(liot_mqtt_client_t *client, void *arg);

/**
 * @brief Set reconnect callback function for MQTT client
 * @param[in] client Pointer to the MQTT client instance
 * @param[in] reconnect_cb Reconnect callback function
 * @param[in] arg Custom argument passed to the callback
 * @return int: Error code 0-- success, other values indicate failure
 */
int liot_mqtt_set_reconnect_callback(liot_mqtt_client_t *client, 
    liot_mqtt_reconnect_cb_t reconnect_cb,void *arg);     

#endif
