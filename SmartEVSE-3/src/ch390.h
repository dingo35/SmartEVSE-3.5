/*
 * CH390D SPI Ethernet MAC/PHY driver for ESP-IDF 4.4 (Arduino ESP32 2.x)
 *
 * The CH390D is register-compatible with the DM9051 but has different
 * Vendor/Product IDs. This driver implements the esp_eth_mac_t / esp_eth_phy_t
 * interface using the identical register map, bypassing the DM9051 driver's
 * hard-coded chip ID check.
 *
 * SPI protocol: 1 command bit (R/W) + 7 address bits, same as DM9051.
 */

#ifndef __CH390_H
#define __CH390_H

#include <Arduino.h>

#if SMARTEVSE_VERSION >= 30 && SMARTEVSE_VERSION < 40

// ---------- Pin mapping for CH390D add-on board (v3) ----------
// Reuses LCD SPI pins. When CH390D is detected, LCD is disabled.
#define CH390_SCK       26      // was SPI_SCK  (PIN_LCD_CLK)
#define CH390_MOSI      33      // was SPI_MOSI (PIN_LCD_SDO_B3)
#define CH390_MISO      14      // was PIN_LCD_LED
#define CH390_CS        25      // was PIN_LCD_A0_B2
#define CH390_INT        5      // INT pin directly connected to ESP32 GPIO5

// ---------- Chip identification ----------
#define CH390_VID_L     0x00
#define CH390_VID_H     0x1C
#define CH390_PID_L     0x51
#define CH390_PID_H     0x91

// ---------- SPI command bits ----------
#define CH390_SPI_RD    0
#define CH390_SPI_WR    1

// ---------- Register map (identical to DM9051) ----------
#define CH390_NCR       0x00    // Network Control
#define CH390_NSR       0x01    // Network Status
#define CH390_TCR       0x02    // TX Control
#define CH390_TSRA      0x03    // TX Status A
#define CH390_TSRB      0x04    // TX Status B
#define CH390_RCR       0x05    // RX Control
#define CH390_RSR       0x06    // RX Status
#define CH390_ROCR      0x07    // RX Overflow Count
#define CH390_BPTR      0x08    // Back Pressure Threshold
#define CH390_FCTR      0x09    // Flow Control Threshold
#define CH390_FCR       0x0A    // Flow Control
#define CH390_EPCR      0x0B    // EEPROM/PHY Control
#define CH390_EPAR      0x0C    // EEPROM/PHY Address
#define CH390_EPDRL     0x0D    // EEPROM/PHY Data Low
#define CH390_EPDRH     0x0E    // EEPROM/PHY Data High
#define CH390_WCR       0x0F    // Wakeup Control
#define CH390_PAR       0x10    // Physical (MAC) Address (6 bytes: 0x10-0x15)
#define CH390_MAR       0x16    // Multicast Address Hash (8 bytes: 0x16-0x1D)
#define CH390_GPCR      0x1E    // GPIO Control
#define CH390_GPR       0x1F    // GPIO
#define CH390_TRPAL     0x22    // TX Read Pointer Low
#define CH390_TRPAH     0x23    // TX Read Pointer High
#define CH390_RWPAL     0x24    // RX Write Pointer Low
#define CH390_RWPAH     0x25    // RX Write Pointer High
#define CH390_VIDL      0x28    // Vendor ID Low
#define CH390_VIDH      0x29    // Vendor ID High
#define CH390_PIDL      0x2A    // Product ID Low
#define CH390_PIDH      0x2B    // Product ID High
#define CH390_CHIPR     0x2C    // Chip Revision
#define CH390_TCR2      0x2D    // TX Control 2
#define CH390_ATCR      0x30    // Auto-TX Control
#define CH390_TCSCR     0x31    // TX Checksum Control
#define CH390_RCSCSR    0x32    // RX Checksum Control/Status
#define CH390_SBCR      0x38    // SPI Bus Control
#define CH390_INTCR     0x39    // INT Pin Control
#define CH390_ALNCR     0x4A    // SPI Alignment Error Count
#define CH390_SCCR      0x50    // System Clock Control
#define CH390_RSCCR     0x51    // Recover System Clock Control
#define CH390_RLENCR    0x52    // RX Data Length Control
#define CH390_BCASTCR   0x53    // RX Broadcast Control
#define CH390_INTCKCR   0x54    // INT Pin Clock Output Control
#define CH390_MPTRCR    0x55    // Memory Pointer Control
#define CH390_MLEDCR    0x57    // LED Control
#define CH390_MEMSCR    0x59    // Memory Control
#define CH390_TMEMR     0x5A    // TX Memory Size
#define CH390_MBSR      0x5D    // Memory BIST Status

// Memory data access registers
#define CH390_MRCMDX    0x70    // Memory Read (no addr increment)
#define CH390_MRCMDX1   0x71    // Memory Read (no pre-fetch)
#define CH390_MRCMD     0x72    // Memory Read (with addr increment)
#define CH390_MRRL      0x74    // Memory Read Address Low
#define CH390_MRRH      0x75    // Memory Read Address High
#define CH390_MWCMDX    0x76    // Memory Write (no addr increment)
#define CH390_MWCMD     0x78    // Memory Write (with addr increment)
#define CH390_MWRL      0x7A    // Memory Write Address Low
#define CH390_MWRH      0x7B    // Memory Write Address High
#define CH390_TXPLL     0x7C    // TX Packet Length Low
#define CH390_TXPLH     0x7D    // TX Packet Length High

// Interrupt registers
#define CH390_ISR       0x7E    // Interrupt Status
#define CH390_IMR       0x7F    // Interrupt Mask

