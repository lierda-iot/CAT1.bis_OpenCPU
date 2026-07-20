/**
 * @file liot_ch390.h
 * @brief CH390 multi-instance driver — Handle API (SPI mode, full feature coverage)
 */

#ifndef __LIOT_CH390_H__
#define __LIOT_CH390_H__

#include <stdint.h>

#ifndef LIOT_CH390_MAX_INST
#define LIOT_CH390_MAX_INST  2
#endif

#ifndef LIOT_CH390_SPI_SPEED
#define LIOT_CH390_SPI_SPEED 20000000
#endif

/* ──── Register definitions (SPI mode, CH390H) ──── */

#define CH390_NCR       0x00
#define CH390_NSR       0x01
#define CH390_TCR       0x02
#define CH390_TSRA      0x03
#define CH390_TSRB      0x04
#define CH390_RCR       0x05
#define CH390_RSR       0x06
#define CH390_ROCR      0x07
#define CH390_BPTR      0x08
#define CH390_FCTR      0x09
#define CH390_FCR       0x0A
#define CH390_EPCR      0x0B
#define CH390_EPAR      0x0C
#define CH390_EPDRL     0x0D
#define CH390_EPDRH     0x0E
#define CH390_WCR       0x0F
#define CH390_PAR       0x10
#define CH390_MAR       0x16
#define CH390_GPCR      0x1E
#define CH390_GPR       0x1F
#define CH390_TRPAL     0x22
#define CH390_TRPAH     0x23
#define CH390_RWPAL     0x24
#define CH390_RWPAH     0x25
#define CH390_VIDL      0x28
#define CH390_VIDH      0x29
#define CH390_PIDL      0x2A
#define CH390_PIDH      0x2B
#define CH390_CHIPR     0x2C
#define CH390_TCR2      0x2D
#define CH390_ATCR      0x30
#define CH390_TCSCR     0x31
#define CH390_RCSCSR    0x32
#define CH390_MPAR      0x33
#define CH390_SBCR      0x38
#define CH390_INTCR     0x39
#define CH390_ALNCR     0x4A
#define CH390_SCCR      0x50
#define CH390_RSCCR     0x51
#define CH390_RLENCR    0x52
#define CH390_BCASTCR   0x53
#define CH390_INTCKCR   0x54
#define CH390_MPTRCR    0x55
#define CH390_MLEDCR    0x57
#define CH390_MRCMDX    0x70
#define CH390_MRCMDX1   0x71
#define CH390_MRCMD     0x72
#define CH390_MRRL      0x74
#define CH390_MRRH      0x75
#define CH390_MWCMDX    0x76
#define CH390_MWCMD     0x78
#define CH390_MWRL      0x7A
#define CH390_MWRH      0x7B
#define CH390_TXPLL     0x7C
#define CH390_TXPLH     0x7D
#define CH390_ISR       0x7E
#define CH390_IMR       0x7F

/* ──── NCR bits ──── */
#define NCR_WAKEEN      (1<<6)
#define NCR_FDX         (1<<3)
#define NCR_LBK_MAC     (1<<1)
#define NCR_RST         (1<<0)

/* ──── NSR bits ──── */
#define NSR_SPEED       (1<<7)
#define NSR_LINKST      (1<<6)
#define NSR_WAKEST      (1<<5)
#define NSR_TX2END      (1<<3)
#define NSR_TX1END      (1<<2)
#define NSR_RXOV        (1<<1)
#define NSR_RXRDY       (1<<0)

/* ──── TCR bits ──── */
#define TCR_TJDIS       (1<<6)
#define TCR_PAD_DIS2    (1<<4)
#define TCR_CRC_DIS2    (1<<3)
#define TCR_PAD_DIS1    (1<<2)
#define TCR_CRC_DIS1    (1<<1)
#define TCR_TXREQ       (1<<0)

