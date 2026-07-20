/**
 * @file liot_router.h
 * @brief liotRouter public API
 */

#ifndef __LIOT_ROUTER_H__
#define __LIOT_ROUTER_H__

#include <stdint.h>

/* Utility macros */

/** @brief IP address printf format string (network-order uint32_t -> a.b.c.d) */
#define IP4_FMT          "%d.%d.%d.%d"
#define IP4_UNPACK(ip)   ((uint8_t*)&(ip))[0], ((uint8_t*)&(ip))[1], \
                         ((uint8_t*)&(ip))[2], ((uint8_t*)&(ip))[3]

/* Byte order conversion */

/** @brief 32-bit host to network order */
#define liot_router_htonl(x) ((uint32_t)( \
    (((x) & 0xFF000000U) >> 24) | \
    (((x) & 0x00FF0000U) >> 8)  | \
    (((x) & 0x0000FF00U) << 8)  | \
    (((x) & 0x000000FFU) << 24)))

/** @brief 32-bit network to host order */
#define liot_router_ntohl(x) liot_router_htonl(x)

/** @brief 16-bit host to network order */
#define liot_router_htons(x) ((uint16_t)( (((x) & 0xFF00U) >> 8) | (((x) & 0x00FFU) << 8)))

/** @brief 16-bit network to host order */
#define liot_router_ntohs(x) liot_router_htons(x)

/* Data types */

/** @brief WAN side network information */
typedef struct Liot_RouterWanInfo {
    uint32_t wanIp;       /**< WAN IP address (network order) */
    uint32_t wanMask;     /**< WAN subnet mask (network order) */
    uint32_t wanGw;       /**< WAN gateway (network order) */
    uint32_t dns1;        /**< Primary DNS (network order) */
    uint32_t dns2;        /**< Secondary DNS (network order) */
} Liot_RouterWanInfo_t;

/* LAN layer */

/** @brief LAN interrupt status: packet ready to read */
#define LIOT_LAN_STA_PKT_RDY    (1U << 0)
/** @brief LAN interrupt status: TX complete */
#define LIOT_LAN_STA_PKT_TX     (1U << 1)
/** @brief LAN interrupt status: RX overflow */
#define LIOT_LAN_STA_RX_OVF     (1U << 2)
/** @brief LAN interrupt status: overflow counter overflow */
#define LIOT_LAN_STA_OVF_CNT    (1U << 3)
/** @brief LAN interrupt status: link state changed */
#define LIOT_LAN_STA_LINK_CHG   (1U << 5)

/** @brief LAN port runtime information */
typedef struct Liot_RouterLan {
    uint8_t     mac[6];   /**< MAC address */
    uint8_t     lanPort;  /**< LAN port index */
    uint8_t     linkUp;   /**< Link state: 1=UP, 0=DOWN */
} Liot_RouterLan_t;

/** @brief LAN event callback */
typedef void (*Liot_RouterLanEventCb)(Liot_RouterLan_t *lan);

/** @brief LAN device driver interface */
typedef struct Liot_RouterLanDev {
    int32_t     (*init)(Liot_RouterLan_t *lan, Liot_RouterLanEventCb eventCb);
    void        (*send)(Liot_RouterLan_t *lan, uint8_t *buf, uint16_t len);
    uint32_t    (*receive)(Liot_RouterLan_t *lan, uint8_t *buf);
    uint8_t     (*getIntStatus)(Liot_RouterLan_t *lan);
    uint8_t     (*getLinkStatus)(Liot_RouterLan_t *lan);
} Liot_RouterLanDev_t;

/** @brief Router configuration */
typedef struct Liot_RouterCfg {
    uint8_t  wanNetmode;      /**< Network mode */
    uint8_t  wanCid;          /**< PDP context ID */
    uint8_t  wanTaskPriority; /**< WAN task priority */
    uint8_t  wanMac[6];       /**< WAN MAC address */
    uint32_t dhcpGatewayIp;   /**< DHCP gateway IP (host order) */
    uint32_t dhcpSubnetMask;  /**< DHCP subnet mask (host order) */
    uint32_t dhcpLeaseTime;   /**< DHCP lease time in seconds */
    uint32_t dhcpDns1;        /**< DHCP DNS1 (network order, 0=from WAN) */
    uint32_t dhcpDns2;        /**< DHCP DNS2 (network order) */
} Liot_RouterCfg_t;

/* Public API */

/**
 * @brief Initialize the router
 * @param cfg Router configuration
 */