// ---------- Register bit definitions ----------
#define NCR_RST         (1 << 0)    // Software reset
#define NCR_LBK_MAC     (1 << 1)    // MAC loopback

#define NSR_WAKEST      (1 << 5)
#define NSR_TX2END      (1 << 3)
#define NSR_TX1END      (1 << 2)
#define NSR_LINKST      (1 << 6)    // Link status (1=up)

#define TCR_TXREQ       (1 << 0)    // TX request
#define TCR2_RLCP       (1 << 6)    // Retry late collision

#define RCR_DIS_CRC     (1 << 4)    // Discard CRC error
#define RCR_ALL         (1 << 3)    // Receive all multicast
#define RCR_RXEN        (1 << 0)    // RX enable
#define RCR_PRMSC       (1 << 1)    // Promiscuous mode
#define RCR_DIS_LONG    (1 << 5)    // Discard long packets

#define ATCR_AUTO_TX    (1 << 7)    // Auto-transmit

#define TCSCR_IPCSE     (1 << 0)    // IP checksum offload
#define TCSCR_TCPCSE    (1 << 1)    // TCP checksum offload
#define TCSCR_UDPCSE    (1 << 2)    // UDP checksum offload

#define MPTRCR_RST_TX   (1 << 1)    // Reset TX memory pointer
#define MPTRCR_RST_RX   (1 << 0)    // Reset RX memory pointer

#define FCR_FLOW_ENABLE 0x39        // Enable flow control

#define FCTR_HWOT(x)    (((x) & 0xF) << 4)  // Flow control high water overflow threshold
#define FCTR_LWOT(x)    ((x) & 0xF)         // Flow control low water overflow threshold

#define RLENCR_RXLEN_EN      0x80   // Enable RX data pack length filter
#define RLENCR_RXLEN_DEFAULT 0x18   // Default MAX length of RX data (div by 64)

#define RSR_ERR_MASK    (0xBF)      // RX status error mask (RF|MF|LCS|RWTO|PLE|AE|CE|FOE)

#define CH390_PKT_NONE  0x00        // No packet received
#define CH390_PKT_RDY   0x01        // Packet ready to receive
#define CH390_PKT_ERR   0xFE        // Un-stable states mask

#define BMCR_LOOPBACK   (1 << 14)   // PHY loopback bit in BMCR

#define ISR_PR          (1 << 0)    // Packet received
#define ISR_PT          (1 << 1)    // Packet transmitted
#define ISR_ROS         (1 << 2)    // RX overflow
#define ISR_ROO         (1 << 3)    // RX overflow counter overflow
#define ISR_LNKCHGS     (1 << 5)    // Link status change
#define ISR_CLR_STATUS  (ISR_LNKCHGS | ISR_ROO | ISR_ROS | ISR_PT | ISR_PR)

#define IMR_PAR         (1 << 7)    // Pointer auto-return
#define IMR_LNKCHGI     (1 << 5)    // Link change interrupt enable
#define IMR_ROOI        (1 << 3)    // RX overflow counter overflow interrupt enable
#define IMR_ROI         (1 << 2)    // RX overflow interrupt enable
#define IMR_PRI         (1 << 0)    // Packet received interrupt enable

#define EPCR_EPOS       (1 << 3)    // Select PHY (1) or EEPROM (0)
#define EPCR_ERPRR      (1 << 2)    // PHY/EEPROM read command (self-clearing)
#define EPCR_ERPRW      (1 << 1)    // PHY/EEPROM write command (self-clearing)
#define EPCR_ERRE       (1 << 0)    // PHY/EEPROM busy flag (set during operation, auto-clears)

#define INTCR_POL_LOW   (1 << 0)    // INT polarity: 1 = active low
#define INTCR_POL_HIGH  0x00        // INT polarity: active high, push-pull
#define INTCR_POD_OD    (1 << 1)    // INT type: 1 = open-drain, 0 = push-pull

// Internal PHY register access via EPAR
#define CH390_PHY       0x40        // PHY address for EPAR

// PHY registers (accessed via EPCR/EPAR/EPDRL/EPDRH)
#define PHY_BMCR        0x00
#define PHY_BMSR        0x01
#define PHY_PHYID1      0x02
#define PHY_PHYID2      0x03
#define PHY_ANAR        0x04
#define PHY_ANLPAR      0x05

#define BMCR_RST        (1 << 15)
#define BMCR_ANEG_EN    (1 << 12)
#define BMCR_ANEG_RST   (1 << 9)
#define BMCR_SPEED100   (1 << 13)
#define BMCR_DUPLEX     (1 << 8)

#define BMSR_LINK       (1 << 2)
#define BMSR_ANEG_DONE  (1 << 5)

#define ANLPAR_100FD    (1 << 8)
#define ANLPAR_100HD    (1 << 7)
#define ANLPAR_10FD     (1 << 6)
#define ANLPAR_10HD     (1 << 5)

// RX packet header size (status + length)
#define CH390_RX_HDR_SIZE   4

// ---------- Public API ----------

// Probe the SPI bus for a CH390D chip. Returns true if found.
// On failure, releases SPI resources so Arduino SPI can reclaim the bus.
bool ch390_detect(void);

// Initialize Ethernet: create MAC/PHY, install driver, create netif, start DHCP.
// Call only after ch390_detect() returned true.
esp_err_t ch390_eth_init(void);

// Get the Ethernet IP address string (empty if no IP).
const char* ch390_get_ip(void);

// Runtime flags
extern bool EthPresent;     // true if CH390D chip was detected at boot
extern bool EthConnected;   // true if Ethernet link is up
extern bool EthHasIP;       // true if Ethernet interface has a DHCP IP

#endif // SMARTEVSE_VERSION >= 30 && < 40
#endif // __CH390_H
