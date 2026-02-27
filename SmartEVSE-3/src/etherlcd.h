/*
 * EtherLCD — Interface to the CH32V003 on the Ethernet+LCD add-on board.
 *
 * The CH32V003 sits between the ESP32 and the LCD/Ethernet peripherals.
 * It forwards chip-select signals (ETH_CS→ETH_SCS, LCD_CS→LCD_SCS).
 * When both CS lines are idle (high) — i.e. neither the Ethernet chip
 * nor the LCD is being addressed — the CH32V003 enters local-select
 * mode for SPI register access.
 *
 * A magic byte (0xEB) precedes every register command to protect against
 * stale SPI data from ETH/LCD transfers being misinterpreted.
 *
 *   REG 0x00  BUTTONS   (RO)  bit0=B1, bit1=B2, bit2=B3, 0=pressed
 *   REG 0x01  LED_PWM   (RW)  0..255 backlight brightness
 *   REG 0x02  LCD_CTL   (RW)  bit0=SCS bit1=RST bit2=A0
 *
 * SPI protocol (mode 3, CPOL=1 CPHA=1):
 *   Read:  TX: 0xEB, 0x80|reg, dummy → RX byte3 = value
 *   Write: TX: 0xEB, reg, value
 */

#ifndef __ETHERLCD_H
#define __ETHERLCD_H

#include <Arduino.h>

#if SMARTEVSE_VERSION >= 30 && SMARTEVSE_VERSION < 40

// CH32V003 register addresses (match EtherLCD firmware)
#define ELCD_REG_BUTTONS    0x00
#define ELCD_REG_LED_PWM    0x01
#define ELCD_REG_LCD_CTL    0x02

// LCD_CTL register bits
#define ELCD_CTL_SCS        (1u << 0)
#define ELCD_CTL_RST        (1u << 1)
#define ELCD_CTL_A0         (1u << 2)

// Pin reuse: on the Ethernet+LCD board these ESP32 GPIOs become chip selects
// that the CH32V003 monitors.
#define ELCD_ETH_CS_PIN     14      // was PIN_LCD_LED,    now ETH_CS
#define ELCD_LCD_CS_PIN      0      // was PIN_IO0_B1,     now LCD_CS

// Initialize SPI devices for LCD and CH32V003 on the shared bus.
// Call after ch390_detect() succeeds and the SPI bus is already initialized.
void etherlcd_init(void);

// Read a CH32V003 register (asserts both CS lines).
uint8_t etherlcd_reg_read(uint8_t reg);

// Write a CH32V003 register (asserts both CS lines).
void etherlcd_reg_write(uint8_t reg, uint8_t value);

// Write button state from CH32V003 into the passed-in variable.
uint8_t etherlcd_read_buttons(void);

// Set LCD backlight brightness (0..255) via CH32V003 PWM.
void etherlcd_set_backlight(uint8_t pwm);

// Set LCD control lines (A0, RST) via CH32V003.
void etherlcd_lcd_a0(bool high);
void etherlcd_lcd_rst(bool high);

// Transfer a byte to the LCD via dedicated LCD SPI device (uses LCD_CS on GPIO0).
void etherlcd_lcd_transfer(uint8_t data);

// Transfer a buffer of bytes to the LCD in one SPI transaction.
void etherlcd_lcd_transfer_buf(const uint8_t *data, size_t len);

// Send a command byte to the LCD (A0=0, CS toggle, SPI transfer).
void etherlcd_lcd_command(uint8_t cmd);

// Send a data byte to the LCD (A0=1, CS toggle, SPI transfer).
void etherlcd_lcd_data(uint8_t data);

#endif // SMARTEVSE_VERSION >= 30 && < 40
#endif // __ETHERLCD_H
