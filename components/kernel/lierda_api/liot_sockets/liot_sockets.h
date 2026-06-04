/**
 * @File Name: liot_sockets.h
 * @brief
 * @Author : yanpp email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef _LIOT_SOCKETS_H_
#define _LIOT_SOCKETS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/netdb.h"
#include "lwip/sockets.h"

/**
 * @brief Accept a new connection on a socket.
 * @param s The listening socket descriptor.
 * @param addr A pointer to a struct sockaddr to store the client address.
 * @param addrlen A pointer to the length of the addr structure.
 * @return The new socket descriptor on success, -1 on error.
 */
extern int lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen);

/**
 * @brief Bind a name to a socket.
 * @param s The socket descriptor.
 * @param name A pointer to a struct sockaddr containing the address to bind.
 * @param namelen The length of the name structure.
 * @return 0 on success, -1 on error.
 */
extern int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen);

/**
 * @brief Shutdown part of a full - duplex connection on a socket.
 * @param s The socket descriptor.
 * @param how Specifies what types of operation will no longer be allowed.
 * @return 0 on success, -1 on error.
 */
extern int lwip_shutdown(int s, int how);

/**
 * @brief Close a socket descriptor.
 * @param s The socket descriptor to close.
 * @return 0 on success, -1 on error.
 */
extern int lwip_close(int s);

/**
 * @brief Connect a socket to a specified address.
 * @param s The socket descriptor.
 * @param name A pointer to a struct sockaddr containing the address to connect to.
 * @param namelen The length of the name structure.
 * @return 0 on success, -1 on error.
 */
extern int lwip_connect(int s, const struct sockaddr *name, socklen_t namelen);

/**
 * @brief Mark a socket as a passive socket, i.e., as a socket that will be used to accept incoming connection requests.
 * @param s The socket descriptor.
 * @param backlog The maximum length to which the queue of pending connections may grow.
 * @return 0 on success, -1 on error.
 */
extern int lwip_listen(int s, int backlog);

/**
 * @brief Receive data from a socket.
 * @param s The socket descriptor.
 * @param mem A pointer to the buffer where the received data will be stored.
 * @param len The maximum number of bytes to receive.
 * @param flags Flags that modify the behavior of the receive operation.
 * @return The number of bytes received on success, -1 on error.
 */
extern int lwip_recv(int s, void *mem, size_t len, int flags);

/**
 * @brief Read data from a socket.
 * @param s The socket descriptor.
 * @param mem A pointer to the buffer where the read data will be stored.
 * @param len The maximum number of bytes to read.
 * @return The number of bytes read on success, -1 on error.
 */
extern int lwip_read(int s, void *mem, size_t len);

/**
 * @brief Receive data from a socket and get the source address.
 * @param s The socket descriptor.
 * @param mem A pointer to the buffer where the received data will be stored.
 * @param len The maximum number of bytes to receive.
 * @param flags Flags that modify the behavior of the receive operation.
 * @param from A pointer to a struct sockaddr to store the source address.
 * @param fromlen A pointer to the length of the from structure.
 * @return The number of bytes received on success, -1 on error.
 */
extern int lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);

/**
 * @brief Send data on a socket.
 * @param s The socket descriptor.
 * @param dataptr A pointer to the buffer containing the data to send.
 * @param size The number of bytes to send.
 * @param flags Flags that modify the behavior of the send operation.
 * @return The number of bytes sent on success, -1 on error.
 */
extern int lwip_send(int s, const void *dataptr, size_t size, int flags);

/**
 * @brief Send a message on a socket.
 * @param s The socket descriptor.
 * @param message A pointer to a struct msghdr containing the message to send.
 * @param flags Flags that modify the behavior of the send operation.
 * @return The number of bytes sent on success, -1 on error.
 */
extern int lwip_sendmsg(int s, const struct msghdr *message, int flags);

/**
 * @brief Send data to a specified address on a socket.
 * @param s The socket descriptor.
 * @param dataptr A pointer to the buffer containing the data to send.
 * @param size The number of bytes to send.
 * @param flags Flags that modify the behavior of the send operation.
 * @param to A pointer to a struct sockaddr containing the destination address.
 * @param tolen The length of the to structure.
 * @return The number of bytes sent on success, -1 on error.
 */
