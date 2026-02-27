/*
 * CH390D SPI Ethernet MAC/PHY driver for ESP-IDF 4.4 (Arduino ESP32 2.x)
 *
 * Implements esp_eth_mac_t and esp_eth_phy_t interfaces for the CH390D chip.
 * Uses DMA for rx/tx data transfer, and supports interrupt-driven packet reception.
 */

#include <Arduino.h>

#if SMARTEVSE_VERSION >= 30 && SMARTEVSE_VERSION < 40

#include "ch390.h"
#include "network_common.h"
#include <string.h>
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

// ---------- Runtime state ----------
bool EthPresent  = false;
bool EthConnected = false;
bool EthHasIP    = false;

static spi_device_handle_t s_spi = NULL;
static spi_host_device_t   s_spi_host = SPI3_HOST; // VSPI
static esp_netif_t *s_eth_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;

// ---------- Low-level SPI register access ----------

static esp_err_t ch390_reg_read(uint8_t reg, uint8_t *val) {
    spi_transaction_t t = {};
    t.cmd = CH390_SPI_RD;
    t.addr = reg;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.rxlength = 8;
    t.length = 0;
    esp_err_t ret = spi_device_polling_transmit(s_spi, &t);
    if (ret == ESP_OK) *val = t.rx_data[0];
    return ret;
}

static esp_err_t ch390_reg_write(uint8_t reg, uint8_t val) {
    spi_transaction_t t = {};
    t.cmd = CH390_SPI_WR;
    t.addr = reg;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = val;
    return spi_device_polling_transmit(s_spi, &t);
}

static esp_err_t ch390_mem_read(uint8_t *buf, uint32_t len) {
    spi_transaction_t t = {};
    t.cmd = CH390_SPI_RD;
    t.addr = CH390_MRCMD;
    t.rx_buffer = buf;
    t.rxlength = len * 8;
    t.length = 0;
    return spi_device_polling_transmit(s_spi, &t);
}

static esp_err_t ch390_mem_write(const uint8_t *buf, uint32_t len) {
    spi_transaction_t t = {};
    t.cmd = CH390_SPI_WR;
    t.addr = CH390_MWCMD;
    t.tx_buffer = buf;
    t.length = len * 8;
    return spi_device_polling_transmit(s_spi, &t);
}

// ---------- Internal PHY register access ----------

static esp_err_t ch390_phy_reg_read(uint8_t phy_reg, uint16_t *val) {
    ch390_reg_write(CH390_EPAR, CH390_PHY | phy_reg);
    ch390_reg_write(CH390_EPCR, EPCR_EPOS | EPCR_ERPRR);
    // Poll EPCR busy flag until hardware signals completion
    uint8_t epcr;
    uint32_t to = 0;
    do {
        esp_rom_delay_us(100);
        ch390_reg_read(CH390_EPCR, &epcr);
        to += 100;
    } while ((epcr & EPCR_ERRE) && to < 1000);
    ch390_reg_write(CH390_EPCR, 0);
    uint8_t lo, hi;
    ch390_reg_read(CH390_EPDRH, &hi);
    ch390_reg_read(CH390_EPDRL, &lo);
    *val = (hi << 8) | lo;
    return ESP_OK;
}

static esp_err_t ch390_phy_reg_write(uint8_t phy_reg, uint16_t val) {
    ch390_reg_write(CH390_EPAR, CH390_PHY | phy_reg);
    ch390_reg_write(CH390_EPDRL, val & 0xFF);
    ch390_reg_write(CH390_EPDRH, (val >> 8) & 0xFF);
    ch390_reg_write(CH390_EPCR, EPCR_EPOS | EPCR_ERPRW);
    // Poll EPCR busy flag until hardware signals completion
    uint8_t epcr;
    uint32_t to = 0;
    do {
        esp_rom_delay_us(100);
        ch390_reg_read(CH390_EPCR, &epcr);
        to += 100;
    } while ((epcr & EPCR_ERRE) && to < 1000);
    ch390_reg_write(CH390_EPCR, 0);
    return ESP_OK;
}

// =====================================================================
// MAC driver (esp_eth_mac_t implementation)
// =====================================================================

typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    TaskHandle_t rx_task;
    SemaphoreHandle_t spi_lock;
    uint8_t addr[6];
    bool flow_ctrl_enabled;
    uint8_t *rx_buffer;
} emac_ch390_t;

static emac_ch390_t *s_emac = NULL;
static volatile bool s_force_link_down = false;


#define CH390_LOCK(emac)   xSemaphoreTake((emac)->spi_lock, portMAX_DELAY)
#define CH390_UNLOCK(emac) xSemaphoreGive((emac)->spi_lock)

