/*
 * EtherLCD — CH32V003 interface for the Ethernet+LCD add-on board.
 *
 * ESP32 communicates with the CH32V003 using the same SPI bus as the CH390D
 * Ethernet chip. The CH32V003 enters local-select mode when both CS lines
 * (ETH_CS and LCD_CS) are idle (high) — i.e. when neither the Ethernet chip
 * nor the LCD is being addressed.
 *
 * A dedicated IDF SPI device with spics_io_num=-1 (no hardware CS) is used
 * for CH32V003 register access. Every transaction is preceded by a magic
 * byte (0xEB) so the CH32V003 can distinguish real commands from stale SPI
 * data left over from ETH/LCD transfers.
 *
 * The LCD is addressed via a separate IDF SPI device with CS=GPIO0 (LCD_CS).
 * The CH32V003 sees LCD_CS go low and forwards it to the LCD's SCS pin.
 */

#include <Arduino.h>

#if SMARTEVSE_VERSION >= 30 && SMARTEVSE_VERSION < 40

#include "debug.h"
#include "etherlcd.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// Magic byte that must precede every CH32V003 register command.
#define ELCD_SPI_MAGIC  0xEBu

// SPI device handle for the LCD (CS = GPIO0, mode 3, 10 MHz).
static spi_device_handle_t s_lcd_spi = NULL;

// SPI device handle for CH32V003 register access (no CS, mode 3, 10 MHz).
static spi_device_handle_t s_ch32_spi = NULL;

// Cached LCD_CTL register state to avoid read-modify-write round trips.
static uint8_t s_lcd_ctl = ELCD_CTL_SCS | ELCD_CTL_RST;   // SCS=1 RST=1 A0=0

void etherlcd_init(void) {
    // LCD_CS (GPIO0) is a new output for the Ethernet+LCD board.
    gpio_set_direction((gpio_num_t)ELCD_LCD_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)ELCD_LCD_CS_PIN, 1);

    // Add LCD SPI device on the same bus.
    // CS = GPIO0 (LCD_CS). The CH32V003 sees LCD_CS low and forwards it
    // to the LCD's SCS output via passthrough.
    spi_device_interface_config_t lcd_dev = {};
    lcd_dev.command_bits = 0;
    lcd_dev.address_bits = 0;
    lcd_dev.mode = 3;                       // CPOL=1 CPHA=1 — ST7567 mode 3
    lcd_dev.clock_speed_hz = 10000000;      // 10 MHz
    lcd_dev.spics_io_num = ELCD_LCD_CS_PIN; // CS = GPIO0
    lcd_dev.queue_size = 1;
    lcd_dev.flags = SPI_DEVICE_HALFDUPLEX;

    esp_err_t ret = spi_bus_add_device(SPI3_HOST, &lcd_dev, &s_lcd_spi);
    if (ret != ESP_OK) {
        _LOG_A("EtherLCD: LCD SPI device add failed: %s\n", esp_err_to_name(ret));
        return;
    }

    // Add CH32V003 register-access SPI device on the same bus.
    // No CS pin — both CS lines stay high, which is the CH32V003 local-select condition.
    spi_device_interface_config_t ch32_dev = {};
    ch32_dev.command_bits = 0;
    ch32_dev.address_bits = 0;
    ch32_dev.mode = 3;
    ch32_dev.clock_speed_hz = 10000000;
    ch32_dev.spics_io_num = -1;             // no hardware CS
    ch32_dev.queue_size = 1;
    ch32_dev.flags = SPI_DEVICE_HALFDUPLEX;

    ret = spi_bus_add_device(SPI3_HOST, &ch32_dev, &s_ch32_spi);
    if (ret != ESP_OK) {
        _LOG_A("EtherLCD: CH32 SPI device add failed: %s\n", esp_err_to_name(ret));
        return;
    }

    // Initialise CH32V003 registers to sane defaults.
    etherlcd_reg_write(ELCD_REG_LCD_CTL, ELCD_CTL_SCS | ELCD_CTL_RST);
    etherlcd_set_backlight(128);

    _LOG_A("EtherLCD: CH32V003 interface initialised\n");
}

uint8_t etherlcd_reg_read(uint8_t reg) {
    // Acquire the bus so no CH390/LCD transaction can overlap.
    spi_device_acquire_bus(s_ch32_spi, portMAX_DELAY);

    // Byte 1: magic. Byte 2: read command (0x80 | reg).
    spi_transaction_t t1 = {};
    t1.flags = SPI_TRANS_USE_TXDATA;
    t1.length = 16;
    t1.tx_data[0] = ELCD_SPI_MAGIC;
    t1.tx_data[1] = 0x80u | (reg & 0x7Fu);
    spi_device_polling_transmit(s_ch32_spi, &t1);

    // Clock in one byte response (read-only).
    spi_transaction_t t2 = {};
    t2.flags = SPI_TRANS_USE_RXDATA;
    t2.rxlength = 8;
    spi_device_polling_transmit(s_ch32_spi, &t2);

    spi_device_release_bus(s_ch32_spi);
    return t2.rx_data[0];
}

void etherlcd_reg_write(uint8_t reg, uint8_t value) {
    spi_device_acquire_bus(s_ch32_spi, portMAX_DELAY);

    // Magic + register address + value in one 3-byte TX-only transaction.
    spi_transaction_t t = {};
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 24;
    t.tx_data[0] = ELCD_SPI_MAGIC;
    t.tx_data[1] = reg & 0x7Fu;
    t.tx_data[2] = value;
    spi_device_polling_transmit(s_ch32_spi, &t);

    spi_device_release_bus(s_ch32_spi);
}

uint8_t etherlcd_read_buttons(void) {
    return etherlcd_reg_read(ELCD_REG_BUTTONS);
}

void etherlcd_set_backlight(uint8_t pwm) {
    etherlcd_reg_write(ELCD_REG_LED_PWM, pwm);
}

void etherlcd_lcd_a0(bool high) {
    uint8_t new_ctl = s_lcd_ctl;
    if (high)
        new_ctl |= ELCD_CTL_A0;
    else
        new_ctl &= ~ELCD_CTL_A0;
    if (new_ctl != s_lcd_ctl) {
        s_lcd_ctl = new_ctl;
        etherlcd_reg_write(ELCD_REG_LCD_CTL, s_lcd_ctl);
    }
}

void etherlcd_lcd_rst(bool high) {
    if (high)
        s_lcd_ctl |= ELCD_CTL_RST;
    else
        s_lcd_ctl &= ~ELCD_CTL_RST;
    etherlcd_reg_write(ELCD_REG_LCD_CTL, s_lcd_ctl);
}

void etherlcd_lcd_transfer(uint8_t data) {
    spi_transaction_t t = {};
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = data;
    spi_device_polling_transmit(s_lcd_spi, &t);
}

void etherlcd_lcd_transfer_buf(const uint8_t *data, size_t len) {
    if (len == 0) return;
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(s_lcd_spi, &t);
}

void etherlcd_lcd_command(uint8_t cmd) {
    etherlcd_lcd_a0(false);
    etherlcd_lcd_transfer(cmd);
}

void etherlcd_lcd_data(uint8_t data) {
    etherlcd_lcd_a0(true);
    etherlcd_lcd_transfer(data);
}

#endif // SMARTEVSE_VERSION >= 30 && < 40