extern int lwip_sendto(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);

/**
 * @brief Create a new socket.
 * @param domain The protocol family to use.
 * @param type The type of socket to create.
 * @param protocol The specific protocol to use.
 * @return The new socket descriptor on success, -1 on error.
 */
extern int lwip_socket(int domain, int type, int protocol);

/**
 * @brief Write data to a socket.
 * @param s The socket descriptor.
 * @param dataptr A pointer to the buffer containing the data to write.
 * @param size The number of bytes to write.
 * @return The number of bytes written on success, -1 on error.
 */
extern int lwip_write(int s, const void *dataptr, size_t size);

/**
 * @brief Write data from a scatter - gather list to a socket.
 * @param s The socket descriptor.
 * @param iov A pointer to an array of struct iovec structures.
 * @param iovcnt The number of elements in the iov array.
 * @return The number of bytes written on success, -1 on error.
 */
extern int lwip_writev(int s, const struct iovec *iov, int iovcnt);

/**
 * @brief Wait for an I/O event on one or more file descriptors.
 * @param maxfdp1 The maximum file descriptor value plus 1.
 * @param readset A pointer to a fd_set containing the file descriptors to monitor for read events.
 * @param writeset A pointer to a fd_set containing the file descriptors to monitor for write events.
 * @param exceptset A pointer to a fd_set containing the file descriptors to monitor for exception events.
 * @param timeout A pointer to a struct timeval specifying the maximum time to wait.
 * @return The number of ready file descriptors on success, 0 on timeout, -1 on error.
 */
extern int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout);

/**
 * @brief Control device I/O behavior.
 * @param s The socket descriptor.
 * @param cmd The command to execute.
 * @param argp A pointer to the argument for the command.
 * @return 0 on success, -1 on error.
 */
extern int lwip_ioctl(int s, long cmd, void *argp);

/**
 * @brief Manipulate file descriptor flags.
 * @param s The socket descriptor.
 * @param cmd The command to execute.
 * @param val The value to use for the command.
 * @return The new value of the flag on success, -1 on error.
 */
extern int lwip_fcntl(int s, int cmd, int val);

/**
 * @brief Get host information by name.
 * @param name The host name.
 * @return A pointer to a struct hostent containing the host information on success, NULL on error.
 */
extern struct hostent *lwip_gethostbyname(const char *name);

/**
 * @brief Reentrant version of lwip_gethostbyname.
 * @param name The host name.
 * @param ret A pointer to a struct hostent to store the host information.
 * @param buf A buffer to use for storing the host information.
 * @param buflen The length of the buf buffer.
 * @param result A pointer to a pointer to a struct hostent to return the result.
 * @param h_errnop A pointer to an integer to store the error code.
 * @return 0 on success, non - zero on error.
 */
extern int lwip_gethostbyname_r(
    const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop);

/**
 * @brief Free the memory allocated by lwip_getaddrinfo.
 * @param ai A pointer to the struct addrinfo to free.
 */
extern void lwip_freeaddrinfo(struct addrinfo *ai);

/**
 * @brief Translate host and service names into socket addresses.
 * @param nodename The host name.
 * @param servname The service name.
 * @param hints A pointer to a struct addrinfo containing hints about the desired socket type.
 * @param res A pointer to a pointer to a struct addrinfo to return the result.
 * @return 0 on success, non - zero on error.
 */
extern int lwip_getaddrinfo(const char *nodename,
                            const char *servname,
                            const struct addrinfo *hints,
                            struct addrinfo **res);

/**
 * @brief Convert an IPv4 address string to a 32 - bit integer.
 * @param cp A pointer to the IPv4 address string.
 * @return The 32 - bit integer representation of the IPv4 address on success, INADDR_NONE on error.
 */
extern u32_t ipaddr_addr(const char *cp);

