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

// Define constants
#define LIOT_SOCKET_DEMO_IPV4_TEST 1
#define MAX_TEST_CYCLES 100              // Maximum number of test cycles. Data transmission test stops after reaching this number.
#define SOCKET_DEMO_RETRY_DELAY_MS 4000  // Delay time after data call, in milliseconds, used to wait for the call to complete.
#define SOCKET_BUFFER_SIZE 128           // Size of the send and receive buffers, used to store sent and received data.
#define MAX_RECONNECT_TIMES 5            // Maximum number of reconnections. Maximum number of reconnection attempts when the connection fails.
#define RECONNECT_DELAY_MS 2000          // Delay time for each reconnection, in milliseconds, time to wait before reconnecting.
#define LIOT_REG_NET_WAIT_TIMEOUT 120    // Network registration wait timeout, in milliseconds, maximum time to wait for network registration to complete.
#define LIOT_REG_NET_WAIT_TIMES   10     // Number of network registration wait retries, maximum number of attempts.
#define LIOT_CONNECT_S_TIMEOUT      10   // Socket connection timeout, in seconds, maximum time to wait for the connection operation.


#if LIOT_SOCKET_DEMO_IPV4_TEST
#define LIOT_SOCKET_DEMO_URL "121.89.205.240"
#define LIOT_SOCKET_DEMO_PORT 41002
#else
#define LIOT_SOCKET_DEMO_URL  "2408:4001:214:d20b:8847:326e:f728:5f5c"
#define LIOT_SOCKET_DEMO_PORT 41000
#endif

static int s_reconnect_count = 0;     // Number of reconnections

// Function declarations
static int start_data_call(uint8_t nSim, int cid);
static int get_data_call_info(uint8_t nSim, int cid, liot_data_call_info_t *socket_info);
static void free_addrinfo_safe(struct addrinfo **pres);
static void liot_handle_socket_operations(int socFd, int *idx);
static int liot_create_socket_and_connect(int cid, const char *url, uint16_t port, const liot_data_call_info_t *socket_info);
static int liot_handle_reconnect(int cid, const char *url, uint16_t port, const liot_data_call_info_t *socket_info);
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
 * @brief Get data call information.
 * This function calls the liot_get_data_call_info function to obtain data call information and prints relevant information.
 * 
 * @param nSim SIM card number
 * @param cid Data connection ID
 * @param socket_info Pointer to the structure storing data call information
 * @return int Acquisition result. LIOT_DATACALL_SUCCESS indicates success, otherwise failure.
 */
static int get_data_call_info(uint8_t nSim, int cid, liot_data_call_info_t *socket_info) {
    int ret = liot_get_data_call_info(nSim, cid, socket_info);
    if (LIOT_DATACALL_SUCCESS != ret) {
        liot_trace("liot_get_data_call_info failed, ret: 0x%x.", ret);
        return ret;
    }
    // Print data call information
    liot_trace("socket_info.cid: %ld", socket_info->cid);
    liot_trace("socket_info.ip_version: %ld", socket_info->ip_version);
    liot_trace("socket_info.apn_name: %s.", socket_info->apn_name);
    return ret;
}

/**
 * @brief Create a socket and connect to the server.
 * This function first performs DNS resolution, then attempts to create a socket and bind a local address, 
 * and finally connects to the server.
 * 
 * @param cid Data connection ID
 * @param url Server address
 * @param port Server port
 * @param socket_info Pointer to the data call information structure
 * @return int Returns the socket descriptor on success, -1 on failure.
 */