/* ──── TSR bits ──── */
#define TSR_TJTO        (1<<7)
#define TSR_LC          (1<<6)
#define TSR_NC          (1<<5)
#define TSR_LCOL        (1<<4)
#define TSR_COL         (1<<3)
#define TSR_EC          (1<<2)

/* ──── RCR bits ──── */
#define RCR_DEFAULT     0x00
#define RCR_WTDIS       (1<<6)
#define RCR_DIS_CRC     (1<<4)
#define RCR_ALL         (1<<3)
#define RCR_RUNT        (1<<2)
#define RCR_PRMSC       (1<<1)
#define RCR_RXEN        (1<<0)

/* ──── RSR bits ──── */
#define RSR_RF          (1<<7)
#define RSR_MF          (1<<6)
#define RSR_LCS         (1<<5)
#define RSR_RWTO        (1<<4)
#define RSR_PLE         (1<<3)
#define RSR_AE          (1<<2)
#define RSR_CE          (1<<1)
#define RSR_FOE         (1<<0)

/* ──── FCTR macros ──── */
#define FCTR_HWOT(ot)   (((ot) & 0xf) << 4)
#define FCTR_LWOT(ot)   ((ot) & 0xf)

/* ──── EPCR bits ──── */
#define EPCR_REEP       (1<<5)
#define EPCR_WEP        (1<<4)
#define EPCR_EPOS       (1<<3)
#define EPCR_ERPRR      (1<<2)
#define EPCR_ERPRW      (1<<1)
#define EPCR_ERRE       (1<<0)

/* ──── WCR bits ──── */
#define WCR_LINKEN      (1<<5)
#define WCR_SAMPLEEN    (1<<4)
#define WCR_MAGICEN     (1<<3)
#define WCR_LINKST      (1<<2)
#define WCR_SAMPLEST    (1<<1)
#define WCR_MAGICST     (1<<0)

/* ──── TCSCR bits ──── */
#define TCSCR_ALL         0x1F
#define TCSCR_IPv6TCPCSE  (1<<4)
#define TCSCR_IPv6UDPCSE  (1<<3)
#define TCSCR_UDPCSE      (1<<2)
#define TCSCR_TCPCSE      (1<<1)
#define TCSCR_IPCSE       (1<<0)

/* ──── RCSCSR bits ──── */
#define RCSCSR_UDPS     (1<<7)
#define RCSCSR_TCPS     (1<<6)
#define RCSCSR_IPS      (1<<5)
#define RCSCSR_UDPP     (1<<4)
#define RCSCSR_TCPP     (1<<3)
#define RCSCSR_IPP      (1<<2)
#define RCSCSR_RCSEN    (1<<1)
#define RCSCSR_DCSE     (1<<0)

/* ──── INTCR bits ──── */
#define INCR_TYPE_OD    0x02
#define INCR_TYPE_PP    0x00
#define INCR_POL_L      0x01
#define INCR_POL_H      0x00

/* ──── ISR bits ──── */
#define ISR_LNKCHG      (1<<5)
#define ISR_ROO         (1<<3)
#define ISR_ROS         (1<<2)
#define ISR_PT          (1<<1)
#define ISR_PR          (1<<0)

/* ──── IMR bits ──── */
#define IMR_NONE        0x00
#define IMR_ALL         0xFF
#define IMR_PAR         (1<<7)
#define IMR_LNKCHGI     (1<<5)
#define IMR_UDRUNI      (1<<4)
#define IMR_ROOI        (1<<3)
#define IMR_ROI         (1<<2)
#define IMR_PTI         (1<<1)
#define IMR_PRI         (1<<0)

/* ──── SPI opcodes ──── */
#define OPC_REG_W       0x80
#define OPC_REG_R       0x00
#define OPC_MEM_DMY_R   0x70
#define OPC_MEM_WRITE   0xF8
#define OPC_MEM_READ    0x72

/* ──── GPIO defines ──── */
#define CH390_GPIO1     0x02
#define CH390_GPIO2     0x04
#define CH390_GPIO3     0x08