// Internal stop/start helpers — caller must hold spi_lock.
static void ch390_stop_locked(emac_ch390_t *emac) {
    ch390_reg_write(CH390_IMR, 0x00);
    uint8_t rcr;
    ch390_reg_read(CH390_RCR, &rcr);
    ch390_reg_write(CH390_RCR, rcr & ~RCR_RXEN);
}

static void ch390_start_locked(emac_ch390_t *emac) {
    ch390_reg_write(CH390_MPTRCR, MPTRCR_RST_RX);
    ch390_reg_write(CH390_ISR, ISR_CLR_STATUS);
    ch390_reg_write(CH390_IMR, IMR_PAR | IMR_ROOI | IMR_ROI | IMR_PRI);
    uint8_t rcr;
    ch390_reg_read(CH390_RCR, &rcr);
    ch390_reg_write(CH390_RCR, rcr | RCR_RXEN);
}

// Drop a received frame by advancing the RX SRAM read pointer past it.
// Caller must hold spi_lock.
static void ch390_drop_frame(uint16_t length) {
    uint8_t mrrh, mrrl;
    ch390_reg_read(CH390_MRRH, &mrrh);
    ch390_reg_read(CH390_MRRL, &mrrl);
    uint16_t addr = (mrrh << 8) | mrrl;
    addr += length;  // includes 4B header already consumed
    if (addr >= 0x4000) addr -= 0x3400;
    ch390_reg_write(CH390_MRRH, addr >> 8);
    ch390_reg_write(CH390_MRRL, addr & 0xFF);
}

// Full software reset + register reinit. Caller must hold spi_lock.
// Does NOT enable RX/interrupts — caller must call ch390_start_locked() afterwards.
// Returns true if the chip responded correctly after reset, false if SPI/chip is dead.
static bool ch390_reset_and_reinit_locked(emac_ch390_t *emac) {
    // Software reset
    ch390_reg_write(CH390_NCR, NCR_RST);
    bool reset_ok = false;
    for (int i = 0; i < 100; i++) {
        vTaskDelay(pdMS_TO_TICKS(2));
        uint8_t ncr;
        ch390_reg_read(CH390_NCR, &ncr);
        if (!(ncr & NCR_RST)) { reset_ok = true; break; }
    }
    if (!reset_ok) {
        _LOG_A("CH390: software reset did not complete (NCR_RST stuck)\n");
    }

    // Power on internal PHY (reset clears GPR back to default)
    ch390_reg_write(CH390_GPR, 0x00);   // Bit 0 = PHYPD, 0 = PHY powered on
    vTaskDelay(pdMS_TO_TICKS(10));       // mac and phy register won't be accessible within at least 1ms

    ch390_reg_write(CH390_NCR, 0x00);
    ch390_reg_write(CH390_WCR, 0x00);
    ch390_reg_write(CH390_TCR, 0x00);
    ch390_reg_write(CH390_RCR, RCR_DIS_CRC | RCR_ALL);
    ch390_reg_write(CH390_TCR2, TCR2_RLCP);
    ch390_reg_write(CH390_TCSCR, TCSCR_IPCSE | TCSCR_TCPCSE | TCSCR_UDPCSE);   // HW TX checksum
    ch390_reg_write(CH390_RCSCSR, 0x00);
    ch390_reg_write(CH390_INTCR, 0x00);            // INT pin: push-pull, active high
    ch390_reg_write(CH390_INTCKCR, 0x00);          // Clear INT pin clock output
    ch390_reg_write(CH390_RLENCR, RLENCR_RXLEN_EN | RLENCR_RXLEN_DEFAULT);  // RX length limit 1536
    ch390_reg_write(CH390_NSR, NSR_WAKEST | NSR_TX2END | NSR_TX1END);

    // Hardware flow-control
    ch390_reg_write(CH390_BPTR, 0x3F);                              // Back pressure threshold
    ch390_reg_write(CH390_FCTR, FCTR_HWOT(3) | FCTR_LWOT(8));       // High/low water marks
    ch390_reg_write(CH390_FCR, FCR_FLOW_ENABLE);

    // Clear multicast hash table and enable broadcast reception
    ch390_reg_write(CH390_BCASTCR, 0x00);
    for (int i = 0; i < 7; i++)
        ch390_reg_write(CH390_MAR + i, 0x00);
    ch390_reg_write(CH390_MAR + 7, 0x80);  // Enable broadcast packet reception

    // Restore MAC address
    for (int i = 0; i < 6; i++)
        ch390_reg_write(CH390_PAR + i, emac->addr[i]);

    // Verify chip is alive by reading back VID
    uint8_t vid_l = 0;
    ch390_reg_read(CH390_VIDL, &vid_l);
    if (vid_l != CH390_VID_L) {
        _LOG_A("CH390: chip not responding after reset (VID_L=0x%02X, expected 0x%02X)\n",
               vid_l, CH390_VID_L);
        return false;
    }

    // Explicitly restart PHY auto-negotiation after reset.
    // The PHY powers up with default BMCR which may not have ANEN set on
    // all revisions; kicking autoneg ensures link comes back reliably.
    uint16_t bmcr;
    ch390_phy_reg_read(PHY_BMCR, &bmcr);
    bmcr |= BMCR_ANEG_EN | BMCR_ANEG_RST;
    ch390_phy_reg_write(PHY_BMCR, bmcr);

    // Force phy_get_link() to report one link-DOWN cycle so the IDF driver
    // sees a DOWN→UP transition and fires proper events (DHCP, ARP flush).
    s_force_link_down = true;

    return true;
}