static int liot_create_socket_and_connect(int cid, const char *url, uint16_t port, const liot_data_call_info_t *socket_info) {
    static struct addrinfo *pres = NULL;
    // Perform DNS resolution
    int ret = lwip_getaddrinfowithcid(url, NULL, NULL, &pres, cid);

    if (ret < 0 || NULL == pres) {
        liot_trace("DNS getaddrinfo failed! ret=%d, pres=%p!", ret, pres);
        return -1;
    }

    struct addrinfo *temp = NULL;
    // Iterate through the DNS resolution results and attempt to create and connect the socket
    for (temp = pres; temp != NULL; temp = temp->ai_next) {
        int socFd;
#if LIOT_SOCKET_DEMO_IPV4_TEST
        struct sockaddr_in local4;
        // Initialize the local IPv4 address structure
        memset(&local4, 0x00, sizeof(struct sockaddr_in));
        local4.sin_family = AF_INET;
        local4.sin_port = 0;

        // Create a non - const copy of the IPv4 address
        liot_ip4_addr_t non_const_ip4_addr = socket_info->v4.addr.ip;
        // Convert the local IPv4 address to network byte order
        (void)liot_ip4addr_aton(liot_ip4addr_ntoa(&non_const_ip4_addr), (liot_ip4_addr_t *)&local4.sin_addr);

        // Create an IPv4 socket
        socFd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (socFd < 0) {
            continue;
        }

        // Bind the local IPv4 address
        ret = lwip_bind(socFd, (struct sockaddr *)&local4, sizeof(struct sockaddr_in));
#else
        struct sockaddr_in6 local6;
        // Initialize the local IPv6 address structure
        memset(&local6, 0x00, sizeof(struct sockaddr_in6));
        local6.sin6_family = AF_INET6;
        local6.sin6_port = 0;

        // Create a non - const copy of the IPv6 address
        liot_ip6_addr_t non_const_ip6_addr = socket_info->v6.addr.ip;

        // Convert the local IPv6 address to network byte order
        (void)liot_ip6addr_aton(liot_ip6addr_ntoa(&non_const_ip6_addr), (liot_ip6_addr_t *)&local6.sin6_addr);

        // Create an IPv6 socket
        socFd = lwip_socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);
        if (socFd < 0) {
            continue;
        }

        // Bind the local IPv6 address
        ret = lwip_bind(socFd, (struct sockaddr *)&local6, sizeof(struct sockaddr_in6));
#endif
        if (ret < 0) {
            // Binding failed, close the socket and try the next address
            lwip_close(socFd);
            socFd = -1;
            continue;
        }

        // Set the socket to non - blocking mode
        int flags = O_NONBLOCK;
        lwip_fcntl(socFd, F_SETFL, flags);

#if LIOT_SOCKET_DEMO_IPV4_TEST
        struct sockaddr_in server_ipv4;
        // Initialize the server IPv4 address structure
        memset(&server_ipv4, 0x00, sizeof(struct sockaddr_in));
        server_ipv4.sin_family = temp->ai_family;
        // Convert the server port to network byte order
        server_ipv4.sin_port = htons(port);
        server_ipv4.sin_addr = ((struct sockaddr_in *)temp->ai_addr)->sin_addr;

        // Connect to the server IPv4 address
        ret = lwip_connect(socFd, (struct sockaddr *)&server_ipv4, sizeof(server_ipv4));
#else
        struct sockaddr_in6 server_ipv6;
        // Initialize the server IPv6 address structure
        memset(&server_ipv6, 0x00, sizeof(struct sockaddr_in6));
        server_ipv6.sin6_family = temp->ai_family;
        // Convert the server port to network byte order
        server_ipv6.sin6_port = htons(port);
        server_ipv6.sin6_addr = ((struct sockaddr_in6 *)temp->ai_addr)->sin6_addr;

        // Connect to the server IPv6 address
        ret = lwip_connect(socFd, (struct sockaddr *)&server_ipv6, sizeof(server_ipv6));
#endif
        liot_trace("===socket demo %d====ret: %d", __LINE__, ret);

        if (ret < 0) {
            if (errno == EINPROGRESS || errno == 0) {
                // Connection is in progress, use select to check if the connection is complete
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(socFd, &write_fds);

                struct timeval timeout;
                timeout.tv_sec = LIOT_CONNECT_S_TIMEOUT;   // Set the timeout
                timeout.tv_usec = 0;

                ret = lwip_select(socFd + 1, NULL, &write_fds, NULL, &timeout);
                if (ret > 0) {
                    int error;
                    socklen_t len = sizeof(error);
                    if (lwip_getsockopt(socFd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                        // Connection successful
                        free_addrinfo_safe(&pres);
                        return socFd;
                    }
                }
            }
            // Connection failed, close the socket and try the next address
            lwip_close(socFd);
            continue;
        }

        // Connection successful, release DNS resolution result resources
        free_addrinfo_safe(&pres);
        return socFd;
    }
    // All addresses failed, release DNS resolution result resources
    free_addrinfo_safe(&pres);
    return -1;
}

/**
 * @brief Handle socket operations, including data reading, writing, and event monitoring.
 * This function uses the select function to monitor read, write, and exception events of the socket, 
 * and performs corresponding processing based on the event type.
 * 
 * @param socFd Socket descriptor
 * @param idx Pointer to the loop counter, recording the number of data transmissions.
 */