/* ──── PHY registers ──── */
#define CH390_PHY          0x40
#define CH390_PHY_BMCR     0x00
#define CH390_PHY_BMSR     0x01
#define CH390_PHY_PHYID1   0x02
#define CH390_PHY_PHYID2   0x03
#define CH390_PHY_ANAR     0x04
#define CH390_PHY_ANLPAR   0x05
#define CH390_PHY_ANER     0x06
#define CH390_PHY_PAGE_SEL 0x1F

/* ──── Packet status ──── */
#define CH390_PKT_NONE  0x00
#define CH390_PKT_RDY   0x01
#define CH390_PKT_ERR   0xFE
#define CH390_PKT_MAX   1536
#define CH390_PKT_MIN   64

/* ──── PHY mode enum ──── */
typedef enum {
    LIOT_CH390_10MHD,
    LIOT_CH390_100MHD,
    LIOT_CH390_10MFD,
    LIOT_CH390_100MFD,
    LIOT_CH390_AUTO,
} Liot_Ch390PhyMode_e;

/* ──── Handle API ──── */

/** @brief Opaque handle to a CH390 instance */
typedef void *Liot_Ch390Handle_t;

/** @brief CH390 instance configuration */
typedef struct {
    uint8_t   spiPort;          /**< SPI port index (0 or 1) */
    uint8_t   intWakeupPad;     /**< Wakeup pad for INT pin */
    uint8_t   mac[6];           /**< Initial MAC address (all-zero = read from chip) */
    uint8_t   rstGpio;          /**< RST pin GPIO number (liot_gpio_e) */
    uint8_t   ssnGpio;          /**< SPI CS pin GPIO number (liot_gpio_e) */
    void    (*intWakeupCb)(void *arg); /**< Wakeup interrupt callback */
    void     *intWakeupCbArg;   /**< Wakeup callback argument */
} Liot_Ch390Cfg_t;

/**
 * @brief Create and initialize a CH390 instance
 * @param cfg Configuration parameters
 * @return Handle on success, NULL on failure
 */
Liot_Ch390Handle_t Liot_Ch390Create(const Liot_Ch390Cfg_t *cfg);

/**
 * @brief Destroy a CH390 instance and release resources
 * @param handle Instance handle
 */
void     Liot_Ch390Destroy(Liot_Ch390Handle_t handle);

/**
 * @brief Hardware reset CH390 by toggling RST pin
 * @param handle Instance handle
 */
void     Liot_Ch390HardwareReset(Liot_Ch390Handle_t handle);

/**
 * @brief Software reset CH390 via NCR register
 * @param handle Instance handle
 */
void     Liot_Ch390SoftwareReset(Liot_Ch390Handle_t handle);

/**
 * @brief Apply default configuration (auto-negotiation, enable RX, all interrupts)
 * @param handle Instance handle
 */
void     Liot_Ch390DefaultConfig(Liot_Ch390Handle_t handle);

/**
 * @brief Set PHY mode (speed/duplex)
 * @param handle Instance handle
 * @param mode PHY mode selection
 */
void     Liot_Ch390SetPhyMode(Liot_Ch390Handle_t handle, Liot_Ch390PhyMode_e mode);

/**
 * @brief Set MAC address
 * @param handle Instance handle
 * @param mac 6-byte MAC address
 */
void     Liot_Ch390SetMac(Liot_Ch390Handle_t handle, const uint8_t *mac);

/**
 * @brief Set multicast hash table
 * @param handle Instance handle
 * @param hash 8-byte hash table
 */
void     Liot_Ch390SetMulticast(Liot_Ch390Handle_t handle, const uint8_t *hash);

/**
 * @brief Add a MAC address to the multicast hash filter
 * @param handle Instance handle
 * @param mac 6-byte destination MAC address
 */