/**
 * @brief Convert an IPv4 address string to a binary representation.
 * @param cp A pointer to the IPv4 address string.
 * @param addr A pointer to an ip4_addr_t structure to store the binary representation.
 * @return 1 on success, 0 on error.
 */
extern int ip4addr_aton(const char *cp, ip4_addr_t *addr);

/**
 * @brief Convert an IPv4 address from binary to string representation.
 * @param addr A pointer to an ip4_addr_t structure containing the binary representation.
 * @param buf A buffer to store the string representation.
 * @param buflen The length of the buf buffer.
 * @return A pointer to the buf buffer containing the string representation.
 */
extern char *ip4addr_ntoa_r(const ip4_addr_t *addr, char *buf, int buflen);

/**
 * @brief Convert an IPv6 address string to a binary representation.
 * @param cp A pointer to the IPv6 address string.
 * @param addr A pointer to an ip6_addr_t structure to store the binary representation.
 * @return 1 on success, 0 on error.
 */
extern int ip6addr_aton(const char *cp, ip6_addr_t *addr);

/**
 * @brief Convert an IPv6 address from binary to string representation.
 * @param addr A pointer to an ip6_addr_t structure containing the binary representation.
 * @param buf A buffer to store the string representation.
 * @param buflen The length of the buf buffer.
 * @return A pointer to the buf buffer containing the string representation.
 */
extern char *ip6addr_ntoa_r(const ip6_addr_t *addr, char *buf, int buflen);

/**
 * @brief Translate host and service names into socket addresses with a specific CID.
 * @param nodename The host name.
 * @param servname The service name.
 * @param hints A pointer to a struct addrinfo containing hints about the desired socket type.
 * @param res A pointer to a pointer to a struct addrinfo to return the result.
 * @param cid The CID to use.
 * @return 0 on success, non - zero on error.
 */
extern int lwip_getaddrinfowithcid(
    const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res, uint8_t cid);

/**
 * @brief Get the current value of a socket option.
 * @param s The socket descriptor.
 * @param level The level at which the option resides.
 * @param optname The option name.
 * @param optval A pointer to a buffer to store the option value.
 * @param optlen A pointer to the length of the optval buffer.
 * @return 0 on success, -1 on error.
 */
extern int lwip_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);

/**
 * @brief Convert a 16 - bit integer from host byte order to network byte order.
 * @param x The 16 - bit integer to convert.
 * @return The converted 16 - bit integer.
 */
extern u16_t lwip_htons(u16_t x);

/**
 * @brief Convert a 32 - bit integer from host byte order to network byte order.
 * @param x The 32 - bit integer to convert.
 * @return The converted 32 - bit integer.
 */
extern u32_t lwip_htonl(u32_t x);

/**
 * @brief Set the value of a socket option.
 * @param s The socket descriptor.
 * @param level The level at which the option resides.
 * @param optname The option name.
 * @param optval A pointer to a buffer containing the option value.
 * @param optlen The length of the optval buffer.
 * @return 0 on success, -1 on error.
 */
extern int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);

/**
 * @brief Get the local address of a socket.
 * @param s The socket descriptor.
 * @param name A pointer to a struct sockaddr to store the local address.
 * @param namelen A pointer to the length of the name structure.
 * @return 0 on success, -1 on error.
 */
extern int lwip_getsockname(int s, struct sockaddr *name, socklen_t *namelen);

/**
 * @brief Calculate the Internet checksum.
 * @param dataptr A pointer to the data buffer.
 * @param len The length of the data buffer.
 * @return The calculated checksum.
 */
extern u16_t inet_chksum(const void *dataptr, u16_t len);

/**
 * @brief Get the peer address of a socket.
 * @param s The socket descriptor.
 * @param name A pointer to a struct sockaddr to store the peer address.
 * @param namelen A pointer to the length of the name structure.
 * @return 0 on success, -1 on error.
 */
extern int lwip_getpeername(int s, struct sockaddr *name, socklen_t *namelen);

/**
 * @brief Get the error number associated with a socket.
 * @param s The socket descriptor.
 * @return The error number.
 */
extern int sock_get_errno(int s);