// ISR handler — notifies rx task on CH390 interrupt
static void IRAM_ATTR ch390_isr_handler(void *arg) {
    emac_ch390_t *emac = (emac_ch390_t *)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    vTaskNotifyGiveFromISR(emac->rx_task, &high_task_wakeup);
    if (high_task_wakeup) portYIELD_FROM_ISR();
}

// RX task — matches reference emac_ch390_task structure
static void ch390_rx_task(void *arg) {
    emac_ch390_t *emac = (emac_ch390_t *)arg;
    uint8_t status = 0;
    uint8_t *buffer;
    while (1) {
        // Wait for ISR notification or 1000ms timeout
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0 &&
                gpio_get_level((gpio_num_t)CH390_INT) == 0) {
            continue;   // no notification and no interrupt asserted
        }

        // read and clear interrupt status
        CH390_LOCK(emac);
        ch390_reg_read(CH390_ISR, &status);
        ch390_reg_write(CH390_ISR, status);
        CH390_UNLOCK(emac);

        // RX FIFO overflow — reset RX path
        if (status & (ISR_ROS | ISR_ROO)) {
            _LOG_W("CH390: RX overflow (ISR=0x%02X), resetting RX\n", status);
            CH390_LOCK(emac);
            ch390_stop_locked(emac);
            esp_rom_delay_us(1000);
            ch390_start_locked(emac);
            CH390_UNLOCK(emac);
            continue;
        }

        /* packet received */
        if (status & ISR_PR) {
            do {
                uint32_t frame_len = 0;
                if (emac->parent.receive(&emac->parent, emac->rx_buffer, &frame_len) == ESP_OK) {
                    if (frame_len == 0) {
                        break;
                    }
                    /* allocate memory and check whether allocation failed */
                    buffer = (uint8_t *)malloc(frame_len);
                    if (buffer == NULL) {
                        _LOG_A("CH390: no memory for receive buffer\n");
                        continue;
                    }
                    /* pass the buffer to stack (e.g. TCP/IP layer) */
                    memcpy(buffer, emac->rx_buffer, frame_len);
                    emac->eth->stack_input(emac->eth, buffer, frame_len);
                } else {
                    _LOG_W("CH390: frame read failed\n");
                    break;
                }
            } while (1);

            // Yield briefly after draining all pending packets.
            vTaskDelay(1);
        }
    }
    vTaskDelete(NULL);
}

static esp_err_t ch390_mac_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    emac->eth = eth;
    return ESP_OK;
}

static esp_err_t ch390_mac_init(esp_eth_mac_t *mac) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);

    CH390_LOCK(emac);
    esp_read_mac(emac->addr, ESP_MAC_ETH);
    if (!ch390_reset_and_reinit_locked(emac)) {
        _LOG_A("CH390: chip init failed — check SPI wiring\n");
    }
    CH390_UNLOCK(emac);

    // Configure INT pin as input with pull-down (active high from CH390, matching INTCR config)
    gpio_set_direction((gpio_num_t)CH390_INT, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CH390_INT, GPIO_PULLDOWN_ONLY);
    gpio_set_intr_type((gpio_num_t)CH390_INT, GPIO_INTR_POSEDGE);
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        _LOG_A("CH390: gpio_install_isr_service failed: %s\n", esp_err_to_name(isr_err));
    }
    gpio_isr_handler_add((gpio_num_t)CH390_INT, ch390_isr_handler, emac);
    gpio_intr_enable((gpio_num_t)CH390_INT);
    _LOG_D("CH390: INT pin %d configured (active high, posedge)\n", CH390_INT);

    return ESP_OK;
}

static esp_err_t ch390_mac_deinit(esp_eth_mac_t *mac) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    gpio_isr_handler_remove((gpio_num_t)CH390_INT);
    gpio_intr_disable((gpio_num_t)CH390_INT);
    if (emac->rx_task) {
        vTaskDelete(emac->rx_task);
        emac->rx_task = NULL;
    }
    return ESP_OK;
}