void     Liot_Ch390SetHashBit(Liot_Ch390Handle_t handle, const uint8_t *mac);

/**
 * @brief Configure interrupt mask
 * @param handle Instance handle
 * @param mask IMR register value
 */
void     Liot_Ch390InterruptConfig(Liot_Ch390Handle_t handle, uint8_t mask);

/**
 * @brief Configure INT pin output type and polarity
 * @param handle Instance handle
 * @param type INCR_TYPE_PP or INCR_TYPE_OD
 * @param pol INCR_POL_L or INCR_POL_H
 */
void     Liot_Ch390IntPinConfig(Liot_Ch390Handle_t handle, uint8_t type, uint8_t pol);

/**
 * @brief Enable or disable packet reception
 * @param handle Instance handle
 * @param op 1=enable, 0=disable
 */
void     Liot_Ch390RxEnable(Liot_Ch390Handle_t handle, int op);

/**
 * @brief Configure receive filter
 * @param handle Instance handle
 * @param config RCR filter bits (RCR_PRMSC, RCR_ALL, etc.)
 */
void     Liot_Ch390RxFilterConfig(Liot_Ch390Handle_t handle, uint8_t config);

/**
 * @brief Configure Wake-On-LAN events
 * @param handle Instance handle
 * @param events WCR event bits (0 to disable)
 */
void     Liot_Ch390WakeupConfig(Liot_Ch390Handle_t handle, uint8_t events);

/**
 * @brief Notify chip to watch for wakeup frame before sleep
 * @param handle Instance handle
 */
void     Liot_Ch390WakeNotify(Liot_Ch390Handle_t handle);

/**
 * @brief Enable or disable MAC loopback mode
 * @param handle Instance handle
 * @param op 1=enable, 0=disable
 */
void     Liot_Ch390LoopBackEnable(Liot_Ch390Handle_t handle, int op);

/**
 * @brief Enter or exit sleep mode
 * @param handle Instance handle
 * @param op 1=sleep, 0=wake
 */
void     Liot_Ch390SleepControl(Liot_Ch390Handle_t handle, int op);

/**
 * @brief Configure CH390 internal GPIO direction
 * @param handle Instance handle
 * @param GPIOx CH390_GPIO1/2/3
 * @param dir 1=output, 0=input
 */
void     Liot_Ch390GpioConfig(Liot_Ch390Handle_t handle, uint8_t GPIOx, uint8_t dir);

/**
 * @brief Write CH390 internal GPIO pin
 * @param handle Instance handle
 * @param GPIOx CH390_GPIO1/2/3
 * @param level 1=high, 0=low
 */
void     Liot_Ch390GpioWriteBit(Liot_Ch390Handle_t handle, uint8_t GPIOx, uint8_t level);

/**
 * @brief Read CH390 internal GPIO pin
 * @param handle Instance handle
 * @param GPIOx CH390_GPIO1/2/3
 * @return Pin level (0 or 1)
 */
uint8_t  Liot_Ch390GpioReadBit(Liot_Ch390Handle_t handle, uint8_t GPIOx);

/**
 * @brief Read a CH390 register
 * @param handle Instance handle
 * @param reg Register address
 * @return Register value
 */
uint8_t  Liot_Ch390ReadReg(Liot_Ch390Handle_t handle, uint8_t reg);

/**
 * @brief Write a CH390 register
 * @param handle Instance handle
 * @param reg Register address
 * @param val Value to write
 */
void     Liot_Ch390WriteReg(Liot_Ch390Handle_t handle, uint8_t reg, uint8_t val);

/**
 * @brief Read data from CH390 RX SRAM
 * @param handle Instance handle
 * @param data Output buffer
 * @param length Bytes to read
 */
void     Liot_Ch390ReadMem(Liot_Ch390Handle_t handle, uint8_t *data, int length);

/**
 * @brief Write data to CH390 TX SRAM
 * @param handle Instance handle
 * @param data Data buffer
 * @param length Bytes to write
 */
