/**
 * @File Name: liot_socket_demo.c
 * @Brief: socket demo
 * @Version : 1.0
 * @Creat Date : 2025-05-19
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 *
 */
/**
 * Overall functionality introduction of the socket demo:
 * 1. After the socket task starts, it waits for the network registration to succeed, and then initiates a data call.
 * 2. After the data call succeeds, it performs DNS domain name resolution to obtain the IP address of the server to connect to.
 * 3. Then it calls the lwip socket interface to create a socket, bind an address, and connect to the server.
 * 4. After successfully connecting to the server, it uses the select interface to monitor the read and write operations of the socket.
 *    Read corresponds to receiving data from the network, and write corresponds to sending data to the server.
 *    It calls the lwip_read and lwip_write interfaces respectively.
 * 5. When an exception occurs (such as a network exception or the server actively disconnecting the socket), it supports automatic reconnection.
 * 
 * @Notes:
 * 1. After successfully creating a socket, if an exception occurs, the socket needs to be closed when reconnecting or exiting the task.
 * 2. Calling the lwip_getaddrinfowithcid interface will allocate space for the pointer pres.
 *    After using pres, the memory must be freed. Make sure to free the pres memory when the business process exits abnormally to avoid memory leaks.
 */

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_os.h"
#include "liot_sockets.h"
#include "liot_dev.h"


/**********************************************************
 * New Socket API Demo using Liot_SocketOpen interface
 **********************************************************/
#define NEW_SOCKET_DEMO_IP "121.89.205.240"  // Server IP
#define NEW_SOCKET_DEMO_PORT 41006           // Server port
#define NEW_SOCKET_DEMO_INTERVAL 80       // Send interval in seconds
#define NEW_SOCKET_DEMO_MAX_SEND_COUNT 5   // Maximum number of sends
#define NEW_SOCKET_BUFFER_SIZE 128           // Buffer size for send/receive

#define SOCKET_DEMO_RETRY_DELAY_MS 4000  
#define LIOT_REG_NET_WAIT_TIMES   5    // Number of network registration wait retries, maximum number of attempts.
#define LIOT_REG_NET_WAIT_TIMEOUT 60  // Network registration wait timeout, in seconds, maximum time to wait for network registration to complete.

// Global variables
int g_new_socket_fd = -1;
liot_mutex_t g_socket_mutex = NULL;
uint8_t g_send_buffer[NEW_SOCKET_BUFFER_SIZE];
uint8_t g_recv_buffer[NEW_SOCKET_BUFFER_SIZE];


// Function declarations
static int start_data_call(uint8_t nSim, int cid);
static int liot_setup_data_call(uint8_t nSim, int cid);

/**
 * @brief Start a data call after waiting for network registration to complete.
 * This function first waits for network registration to succeed, then calls the liot_start_data_call function
 * to start a data call, and finally sleeps for a period of time to wait for the call to complete.
 * 
 * @param nSim SIM card number
 * @param cid Data connection ID
 * @return int Data call result. LIOT_DATACALL_SUCCESS indicates success, otherwise failure.
 */
