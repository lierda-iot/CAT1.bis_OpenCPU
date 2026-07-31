/**
 * @File Name: liot_gnss_demo.c
 * @brief  
 * @Author : wty 
  * @Version : 1.0
 * @Creat Date : 2024-11-12
 * 
 * @copyright Copyright (c) 2024 Lierda Science & Technology Group Co., Ltd.
 *
 * @par Modification Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2024-08-23 <td>1.0     <td>wty           <td>new create
 * <tr><td>2025-08-11 <td>1.1     <td>zlc           <td>Optimize the demo format and change the comments to English.
 * </table>
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_gnss.h"
#include "liot_datacall.h"


#define AGNSS_DEMO_ENABLE 1

#define GNSS_TEST_E1_0G
//#define GNSS_TEST_E0_0G

// Timer related definitions
#define GNSS_WORK_TIMEOUT     (60*1000)  // 1 minute
#define GNSS_RESTART_TIMEOUT  (10*1000)  // 10 seconds

char gnss_nmea_data[1024];
uint8_t gnss_ready_state = 0;

// Global queue handles and timer handles
liot_queue_t gnss_queuehandle = NULL;
liot_timer_t gnss_work_timer = NULL;
liot_timer_t gnss_restart_timer = NULL;

#define GNSS_QUEUE_MAX 10

// Demo internal message type
typedef enum {
    GNSS_DEMO_MSG_GNSS_EVENT = 0,
    GNSS_DEMO_MSG_WORK_TIMEOUT,
    GNSS_DEMO_MSG_RESTART_TIMEOUT
} gnss_demo_msg_type_e;

// GNSS event structure
typedef struct {
    gnss_demo_msg_type_e msg_type;
    liot_gnss_event_type_e event_type;
    uint8_t *data;
    uint16_t datalen;
    uint8_t data_owned;
} gnss_event_msg_t;

// GNSS state enumeration
typedef enum {
    GNSS_STATE_IDLE = 0,
    GNSS_STATE_RUNNING,
    GNSS_STATE_CLOSING,
    GNSS_STATE_RESTARTING
} gnss_state_e;

gnss_state_e g_gnss_state = GNSS_STATE_IDLE;

static void gnss_demo_free_msg_data(gnss_event_msg_t *msg)
{
    if(msg && msg->data_owned && msg->data)
    {
        liot_rtos_free(msg->data);
    }

    if(msg)
    {
        msg->data = NULL;
        msg->datalen = 0;
        msg->data_owned = 0;
    }
}

static int gnss_demo_queue_send(gnss_event_msg_t *msg)
{
    int ret = LIOT_SUCCESS;

    if(msg == NULL || gnss_queuehandle == NULL)
    {
        gnss_demo_free_msg_data(msg);
        liot_trace("GNSS demo queue is not ready");
        return -1;
    }

    ret = liot_rtos_queue_release(gnss_queuehandle, sizeof(gnss_event_msg_t), (uint8 *)msg, 0);
    if(ret != LIOT_SUCCESS)
    {
        gnss_demo_free_msg_data(msg);
        liot_trace("GNSS demo queue send failed, ret=%d", ret);
    }

    return ret;
}

void liot_show_mem(void)
{
    //Print out SRAM information
    liot_trace("========== rtos Get TotalHeapSize:%dKB,FreeHeapSize:%dKB,MinFreeHeapSize:%dKB,MaxFreeBlockSize:%dKB", 
            (liot_xPortGetTotalHeapSize()) >> 10, (liot_xPortGetFreeHeapSize()) >> 10,
            (liot_xPortGetMinimumEverFreeHeapSize()) >> 10, (liot_xPortGetMaximumFreeBlockSize()) >> 10);
}

/**
 * @brief GNSS event callback function
 * @details Sends GNSS events to message queue for processing
 */
void liot_gnss_demo_event(liot_gnss_event_type_e event, uint8_t *data, uint16_t datalen)
{
    liot_trace("liot_gnss_demo_event %d", event);
    gnss_event_msg_t event_msg = {0};
    
    event_msg.msg_type = GNSS_DEMO_MSG_GNSS_EVENT;
    event_msg.event_type = event;
    event_msg.datalen = datalen;
    // Note: For NMEA data, need to copy data content as original data pointer may become invalid after callback
    if(data && datalen > 0 && (event == LIOT_GNSS_EVENT_GNSS_NMEA)) {
        event_msg.data = liot_rtos_malloc(datalen + 1);
        if(event_msg.data) {
            memcpy(event_msg.data, data, datalen);
            event_msg.data[datalen] = '\0';
            event_msg.data_owned = 1;
        } else {
            liot_trace("GNSS NMEA data malloc failed, len=%d", datalen);
            return;
        }
    } else {
        event_msg.data = data;
    }
    
    gnss_demo_queue_send(&event_msg);
}

/**
 * @brief 1 minute work timer callback function
 * @details Closes GNSS module after timer timeout
 */