void     Liot_Ch390WriteMem(Liot_Ch390Handle_t handle, uint8_t *data, int length);

/**
 * @brief Read a PHY register
 * @param handle Instance handle
 * @param reg PHY register address
 * @return 16-bit register value
 */
uint16_t Liot_Ch390ReadPhy(Liot_Ch390Handle_t handle, uint8_t reg);

/**
 * @brief Write a PHY register
 * @param handle Instance handle
 * @param reg PHY register address
 * @param val 16-bit value to write
 */
void     Liot_Ch390WritePhy(Liot_Ch390Handle_t handle, uint8_t reg, uint16_t val);

/**
 * @brief Write an EEPROM register
 * @param handle Instance handle
 * @param reg EEPROM register address
 * @param val 16-bit value to write
 */
void     Liot_Ch390WriteEeprom(Liot_Ch390Handle_t handle, uint8_t reg, uint16_t val);

/**
 * @brief Receive a packet from the RX FIFO
 * @param handle Instance handle
 * @param buf Output buffer (must be >= CH390_PKT_MAX bytes)
 * @return Packet length in bytes, 0 if no packet or error
 */
uint32_t Liot_Ch390Receive(Liot_Ch390Handle_t handle, uint8_t *buf);

/**
 * @brief Send a packet
 * @param handle Instance handle
 * @param buf Packet data
 * @param len Packet length
 */
void     Liot_Ch390Send(Liot_Ch390Handle_t handle, uint8_t *buf, uint16_t len);

/**
 * @brief Issue a transmit request (TCR TXREQ bit)
 * @param handle Instance handle
 */
void     Liot_Ch390SendRequest(Liot_Ch390Handle_t handle);

/**
 * @brief Drop a packet from RX SRAM without reading it
 * @param handle Instance handle
 * @param len Packet length to skip
 */
void     Liot_Ch390DropPacket(Liot_Ch390Handle_t handle, uint16_t len);

/**
 * @brief Get current MAC address from chip
 * @param handle Instance handle
 * @param mac 6-byte output buffer
 */
void     Liot_Ch390GetMac(Liot_Ch390Handle_t handle, uint8_t *mac);

/**
 * @brief Get current multicast hash table from chip
 * @param handle Instance handle
 * @param hash 8-byte output buffer
 */
void     Liot_Ch390GetMulticast(Liot_Ch390Handle_t handle, uint8_t *hash);

/**
 * @brief Get vendor ID
 * @param handle Instance handle
 * @return Vendor ID
 */
uint16_t Liot_Ch390GetVendorId(Liot_Ch390Handle_t handle);

/**
 * @brief Get product ID
 * @param handle Instance handle
 * @return Product ID
 */
uint16_t Liot_Ch390GetProductId(Liot_Ch390Handle_t handle);

/**
 * @brief Get chip revision
 * @param handle Instance handle
 * @return Revision number
 */
uint8_t  Liot_Ch390GetRevision(Liot_Ch390Handle_t handle);

/**
 * @brief Get and clear interrupt status
 * @param handle Instance handle
 * @return ISR register value
 */
uint8_t  Liot_Ch390GetIntStatus(Liot_Ch390Handle_t handle);

/**
 * @brief Get PHY link status
 * @param handle Instance handle
 * @return 1=linked, 0=not linked
 */
int      Liot_Ch390GetLinkStatus(Liot_Ch390Handle_t handle);

/**
 * @brief Get current duplex mode
 * @param handle Instance handle
 * @return 1=full-duplex, 0=half-duplex
 */
int      Liot_Ch390GetDuplexMode(Liot_Ch390Handle_t handle);

/**
 * @brief Get PHY speed
 * @param handle Instance handle
 * @return 1=10Mbps, 0=100Mbps
 */
int      Liot_Ch390GetPhySpeed(Liot_Ch390Handle_t handle);

#endif /* __LIOT_CH390_H__ */