static esp_err_t ch390_mac_start(esp_eth_mac_t *mac) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    CH390_LOCK(emac);
    ch390_start_locked(emac);
    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_stop(esp_eth_mac_t *mac) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    CH390_LOCK(emac);
    ch390_stop_locked(emac);
    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    if (length > ETH_MAX_PACKET_SIZE) return ESP_ERR_INVALID_SIZE;

    CH390_LOCK(emac);

    // Write packet data to TX memory
    ch390_mem_write(buf, length);

    // Check if last transmit is complete (poll TCR_TXREQ)
    uint8_t tcr;
    int64_t wait_time = esp_timer_get_time();
    do {
        ch390_reg_read(CH390_TCR, &tcr);
    } while ((tcr & TCR_TXREQ) && ((esp_timer_get_time() - wait_time) < 1000));

    if (tcr & TCR_TXREQ) {
        _LOG_W("CH390: last transmit still in progress, cannot send\n");
        CH390_UNLOCK(emac);
        return ESP_ERR_INVALID_STATE;
    }

    // Set TX packet length
    ch390_reg_write(CH390_TXPLL, length & 0xFF);
    ch390_reg_write(CH390_TXPLH, (length >> 8) & 0xFF);

    // Issue TX polling command
    ch390_reg_read(CH390_TCR, &tcr);
    ch390_reg_write(CH390_TCR, tcr | TCR_TXREQ);

    CH390_UNLOCK(emac);
    return ESP_OK;
}

// Receive one packet from the CH390 RX SRAM.
// Matches the Espressif reference emac_ch390_receive pattern.
static esp_err_t ch390_mac_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);

    CH390_LOCK(emac);

    // Double dummy read to get the most updated data
    uint8_t rxbyte;
    ch390_reg_read(CH390_MRCMDX, &rxbyte);
    ch390_reg_read(CH390_MRCMDX, &rxbyte);

    // If rxbyte indicates error state, do stop/start recovery
    if (rxbyte & CH390_PKT_ERR) {
        ch390_stop_locked(emac);
        esp_rom_delay_us(1000);
        ch390_start_locked(emac);
        CH390_UNLOCK(emac);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (rxbyte & CH390_PKT_RDY) {
        // Read 4-byte RX header: flag, status, length_lo, length_hi
        __attribute__((aligned(4))) uint8_t rx_header[CH390_RX_HDR_SIZE];
        ch390_mem_read(rx_header, CH390_RX_HDR_SIZE);

        uint8_t status = rx_header[1];
        *length = (rx_header[3] << 8) + rx_header[2];

        if (status & RSR_ERR_MASK) {
            ch390_drop_frame(*length);
            *length = 0;
            CH390_UNLOCK(emac);
            return ESP_ERR_INVALID_RESPONSE;
        } else if (*length > ETH_MAX_PACKET_SIZE) {
            // Reset rx memory pointer
            ch390_reg_write(CH390_MPTRCR, MPTRCR_RST_RX);
            CH390_UNLOCK(emac);
            return ESP_ERR_INVALID_RESPONSE;
        } else {
            ch390_mem_read(buf, *length);
            *length -= ETH_CRC_LEN;
        }
    } else {
        *length = 0;
    }

    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_set_addr(esp_eth_mac_t *mac, uint8_t *addr) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    memcpy(emac->addr, addr, 6);
    CH390_LOCK(emac);
    for (int i = 0; i < 6; i++) {
        ch390_reg_write(CH390_PAR + i, addr[i]);
    }
    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_get_addr(esp_eth_mac_t *mac, uint8_t *addr) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    memcpy(addr, emac->addr, 6);
    return ESP_OK;
}

static esp_err_t ch390_mac_set_speed(esp_eth_mac_t *mac, eth_speed_t speed) {
    // Speed is managed by PHY auto-negotiation, nothing to configure in MAC
    _LOG_D("CH390 Speed set to %s\n", speed == ETH_SPEED_100M ? "100M" : "10M");
    return ESP_OK;
}

static esp_err_t ch390_mac_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex) {
    // Duplex is managed by PHY auto-negotiation, nothing to configure in MAC
    _LOG_D("CH390 Duplex set to %s\n", duplex == ETH_DUPLEX_FULL ? "Full" : "Half");
    return ESP_OK;
}

static esp_err_t ch390_mac_set_link(esp_eth_mac_t *mac, eth_link_t link) {
    esp_err_t ret = ESP_OK;
    switch (link) {
    case ETH_LINK_UP:
        ret = mac->start(mac);
        break;
    case ETH_LINK_DOWN:
        ret = mac->stop(mac);
        break;
    default:
        break;
    }
    _LOG_I("CH390 Link %s\n", link == ETH_LINK_UP ? "Up" : "Down");
    return ret;
}