static int start_data_call(uint8_t nSim, int cid) {
    int times = 0;         // Attempt counter
    int ret = -1;          // Stores return value of liot_network_register_wait function

    // Loop to attempt network registration, maximum 10 times
    while (LIOT_DATACALL_SUCCESS != (ret = liot_network_register_wait(nSim, LIOT_REG_NET_WAIT_TIMEOUT)) && times < LIOT_REG_NET_WAIT_TIMES) {
        times++;
        liot_trace("====liot_network_register_wait ret: %x===", ret);
        // 1 second interval between attempts
        liot_rtos_task_sleep_s(1);
    }

    if (ret != LIOT_DATACALL_SUCCESS) {
        liot_trace("====network register failure before data call!!!!!====");
        return ret;
    }

    liot_trace("===start data call====");
    // Start data call
    ret = liot_start_data_call(nSim, cid, LIOT_DATA_TYPE_IPV4V6, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    // Sleep to wait for call completion
    liot_rtos_task_sleep_ms(SOCKET_DEMO_RETRY_DELAY_MS);
    return ret;
}


/**
 * @brief Encapsulate data call - related operations.
 * This function sets the data call asynchronous mode, starts the data call, and checks if the call is successful.
 * 
 * @param nSim SIM card number
 * @param cid Data connection ID
 * @return int Returns LIOT_DATACALL_SUCCESS on successful data call, other error codes on failure.
 */
static int liot_setup_data_call(uint8_t nSim, int cid) {
    // Set the data call asynchronous mode
    liot_set_data_call_asyn_mode(nSim, cid, 0);

    // Start the data call
    if (start_data_call(nSim, cid) != LIOT_DATACALL_SUCCESS) {
        liot_trace("====data call start failure!!!!!====");
        return -1;
    }
    return LIOT_DATACALL_SUCCESS;
}



/**
 * @brief Socket event callback function
 * @param event Socket event type
 * @param socket_fd Socket file descriptor
 * @param data Received data buffer
 * @param data_len Received data length
 * @param user_data User data
 */
void new_socket_callback(Liot_SocketEvent_e event, int socket_fd, uint8_t *data, uint32_t data_len, void *user_data)
{
    switch (event) {
        case LIOT_SOCKET_CONNECT_SUCCESS:
            liot_trace("Socket connected successfully");
            g_new_socket_fd = socket_fd;
            break;
        case LIOT_SOCKET_RECV_DATA:
            liot_trace("Socket LIOT_SOCKET_RECV_DATA data_len: %ld", data_len);
            if (data && data_len > 0) {
                // Copy received data to buffer
                int copy_len = data_len < NEW_SOCKET_BUFFER_SIZE - 1 ? data_len : NEW_SOCKET_BUFFER_SIZE - 1;
                memcpy(g_recv_buffer, data, copy_len);
                g_recv_buffer[copy_len] = '\0';
                liot_trace("Received data: %s", g_recv_buffer);
            }
            break;
        case LIOT_SOCKET_DISCONNECT:
            liot_trace("Socket disconnected");
            g_new_socket_fd = -1;
            break;
        default:
            liot_trace("Unknown socket event: %d", event);
            break;
    }
}

/**
 * @brief New socket demo task
 * @param arg Task argument
 */
void liot_socket2_demo_thread(void *arg)
{
    int i = 0;
    int ret = LIOT_DATACALL_SUCCESS;
    int nSim = 0;
    int cid = 1;
    Liot_SocketInfo_t socket_info;
    
    liot_trace("New Socket Demo Task started");
    
      // Call the encapsulated data dial function
    if (liot_setup_data_call(nSim, cid) != LIOT_DATACALL_SUCCESS) {
        goto exit;
    }

    liot_trace("Data call started successfully");
    
    // Wait for data call to be ready
    liot_rtos_task_sleep_ms(2000);
    
    // Configure socket information
    memset(&socket_info, 0, sizeof(socket_info));
    socket_info.cid = cid;
    socket_info.socket_type = LIOT_SOCKET_TYPE_TCP;  // Use the correct enum value for TCP
    strncpy((char *)socket_info.host, NEW_SOCKET_DEMO_IP, sizeof(socket_info.host) - 1);
    socket_info.port = NEW_SOCKET_DEMO_PORT;
    socket_info.local_port = 0;  // Use any available local port
    socket_info.keepidle = 20;  // 30 seconds keepalive idle time
    socket_info.keepinterval = 35;  // 60 seconds keepalive interval
    socket_info.keepcount = 5;    // 5 keepalive counts

    // Create and connect socket
    g_new_socket_fd = Liot_SocketOpen(&socket_info, new_socket_callback, NULL);
    if (g_new_socket_fd < 0) {
        liot_trace("Failed to open socket");
        goto stop_data_call;
    }
    liot_trace("Socket opened successfully, fd: %d", g_new_socket_fd);
    
    liot_rtos_task_sleep_ms(500);
    // Send periodic data
    for (i = 0; i < NEW_SOCKET_DEMO_MAX_SEND_COUNT; i++) {
        if (g_new_socket_fd == -1) {
            liot_trace("Socket is closed, exiting loop");
            break;
        }
        
        // Send data
        sprintf((char *)g_send_buffer, "Periodic message %d", i);
        int send_len = Liot_SocketSend(g_new_socket_fd, g_send_buffer, strlen((char *)g_send_buffer));
        if (send_len > 0) {
            liot_trace("Sent data: %s, length: %d", g_send_buffer, send_len);
        } else {
            liot_trace("Failed to send data");
        }
        
        // Wait for next send
        liot_rtos_task_sleep_s(NEW_SOCKET_DEMO_INTERVAL);
    }
    ret = Liot_SocketGetStatus(g_new_socket_fd);
    liot_trace("Liot_SocketGetStatus ret: %d", ret);
    // Close socket
    if (g_new_socket_fd != -1) {
        Liot_SocketClose(g_new_socket_fd);
        g_new_socket_fd = -1;
        liot_trace("Socket closed");
    }
    
stop_data_call:
    // Stop data call
    ret = liot_stop_data_call(nSim, cid);
    if (LIOT_DATACALL_SUCCESS != ret) {
        liot_trace("liot_stop_data_call failed, ret: 0x%x", ret);
    }
    
exit:
    liot_trace("New Socket Demo Task exiting");
    
    // Delete the mutex
    if (g_socket_mutex != NULL) {
        liot_rtos_mutex_delete(g_socket_mutex);
        g_socket_mutex = NULL;
    }
    
    // Delete the task
    liot_rtos_task_delete(NULL);
}