void gnss_work_timer_callback(void *param)
{
    liot_trace("GNSS work timer timeout, closing GNSS...");
    
    // Send internal close request to queue
    gnss_event_msg_t close_msg = {0};
    close_msg.msg_type = GNSS_DEMO_MSG_WORK_TIMEOUT;
    close_msg.data = NULL;
    close_msg.datalen = 0;
    gnss_demo_queue_send(&close_msg);
}

/**
 * @brief 10 second restart timer callback function
 * @details Reopens GNSS module after timer timeout
 */
void gnss_restart_timer_callback(void *param)
{
    liot_trace("GNSS restart timer timeout, restarting GNSS...");
    
    // Send internal restart request to queue
    gnss_event_msg_t restart_msg = {0};
    restart_msg.msg_type = GNSS_DEMO_MSG_RESTART_TIMEOUT;
    restart_msg.data = NULL;
    restart_msg.datalen = 0;
    gnss_demo_queue_send(&restart_msg);
}

/**
 * @brief Get GNSS demo data
 * @details Retrieves and prints GNSS positioning information and NMEA data
 */
void liot_gnss_get_demo_data(void)
{
    liot_gnss_loc_info_t loc_info = {0};
    char *nmea_data[10] = {NULL};
    uint16_t nmea_data_len = 0;

    liot_trace("get demo data...");
    
    // Get NMEA data
    if(liot_gnss_get_nmea(LIOT_GNSS_NMEA_TYPE_GSA, nmea_data, 10, &nmea_data_len) == LIOT_GNSS_SUCCESS)
    {
        for(int i=0; i<nmea_data_len; i++)
        {
            liot_trace("%s", nmea_data[i]);
            liot_rtos_free(nmea_data[i]);
            nmea_data[i] = NULL;
        }
    }
    else
    {
        liot_trace("Failed to get GSA NMEA data");
    }
    
    nmea_data_len = 0;
    if(liot_gnss_get_nmea(LIOT_GNSS_NMEA_TYPE_VTG, nmea_data, 10, &nmea_data_len) == LIOT_GNSS_SUCCESS)
    {
        for(int i=0; i<nmea_data_len; i++)
        {
            liot_trace("%s", nmea_data[i]);
            liot_rtos_free(nmea_data[i]);
            nmea_data[i] = NULL;
        }
    }
    else
    {
        liot_trace("Failed to get VTG NMEA data");
    }
    
    // Get positioning information
    memset(&loc_info, 0, sizeof(loc_info));
    if(liot_gnss_get_location(&loc_info) == LIOT_GNSS_SUCCESS)
    {
        liot_trace("utc: %s, fix_type: %d, num_sats: %d", loc_info.utc, loc_info.fix, loc_info.satcount);
        liot_trace("Latitude: %s, Longitude: %s", loc_info.latitude, loc_info.longitude);
    }
    else
    {
        liot_trace("Failed to get GNSS location");
    }
}

/**
 * @brief Start GNSS module
 * @details Configures and opens GNSS module, starts work timer
 */
void start_gnss_module(void)
{
    liot_gnss_config_t gnss_config = {0};
    
    liot_trace("Starting GNSS module...");
    
    // GNSS configuration
#if defined (GNSS_TEST_E0_0G) 
    gnss_config.gnss_ic_model = GNSS_IC_CC1161W;
    gnss_config.gnsscfg_type = LIOT_GNSS_CFG_TYPE_GPS | LIOT_GNSS_CFG_TYPE_BEIDOU;
#elif defined (GNSS_TEST_E1_0G)
    gnss_config.gnss_ic_model = GNSS_IC_CC1177W;
    gnss_config.gnsscfg_type = LIOT_GNSS_CFG_TYPE_BEIDOU;
#endif
    gnss_config.gnssnmea_type = LIOT_GNSS_NMEA_TYPE_GGA | LIOT_GNSS_NMEA_TYPE_GSA;
    gnss_config.apflash = 0;
    gnss_config.agnss_mode = 0;

#if AGNSS_DEMO_ENABLE
    liot_agnss_config(NULL);
    liot_rtos_task_sleep_ms(10000);
#endif

    // Configure and open GNSS
    if(liot_gnss_config(&gnss_config) != LIOT_GNSS_SUCCESS) {
        g_gnss_state = GNSS_STATE_IDLE;
        liot_trace("Failed to configure GNSS module");
        return;
    }

    if(liot_gnss_open(liot_gnss_demo_event) == LIOT_GNSS_SUCCESS) {
        g_gnss_state = GNSS_STATE_RUNNING;
        liot_trace("GNSS module opened successfully");
        
        // Start 1 minute work timer
        liot_rtos_timer_start(gnss_work_timer, GNSS_WORK_TIMEOUT);
        liot_trace("GNSS work timer started for %d ms", GNSS_WORK_TIMEOUT);
    } else {
        g_gnss_state = GNSS_STATE_IDLE;
        liot_trace("Failed to open GNSS module");
    }
}

/**
 * @brief Stop GNSS module
 * @details Closes GNSS module, starts restart timer
 */