static esp_err_t ch390_mac_set_promiscuous(esp_eth_mac_t *mac, bool enable) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    CH390_LOCK(emac);
    uint8_t rcr;
    ch390_reg_read(CH390_RCR, &rcr);
    if (enable) rcr |= RCR_PRMSC;
    else        rcr &= ~RCR_PRMSC;
    ch390_reg_write(CH390_RCR, rcr);
    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    emac->flow_ctrl_enabled = enable;
    return ESP_OK;
}

static void ch390_enable_flow_ctrl_hw(emac_ch390_t *emac, bool enable) {
    CH390_LOCK(emac);
    if (enable) {
        // Send jam pattern when RX free space < 3K bytes
        ch390_reg_write(CH390_BPTR, 0x3F);
        // Flow control thresholds: high water = 3K, low water = 8K
        ch390_reg_write(CH390_FCTR, FCTR_HWOT(3) | FCTR_LWOT(8));
        ch390_reg_write(CH390_FCR, FCR_FLOW_ENABLE);
    } else {
        ch390_reg_write(CH390_FCR, 0x00);
    }
    CH390_UNLOCK(emac);
}

static esp_err_t ch390_mac_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    if (emac->flow_ctrl_enabled && ability) {
        ch390_enable_flow_ctrl_hw(emac, true);
    } else {
        ch390_enable_flow_ctrl_hw(emac, false);
    }
    return ESP_OK;
}

static esp_err_t ch390_mac_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    CH390_LOCK(emac);
    ch390_phy_reg_write(phy_reg, (uint16_t)reg_value);
    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    CH390_LOCK(emac);
    uint16_t val;
    ch390_phy_reg_read(phy_reg, &val);
    *reg_value = val;
    CH390_UNLOCK(emac);
    return ESP_OK;
}

static esp_err_t ch390_mac_del(esp_eth_mac_t *mac) {
    emac_ch390_t *emac = __containerof(mac, emac_ch390_t, parent);
    if (emac->rx_task) vTaskDelete(emac->rx_task);
    if (emac->spi_lock) vSemaphoreDelete(emac->spi_lock);
    heap_caps_free(emac->rx_buffer);
    free(emac);
    return ESP_OK;
}

static esp_eth_mac_t *ch390_mac_new(void) {
    emac_ch390_t *emac = (emac_ch390_t *)calloc(1, sizeof(emac_ch390_t));
    if (!emac) return NULL;

    emac->parent.set_mediator        = ch390_mac_set_mediator;
    emac->parent.init                = ch390_mac_init;
    emac->parent.deinit              = ch390_mac_deinit;
    emac->parent.start               = ch390_mac_start;
    emac->parent.stop                = ch390_mac_stop;
    emac->parent.del                 = ch390_mac_del;
    emac->parent.write_phy_reg       = ch390_mac_write_phy_reg;
    emac->parent.read_phy_reg        = ch390_mac_read_phy_reg;
    emac->parent.set_addr            = ch390_mac_set_addr;
    emac->parent.get_addr            = ch390_mac_get_addr;
    emac->parent.set_speed           = ch390_mac_set_speed;
    emac->parent.set_duplex          = ch390_mac_set_duplex;
    emac->parent.set_link            = ch390_mac_set_link;
    emac->parent.set_promiscuous     = ch390_mac_set_promiscuous;
    emac->parent.set_peer_pause_ability = ch390_mac_set_peer_pause_ability;
    emac->parent.enable_flow_ctrl    = ch390_mac_enable_flow_ctrl;
    emac->parent.transmit            = ch390_mac_transmit;
    emac->parent.receive             = ch390_mac_receive;

    emac->spi_lock = xSemaphoreCreateMutex();
    if (!emac->spi_lock) { free(emac); return NULL; }

    emac->rx_buffer = (uint8_t *)heap_caps_malloc(1536 + CH390_RX_HDR_SIZE, MALLOC_CAP_DMA);
    if (!emac->rx_buffer) { vSemaphoreDelete(emac->spi_lock); free(emac); return NULL; }

    BaseType_t ret = xTaskCreatePinnedToCore(ch390_rx_task, "ch390_rx", 4096,
                                              emac, 8, &emac->rx_task, 0);
    if (ret != pdPASS) {
        heap_caps_free(emac->rx_buffer);
        vSemaphoreDelete(emac->spi_lock);
        free(emac);
        return NULL;
    }

    s_emac = emac;

    return &emac->parent;
}

// =====================================================================
// PHY driver (esp_eth_phy_t implementation)
// =====================================================================

typedef struct {
    esp_eth_phy_t parent;
    esp_eth_mediator_t *eth;
    int addr;
    eth_link_t link;
    eth_speed_t negotiated_speed;
    eth_duplex_t negotiated_duplex;
} phy_ch390_t;