typedef enum
{
    LIOT_SOCKET_CONNECT_SUCCESS = 0,
    LIOT_SOCKET_DNS_FAIL = 1,
    LIOT_SOCKET_CONNECT_FAIL = 2,
    LIOT_SOCKET_RECV_DATA = 3,
    LIOT_SOCKET_DISCONNECT = 4,
}Liot_SocketEvent_e;

typedef enum
{
    LIOT_SOCKET_TYPE_TCP = 0,
    LIOT_SOCKET_TYPE_UDP = 1,

}Liot_Socket_Type_e;

typedef void (*liot_socket_callback)(Liot_SocketEvent_e event, int socket_fd, uint8_t *data, uint32_t data_len, void *user_data);

typedef struct
{
    int socket_fd;          // socket fd 0-7
    uint8_t cid;            // cid 0-7
    uint8_t socket_type;    //TCP UDP
    uint8_t host[256];      //host name 域名或者IP
    uint16_t port;          //端口号
    uint16_t local_port;    //本地端口
    int32_t keepidle;       //心跳检测间隔
    int32_t keepinterval;   //心跳检测间隔
    int32_t keepcount;      //心跳检测次数
}Liot_SocketInfo_t;


/**
 * @brief 打开一个套接字。
 *
 * 该函数通过提供的 [liot_socket_info_t](file:///home/zlc/EC718/cat1_ec718_general/PLAT/new_pai.h#L122-L135) 配置结构体初始化并打开一个套接字。
 * 它根据主机名、端口、套接字类型等参数建立连接。
 *
 * @param info 指向包含套接字配置信息的 [liot_socket_info_t](file:///home/zlc/EC718/cat1_ec718_general/PLAT/new_pai.h#L122-L135) 结构体指针。
 * @return int32_t 成功返回 0，失败返回负值。
 */
int32_t Liot_SocketOpen(Liot_SocketInfo_t *info, liot_socket_callback callback, void *userdata);

/**
 * @brief 通过套接字发送数据。
 *
 * 该函数通过指定的套接字文件描述符 [fd](file:///home/zlc/EC718/cat1_ec718_general/PLAT/middleware/thirdparty/lzma2201/C/7zFile.h#L32-L32) 发送数据缓冲区中的内容。
 *
 * @param fd 通过 [liot_SocketOpen](file:///home/zlc/EC718/cat1_ec718_general/PLAT/new_pai.h#L138-L138) 获取的套接字文件描述符。
 * @param data 指向要发送的数据缓冲区。
 * @param len 要发送的数据长度（以字节为单位）。
 * @return int32_t 成功返回发送的字节数，失败返回负值。
 */
int32_t Liot_SocketSend(int32_t fd, uint8_t *data, uint16_t len);

/**
 * @brief 获取套接字的当前状态。
 *
 * 该函数检查并返回由指定文件描述符 [fd](file:///home/zlc/EC718/cat1_ec718_general/PLAT/middleware/thirdparty/lzma2201/C/7zFile.h#L32-L32) 标识的套接字的状态。
 *
 * @param fd 通过 [liot_SocketOpen](file:///home/zlc/EC718/cat1_ec718_general/PLAT/new_pai.h#L138-L138) 获取的套接字文件描述符。
 * @return int32_t 返回套接字状态码，可用于判断套接字是否已连接或断开等。
 */
int32_t Liot_SocketGetStatus(int32_t fd);

/**
 * @brief 关闭一个打开的套接字。
 *
 * 该函数关闭由指定文件描述符 [fd](file:///home/zlc/EC718/cat1_ec718_general/PLAT/middleware/thirdparty/lzma2201/C/7zFile.h#L32-L32) 标识的套接字，并释放相关资源。
 *
 * @param fd 通过 [liot_SocketOpen](file:///home/zlc/EC718/cat1_ec718_general/PLAT/new_pai.h#L138-L138) 获取的套接字文件描述符。
 * @return int32_t 成功返回 0，失败返回负值。
 */
int32_t Liot_SocketClose(int32_t fd);

#ifdef __cplusplus
} /*"C" */
#endif

#endif