void stop_gnss_module(void)
{
    liot_trace("Stopping GNSS module...");
    
    if(liot_gnss_close() == LIOT_GNSS_SUCCESS) {
        g_gnss_state = GNSS_STATE_IDLE;
        gnss_ready_state = 0;
        liot_trace("GNSS module closed successfully");
        
        // Start 10 second restart timer
        liot_rtos_timer_start(gnss_restart_timer, GNSS_RESTART_TIMEOUT);
        liot_trace("GNSS restart timer started for %d ms", GNSS_RESTART_TIMEOUT);

        liot_show_mem();
    } else {
        g_gnss_state = GNSS_STATE_RUNNING;
        liot_trace("Failed to close GNSS module");
    }
}

/**
 * @brief GNSS demo main thread
 * @details Uses event-driven approach to handle GNSS operations and timer control
 */
void liot_gnss_demo_thread(void *argv)
{

    gnss_event_msg_t event_msg = {0};
    int ret = 0;
    
    liot_rtos_task_sleep_ms(2000);
    liot_trace("max--GNSS demo thread started");
    
    // Create queue
    if(liot_rtos_queue_create(&gnss_queuehandle, sizeof(gnss_event_msg_t), GNSS_QUEUE_MAX) != LIOT_SUCCESS)
    {
        liot_trace("Failed to create GNSS demo queue");
        return;
    }
    
    // Create timers
    if(liot_rtos_timer_create(&gnss_work_timer, LIOT_TimerOnce, gnss_work_timer_callback, NULL) != LIOT_SUCCESS)
    {
        liot_trace("Failed to create GNSS work timer");
        return;
    }

    if(liot_rtos_timer_create(&gnss_restart_timer, LIOT_TimerOnce, gnss_restart_timer_callback, NULL) != LIOT_SUCCESS)
    {
        liot_trace("Failed to create GNSS restart timer");
        return;
    }
    
    // Start GNSS module
    start_gnss_module();
    
    liot_trace("GNSS demo started, waiting for events...");
    
    // Event processing loop
    while(1)
    {
        ret = liot_rtos_queue_wait(gnss_queuehandle, (uint8 *)&event_msg, sizeof(gnss_event_msg_t), LIOT_WAIT_FOREVER);
        if (ret != LIOT_SUCCESS)
        {
            continue;
        }
        
        // Process demo internal messages and GNSS events
        switch(event_msg.msg_type)
        {
            case GNSS_DEMO_MSG_WORK_TIMEOUT:
            {
                if(g_gnss_state == GNSS_STATE_RUNNING)
                {
                    g_gnss_state = GNSS_STATE_CLOSING;
                    stop_gnss_module();
                }
                else
                {
                    liot_trace("Ignore GNSS work timeout in state %d", g_gnss_state);
                }
            }
            break;

            case GNSS_DEMO_MSG_RESTART_TIMEOUT:
            {
                if(g_gnss_state == GNSS_STATE_IDLE)
                {
                    g_gnss_state = GNSS_STATE_RESTARTING;
                    start_gnss_module();
                }
                else
                {
                    liot_trace("Ignore GNSS restart timeout in state %d", g_gnss_state);
                }
            }
            break;

            case GNSS_DEMO_MSG_GNSS_EVENT:
            {
                switch(event_msg.event_type)
                {
                    case LIOT_GNSS_EVENT_GNSS_READY:
                    {
                        liot_trace("GNSS is ready");
                        gnss_ready_state = 1;

                        liot_show_mem();
                    }
                    break;

                    case LIOT_GNSS_EVENT_GNSS_NMEA:
                    {
                        if(event_msg.data && event_msg.datalen > 0)
                        {
                            uint16_t copy_len = event_msg.datalen;

                            if(copy_len >= sizeof(gnss_nmea_data))
                            {
                                copy_len = sizeof(gnss_nmea_data) - 1;
                                liot_trace("GNSS NMEA data truncated, len=%d", event_msg.datalen);
                            }

                            memset(gnss_nmea_data, 0, sizeof(gnss_nmea_data));
                            memcpy(gnss_nmea_data, (char *)event_msg.data, copy_len);
                            gnss_nmea_data[copy_len] = '\0';
                            liot_trace("GNSS NMEA: %s", gnss_nmea_data);
                        }

                        gnss_demo_free_msg_data(&event_msg);
                    }
                    break;
                    
                    case LIOT_GNSS_EVENT_GNSS_CLOSED:
                    {
                        liot_trace("GNSS is closed");
                    }
                    break;

                    case LIOT_GNSS_EVENT_GNSS_ERROR:
                    {
                        liot_trace("GNSS error occurred");
                    }
                    break;
                    
                    default:
                    {
                        liot_trace("Unknown GNSS event: %d", event_msg.event_type);
                    }
                    break;
                }
            }
            break;
            
            default:
            {
                liot_trace("Unknown GNSS demo message: %d", event_msg.msg_type);
            }
            break;
        }
    }
}