static esp_err_t ch390_phy_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    p->eth = eth;
    return ESP_OK;
}

static esp_err_t ch390_phy_init(esp_eth_phy_t *phy) {
    // PHY is integrated, no external init needed beyond what MAC init does
    return ESP_OK;
}

static esp_err_t ch390_phy_deinit(esp_eth_phy_t *phy) {
    return ESP_OK;
}

static esp_err_t ch390_phy_reset(esp_eth_phy_t *phy) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    uint32_t bmcr;
    p->eth->phy_reg_read(p->eth, p->addr, PHY_BMCR, &bmcr);
    bmcr |= BMCR_RST;
    p->eth->phy_reg_write(p->eth, p->addr, PHY_BMCR, bmcr);
    // Wait for reset to complete
    for (int i = 0; i < 100; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        p->eth->phy_reg_read(p->eth, p->addr, PHY_BMCR, &bmcr);
        if (!(bmcr & BMCR_RST)) return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t ch390_phy_reset_hw(esp_eth_phy_t *phy) {
    // No hardware reset pin available
    return ESP_OK;
}

static esp_err_t ch390_phy_negotiate(esp_eth_phy_t *phy) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    uint32_t bmcr;

    // Read current BMCR
    p->eth->phy_reg_read(p->eth, p->addr, PHY_BMCR, &bmcr);

    // Enable auto-negotiation and restart
    bmcr |= BMCR_ANEG_EN | BMCR_ANEG_RST;
    p->eth->phy_reg_write(p->eth, p->addr, PHY_BMCR, bmcr);
    _LOG_I("CH390: auto-negotiation started (non-blocking)\n");

    // Read BMCR to determine resolved speed/duplex (reference reads BMCR, not ANLPAR)
    p->eth->phy_reg_read(p->eth, p->addr, PHY_BMCR, &bmcr);
    p->negotiated_speed  = (bmcr & BMCR_SPEED100) ? ETH_SPEED_100M : ETH_SPEED_10M;
    p->negotiated_duplex = (bmcr & BMCR_DUPLEX)   ? ETH_DUPLEX_FULL : ETH_DUPLEX_HALF;

    return ESP_OK;
}

static esp_err_t ch390_phy_get_link(esp_eth_phy_t *phy) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);

    // After a chip reinit, force one link-DOWN report so the IDF driver
    // sees a transition and fires CONNECTED/GOT_IP events on recovery.
    if (s_force_link_down) {
        s_force_link_down = false;
        if (p->link != ETH_LINK_DOWN) {
            p->link = ETH_LINK_DOWN;
            p->eth->on_state_changed(p->eth, ETH_STATE_LINK,
                         reinterpret_cast<void *>(static_cast<intptr_t>(ETH_LINK_DOWN)));
        }
        return ESP_OK;
    }

    uint32_t bmsr;
    // Read BMSR twice: first read clears latched-low link bit, second gives real-time value
    p->eth->phy_reg_read(p->eth, p->addr, PHY_BMSR, &bmsr);
    p->eth->phy_reg_read(p->eth, p->addr, PHY_BMSR, &bmsr);

    eth_link_t new_link = (bmsr & BMSR_LINK) ? ETH_LINK_UP : ETH_LINK_DOWN;
    if (p->link != new_link) {
        // When link comes up, report speed/duplex before link state
        if (new_link == ETH_LINK_UP) {
            // Read BMCR for resolved speed/duplex (reference reads BMCR, not ANLPAR)
            uint32_t bmcr_val;
            p->eth->phy_reg_read(p->eth, p->addr, PHY_BMCR, &bmcr_val);
            eth_speed_t speed  = (bmcr_val & BMCR_SPEED100) ? ETH_SPEED_100M : ETH_SPEED_10M;
            eth_duplex_t duplex = (bmcr_val & BMCR_DUPLEX)   ? ETH_DUPLEX_FULL : ETH_DUPLEX_HALF;
            p->eth->on_state_changed(p->eth, ETH_STATE_SPEED,
                         reinterpret_cast<void *>(static_cast<intptr_t>(speed)));
            p->eth->on_state_changed(p->eth, ETH_STATE_DUPLEX,
                         reinterpret_cast<void *>(static_cast<intptr_t>(duplex)));
        }
        p->link = new_link;
        p->eth->on_state_changed(p->eth, ETH_STATE_LINK,
                     reinterpret_cast<void *>(static_cast<intptr_t>(new_link)));
    }
    return ESP_OK;
}

static esp_err_t ch390_phy_pwrctl(esp_eth_phy_t *phy, bool enable) {
    return ESP_OK;
}

static esp_err_t ch390_phy_set_addr(esp_eth_phy_t *phy, uint32_t addr) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    p->addr = addr;
    return ESP_OK;
}