static void liot_handle_socket_operations(int socFd, int *idx) {
    char send_buf[SOCKET_BUFFER_SIZE] = {0};  // Send buffer
    char recv_buf[SOCKET_BUFFER_SIZE] = {0};  // Receive buffer
    int send_len = -1;        // Length of sent data
    int recv_len = -1;        // Length of received data

    fd_set read_fds;          // Read event set
    fd_set write_fds;         // Write event set
    fd_set exp_fds;           // Exception event set

    int connected = 0;        // Connection status flag, 1 indicates connected
    int closing = false;      // Closing status flag, true indicates closing

    // Clear the event sets
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exp_fds);

    if (socFd >= 0) {
        // Add the socket descriptor to the event sets
        FD_SET(socFd, &read_fds);
        FD_SET(socFd, &write_fds);
        FD_SET(socFd, &exp_fds);
    }

    // Loop to monitor events
    while (1) {
        liot_trace("===socket demo %d====", __LINE__);
        // Use the select function to monitor events
        int fdChanged = lwip_select(socFd + 1, &read_fds, &write_fds, &exp_fds, NULL);
        liot_trace("===socket demo fdChanged:%d, line:%d===", fdChanged, __LINE__);

        if (fdChanged > 0) {
            if (FD_ISSET(socFd, &write_fds)) {
                // Write event triggered
                FD_CLR(socFd, &write_fds);
                if (0 == connected) {
                    int value = 0;
                    socklen_t len = sizeof(value);
                    // Get the socket error code
                    lwip_getsockopt(socFd, SOL_SOCKET, SO_ERROR, &value, &len);

                    liot_trace("errno: %d!", value);
                    if (value == 0 || value == EISCONN) {
                        // Connection successful
                        liot_trace("=====connect to \"%s:%d\" success=====", LIOT_SOCKET_DEMO_URL, LIOT_SOCKET_DEMO_PORT);
                        connected = 1;
                        // Add the socket descriptor to the read event set
                        FD_SET(socFd, &read_fds);

                        // Construct and send data
                        send_len = snprintf(send_buf, SOCKET_BUFFER_SIZE, "%d%s%d\r\n", *idx,
                                            "startAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAend", *idx);
                        lwip_write(socFd, send_buf, send_len);
                        (*idx)++;
                    } else {
                        // Connection failed, close the socket
                        liot_trace("=====connect to \"%s:%d\" failed=====", LIOT_SOCKET_DEMO_URL, LIOT_SOCKET_DEMO_PORT);
                        lwip_close(socFd);
                        break;
                    }
                } else {
                    // Already connected, construct and send data
                    send_len = snprintf(send_buf, SOCKET_BUFFER_SIZE, "%d%s%d\r\n", *idx,
                                        "startAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAend", *idx);
                    lwip_write(socFd, send_buf, send_len);
                    (*idx)++;
                }
            } else if (FD_ISSET(socFd, &read_fds)) {
                // Read event triggered
                FD_CLR(socFd, &read_fds);
                // Clear the receive buffer
                memset(recv_buf, 0x00, SOCKET_BUFFER_SIZE);
                // Read data
                recv_len = lwip_read(socFd, recv_buf, SOCKET_BUFFER_SIZE);
                if (recv_len > 0) {
                    // Valid data received, print and send response data
                    liot_trace(">>>>Recv: %s", recv_buf);
                    send_len = snprintf(send_buf, SOCKET_BUFFER_SIZE, "%d%s%d\r\n", *idx,
                                        "startAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAend", *idx);
                    lwip_write(socFd, send_buf, send_len);
                    (*idx)++;
                    // Add the socket descriptor to the read event set
                    FD_SET(socFd, &read_fds);
                } else if (recv_len == 0) {
                    if (closing == false) {
                        // The peer closed the connection, send a FIN packet
                        liot_trace("===passive close!!!!");
                        lwip_shutdown(socFd, SHUT_WR);
                        closing = true;
                        // Add the socket descriptor to the read event set
                        FD_SET(socFd, &read_fds);
                    } else {
                        // Close the socket
                        lwip_close(socFd);
                        break;
                    }
                }
            } else if (FD_ISSET(socFd, &exp_fds)) {
                // Exception event triggered, close the socket
                FD_CLR(socFd, &exp_fds);
                lwip_close(socFd);
                break;
            }
        }

        if (MAX_TEST_CYCLES == *idx) {
            // Reached the maximum number of test cycles, close the socket
            lwip_close(socFd);
            break;
        }
    }
}