void Liot_RouterInit(const Liot_RouterCfg_t *cfg);

/**
 * @brief Post a packet to the main processing queue
 * @param pkt     Packet slot handle (from liot_pktpool_alloc), NULL for control msgs
 * @param source  Source identifier (0~N=LAN port, 0xFF=WAN, 0xFE=WAN changed)
 * @return 0 on success, non-zero on failure
 */
int32_t Liot_RouterPostMsg(void *pkt, uint8_t source);

/* WAN interface */

/**
 * @brief Get current WAN network information
 * @return Pointer to WAN info, NULL if not available
 */
Liot_RouterWanInfo_t *Liot_RouterGetWaninfo(void);

/* DHCP interface */

/**
 * @brief Lookup lease by IP address
 * @param ip       [in] Target IP (network order)
 * @param lan_port [out] LAN port index, may be NULL
 * @param mac      [out] MAC address, may be NULL
 * @return 0 if found, -1 if not found
 */
int32_t Liot_RouterDhcpGetHost(uint32_t ip, uint8_t *lan_port, uint8_t *mac);

/**
 * @brief Update or add a lease entry (ARP learning)
 * @param lan_port LAN port index
 * @param mac      MAC address
 * @param ip       IP address (network order)
 */
void Liot_RouterDhcpAddHost(uint8_t lan_port, const uint8_t *mac, uint32_t ip);

/**
 * @brief Set DHCP address pool range
 * @param startHost Start IP (host order)
 * @param endHost   End IP (host order)
 */
void Liot_RouterDhcpSetPool(uint32_t startHost, uint32_t endHost);

/**
 * @brief Set DHCP DNS servers
 * @param dns1 Primary DNS (network order), 0=auto from WAN
 * @param dns2 Secondary DNS (network order)
 */
void Liot_RouterDhcpSetDns(uint32_t dns1, uint32_t dns2);

/**
 * @brief Add a static MAC-IP binding
 * @param mac MAC address
 * @param ip  Bound IP (network order)
 */
void Liot_RouterDhcpBindStatic(const uint8_t *mac, uint32_t ip);

/**
 * @brief Set gateway address
 * @param gwIpHost  Gateway IP (host order)
 * @param maskHost  Subnet mask (host order)
 */
void Liot_RouterDhcpSetGateway(uint32_t gwIpHost, uint32_t maskHost);

/**
 * @brief Get current gateway IP (network order)
 */
uint32_t Liot_RouterDhcpGetGateway(void);

/**
 * @brief Get current subnet mask (network order)
 */
uint32_t Liot_RouterDhcpGetSubnetMask(void);

/**
 * @brief Get a read-only pointer to the DHCP lease table
 * @param out_count [out] Receives the pool capacity (total slots, NOT the
 *                        number of active leases); may be NULL.
 * @return Pointer to the lease table array; caller must filter entries by
 *         their `valid` flag. The element layout matches the internal
 *         lease structure (mac[6], lan_port, valid, ip, leaseStart).
 */
const void *liot_dhcp_get_lease_table(uint8_t *out_count);

/* NAT / netif interface */

/**
 * @brief Update the netif gateway IP and subnet mask
 * @param gwIp Gateway IP (host order)
 * @param mask Subnet mask (host order)
 */
void liot_netif_update_ip(uint32_t gwIp, uint32_t mask);

/**
 * @brief Add a NAT port-forwarding rule
 * @param proto   Protocol (6=TCP, 17=UDP)
 * @param extPort External port
 * @param intIp   Internal IP (network order)
 * @param intPort Internal port
 * @return 0 on success, non-zero on failure
 */
int32_t liot_nat_add_rule(uint8_t proto, uint16_t extPort, uint32_t intIp, uint16_t intPort);

/* LAN interface */

/**
 * @brief Initialize LAN device list
 * @param lan_dev_list Array of device driver pointers
 * @param listNum      Array length
 * @return Number of successfully initialized LAN ports
 */
uint8_t Liot_RouterLanInit(Liot_RouterLanDev_t *lan_dev_list[], uint8_t listNum);

/**
 * @brief Get active LAN port count
 */
uint8_t Liot_RouterLanGetCount(void);

/**
 * @brief Get LAN port information
 * @param lan_port LAN port index
 * @return Pointer to LAN info, NULL if invalid
 */
Liot_RouterLan_t *Liot_RouterLanGetInfo(uint8_t lan_port);

#endif /* __LIOT_ROUTER_H__ */