static esp_err_t ch390_phy_get_addr(esp_eth_phy_t *phy, uint32_t *addr) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    *addr = p->addr;
    return ESP_OK;
}

static esp_err_t ch390_phy_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability) {
    return ESP_OK;
}

static esp_err_t ch390_phy_loopback(esp_eth_phy_t *phy, bool enable) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    uint32_t bmcr;
    p->eth->phy_reg_read(p->eth, p->addr, PHY_BMCR, &bmcr);
    if (enable) bmcr |= BMCR_LOOPBACK;
    else        bmcr &= ~BMCR_LOOPBACK;
    p->eth->phy_reg_write(p->eth, p->addr, PHY_BMCR, bmcr);
    return ESP_OK;
}

static esp_err_t ch390_phy_del(esp_eth_phy_t *phy) {
    phy_ch390_t *p = __containerof(phy, phy_ch390_t, parent);
    free(p);
    return ESP_OK;
}

static esp_eth_phy_t *ch390_phy_new(void) {
    phy_ch390_t *p = (phy_ch390_t *)calloc(1, sizeof(phy_ch390_t));
    if (!p) return NULL;

    p->addr = 1; // DM9051/CH390 internal PHY address
    p->link = ETH_LINK_DOWN;

    p->parent.set_mediator             = ch390_phy_set_mediator;
    p->parent.init                     = ch390_phy_init;
    p->parent.deinit                   = ch390_phy_deinit;
    p->parent.reset                    = ch390_phy_reset;
    p->parent.reset_hw                 = ch390_phy_reset_hw;
    p->parent.negotiate                = ch390_phy_negotiate;
    p->parent.get_link                 = ch390_phy_get_link;
    p->parent.pwrctl                   = ch390_phy_pwrctl;
    p->parent.set_addr                 = ch390_phy_set_addr;
    p->parent.get_addr                 = ch390_phy_get_addr;
    p->parent.advertise_pause_ability  = ch390_phy_advertise_pause_ability;
    p->parent.loopback                 = ch390_phy_loopback;
    p->parent.del                      = ch390_phy_del;

    return &p->parent;
}

// =====================================================================
// Public API
// =====================================================================

bool ch390_detect(void) {
    _LOG_I("CH390: Probing SPI (SCK=%d MOSI=%d MISO=%d CS=%d)...\n",
             CH390_SCK, CH390_MOSI, CH390_MISO, CH390_CS);

    // Initialize SPI bus
    spi_bus_config_t bus = {};
    bus.mosi_io_num = CH390_MOSI;
    bus.miso_io_num = CH390_MISO;
    bus.sclk_io_num = CH390_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 1600;

    // Setup DMA channel, let driver pick the best one if available
    esp_err_t ret = spi_bus_initialize(s_spi_host, &bus, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        _LOG_A("CH390: SPI bus init failed: %s (%d)\n", esp_err_to_name(ret), ret);
        return false;
    }
    _LOG_D("CH390: SPI bus initialized OK\n");

    // Add CH390D SPI device: 1 cmd bit + 7 addr bits, mode 0, 1 MHz for probe
    spi_device_interface_config_t dev = {};
    dev.command_bits = 1;
    dev.address_bits = 7;
    dev.mode = 3;  // CPOL=1, CPHA=1 (mode 3)
    dev.clock_speed_hz = 1000000; // 1 MHz for safe probing
    dev.spics_io_num = CH390_CS;
    dev.queue_size = 1;
    dev.flags = SPI_DEVICE_HALFDUPLEX;  // CH390D uses separate read/write phases

    ret = spi_bus_add_device(s_spi_host, &dev, &s_spi);
    if (ret != ESP_OK) {
        _LOG_A("CH390: SPI device add failed: %s (%d)\n", esp_err_to_name(ret), ret);
        spi_bus_free(s_spi_host);
        return false;
    }
    _LOG_D("CH390: SPI device added OK\n");

    // Read chip ID
    uint8_t vid_l = 0, vid_h = 0, pid_l = 0, pid_h = 0;
    ch390_reg_read(CH390_VIDL, &vid_l);
    ch390_reg_read(CH390_VIDH, &vid_h);
    ch390_reg_read(CH390_PIDL, &pid_l);
    ch390_reg_read(CH390_PIDH, &pid_h);
    _LOG_I("CH390: Probed VID=%02X%02X PID=%02X%02X\n", vid_h, vid_l, pid_h, pid_l);

    if (vid_h == CH390_VID_H && vid_l == CH390_VID_L &&
        pid_h == CH390_PID_H && pid_l == CH390_PID_L) {
        _LOG_I("CH390D detected!\n");

        // Increase SPI clock to 10 MHz for normal operation
        spi_bus_remove_device(s_spi);
        dev.clock_speed_hz = 10000000;
        spi_bus_add_device(s_spi_host, &dev, &s_spi);

        EthPresent = true;
        return true;
    }

    // Not found — release SPI resources
    _LOG_I("CH390D not found, releasing SPI bus for LCD\n");
    spi_bus_remove_device(s_spi);
    s_spi = NULL;
    spi_bus_free(s_spi_host);
    return false;
}