/**
 * @brief Safely release addrinfo resources.
 * This function checks if the addrinfo pointer is not null. If so, it releases the resources and sets the pointer to NULL.
 * 
 * @param pres Pointer to the addrinfo pointer
 */
static void free_addrinfo_safe(struct addrinfo **pres) {
    if (*pres) {
        // Release addrinfo resources
        lwip_freeaddrinfo(*pres);
        *pres = NULL;
    }
}

/**
 * @brief Handle reconnection logic.
 * This function attempts to reconnect when the connection fails. It sleeps for a period of time before each reconnection. 
 * If the maximum number of reconnections is reached, it stops reconnecting.
 * 
 * @param cid Data connection ID
 * @param url Server address
 * @param port Server port
 * @param socket_info Pointer to the data call information structure
 * @return int Returns 0 on successful reconnection, -1 when the maximum number of reconnections is reached.
 */
static int liot_handle_reconnect(int cid, const char *url, uint16_t port, const liot_data_call_info_t *socket_info) {
    if (s_reconnect_count >= MAX_RECONNECT_TIMES) {
        // Reached the maximum number of reconnections, return -1
        return -1;
    }
    liot_trace("Connection failed, try to reconnect %d/%d", s_reconnect_count + 1, MAX_RECONNECT_TIMES);
    // Sleep for a while before reconnecting
    liot_rtos_task_sleep_ms(RECONNECT_DELAY_MS);
    s_reconnect_count++;
    // Recursively call to attempt to reconnect again
    return liot_create_socket_and_connect(cid, url, port, socket_info) < 0 ? liot_handle_reconnect(cid, url, port, socket_info) : 0;
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
 * @brief Socket demo task function.
 * This function implements the complete process of network registration, data call, DNS resolution, socket connection, and data transmission.
 * Main steps:
 * 1. Wait for network registration to succeed. (Merged into start_data_call)
 * 2. Initiate a data call.
 * 3. Create and connect a socket.
 * 4. Use select to monitor read, write, and exception events.
 * 5. Reconnect when a network exception or the server actively disconnects occurs.
 * 
 * @param argv Thread parameter, not used.
 */
void liot_sockets_demo_thread(void *argv) {
    (void)argv;
    uint8_t nSim = 0;      // SIM card number
    int cid = 2;           // Data connection ID
    int ret = -1;          // Stores function return values to check operation success

    // Sleep for 2 seconds after thread starts to wait for system initialization
    liot_rtos_task_sleep_ms(2000);
    liot_trace("==========socket demo start==========");

    // Call the encapsulated data dial function
    if (liot_setup_data_call(nSim, cid) != LIOT_DATACALL_SUCCESS) {
        goto exit;
    }

    liot_data_call_info_t socket_info;
    // Initialize data call information structure
    memset(&socket_info, 0x00, sizeof(liot_data_call_info_t));

    // Get data call information
    if (get_data_call_info(nSim, cid, &socket_info) != LIOT_DATACALL_SUCCESS) {
        goto exit;
    }

    int idx = 0;           // Loop counter, tracks number of data transmissions
    int socFd = -1;        // Socket descriptor, initialized to -1 (invalid)

    // Perform data transmission tests until maximum test cycles reached
    while (idx < MAX_TEST_CYCLES) {
        // Create and connect socket
        socFd = liot_create_socket_and_connect(cid, LIOT_SOCKET_DEMO_URL, LIOT_SOCKET_DEMO_PORT, &socket_info);
        if (socFd < 0) {
            // Connection failed, attempt reconnection
            if (liot_handle_reconnect(cid, LIOT_SOCKET_DEMO_URL, LIOT_SOCKET_DEMO_PORT, &socket_info) < 0) {
                liot_trace("Max reconnect times reached, exit.");
                break;
            }
            continue;
        }
        // Connection successful, reset reconnection counter
        s_reconnect_count = 0;

        // Handle socket operations
        liot_handle_socket_operations(socFd, &idx);

        // Close socket
        lwip_close(socFd);
        socFd = -1;
    }
 exit:
    liot_trace("Max reconnect times reached, exit.");
    s_reconnect_count = 0;
    // Stop data call
    ret = liot_stop_data_call(nSim, cid);
    if (LIOT_DATACALL_SUCCESS != ret) {
        liot_trace("liot_stop_data_call ret: 0x%x.", ret);
    }

    liot_rtos_task_delete(NULL);
}