// Ethernet event handlers
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    uint8_t mac[6];
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
        _LOG_I("Ethernet Link Up - MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        EthConnected = true;
        // DHCP lifecycle is managed by esp_eth_stop/start via the
        // esp_eth_netif_glue layer (esp_netif_action_start/connected).
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        _LOG_I("Ethernet Link Down\n");
        EthConnected = false;
        EthHasIP = false;
        break;
    case ETHERNET_EVENT_START:
        _LOG_I("Ethernet Started\n");
        // DHCP is started in ETHERNET_EVENT_CONNECTED, not here.
        // Starting it on START (before link is up) causes a duplicate
        // DHCP cycle and double GOT_IP events.
        break;
    case ETHERNET_EVENT_STOP:
        _LOG_W("Ethernet Stopped\n");
        EthConnected = false;
        EthHasIP = false;
        break;
    default:
        break;
    }
}

static char eth_ip_str[16] = "";

const char* ch390_get_ip(void) {
    return eth_ip_str;
}

static void eth_got_ip_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip = &event->ip_info;
    snprintf(eth_ip_str, sizeof(eth_ip_str), IPSTR, IP2STR(&ip->ip));
    _LOG_I("Ethernet Got IP: %s\n", eth_ip_str);
    EthHasIP = true;

    _LOG_I("Ethernet Netmask: " IPSTR " GW: " IPSTR "\n",
           IP2STR(&ip->netmask), IP2STR(&ip->gw));

    // Get DNS server from the Ethernet netif
    esp_netif_dns_info_t dns;
    if (event->esp_netif && esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
        char dns_str[16];
        snprintf(dns_str, sizeof(dns_str), IPSTR, IP2STR(&dns.ip.u_addr.ip4));
        onGotIP(dns_str);
    } else {
        onGotIP(NULL);
    }
}

static void eth_lost_ip_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    (void)arg;
    (void)event_base;
    (void)event_id;
    (void)event_data;
    EthHasIP = false;
    eth_ip_str[0] = '\0';
    _LOG_W("Ethernet lost IP\n");
}

esp_err_t ch390_eth_init(void) {
    if (!s_spi) return ESP_ERR_INVALID_STATE;

    // Initialize TCP/IP stack and event loop
    // esp_event_loop_create_default() is safe to call multiple times — returns
    // ESP_ERR_INVALID_STATE if already created (by Arduino WiFi).
    // Must be created before esp_netif_new() which depends on it.
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        _LOG_A("CH390: event loop create failed: %s\n", esp_err_to_name(err));
        return err;
    }
    ESP_ERROR_CHECK(esp_netif_init());

    // Create MAC and PHY
    esp_eth_mac_t *mac = ch390_mac_new();
    if (!mac) {
        _LOG_A("CH390: Failed to create MAC\n");
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = ch390_phy_new();
    if (!phy) {
        _LOG_A("CH390: Failed to create PHY\n");
        mac->del(mac);
        return ESP_FAIL;
    }

    // Install Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    esp_err_t ret = esp_eth_driver_install(&eth_config, &eth_handle);
    if (ret != ESP_OK) {
        _LOG_A("CH390: Ethernet driver install failed: %s\n", esp_err_to_name(ret));
        return ret;
    }

    // Set MAC address from ESP32's Ethernet MAC slot
    uint8_t eth_mac[6];
    esp_read_mac(eth_mac, ESP_MAC_ETH);
    esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, eth_mac);
    s_eth_handle = eth_handle;

    // Create netif for Ethernet (DHCP enabled by default)
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    if (!eth_netif) {
        _LOG_A("CH390: Failed to create netif\n");
        return ESP_FAIL;
    }
    s_eth_netif = eth_netif;

    // Attach driver to TCP/IP stack
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    esp_netif_attach(eth_netif, glue);

    // Register event handlers
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_got_ip_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &eth_lost_ip_handler, NULL);

    // Start Ethernet
    ret = esp_eth_start(eth_handle);
    if (ret != ESP_OK) {
        _LOG_A("CH390: Ethernet start failed: %s\n", esp_err_to_name(ret));
        return ret;
    }

    _LOG_I("CH390D Ethernet initialized, DHCP started\n");
    return ESP_OK;
}

#endif // SMARTEVSE_VERSION >= 30 && < 40
