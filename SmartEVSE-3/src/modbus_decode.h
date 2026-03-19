/*
 * modbus_decode.h - Pure C Modbus frame parser
 *
 * Extracted from modbus.cpp ModbusDecode() for native testability.
 * No platform dependencies — compiles with gcc on any host.
 */

#ifndef MODBUS_DECODE_H
#define MODBUS_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Modbus frame types (matches existing firmware constants) */
#define MODBUS_INVALID   0
#define MODBUS_OK        1
#define MODBUS_REQUEST   2
#define MODBUS_RESPONSE  3
#define MODBUS_EXCEPTION 4

/* Default broadcast address */
#define MODBUS_BROADCAST_ADR 0x09

/*
 * Parsed Modbus frame.
 * Mirrors the existing firmware `struct ModBus` layout so the glue layer
 * can memcpy between them during the transition period.
 */
typedef struct {
    uint8_t  Address;
    uint8_t  Function;
    uint16_t Register;
    uint16_t RegisterCount;
    uint16_t Value;
    uint8_t  *Data;
    uint8_t  DataLength;
    uint8_t  Type;          /* MODBUS_INVALID / MODBUS_REQUEST / etc. */
    uint8_t  RequestAddress;
    uint8_t  RequestFunction;
    uint16_t RequestRegister;
    uint8_t  Exception;
} modbus_frame_t;

/*
 * Initialize a modbus_frame_t to safe defaults.
 * Call before first use or to clear between frames.
 */
void modbus_frame_init(modbus_frame_t *frame);

/*
 * Decode a raw Modbus RTU frame (CRC already stripped).
 *
 * This function expects the CRC to have been validated and removed
 * by the caller (matching ESP32 behavior where the Modbus library
 * strips CRC). For CH32 compatibility, the caller should validate
 * CRC and subtract 2 from len before calling.
 *
 * @param frame  Output: parsed frame fields
 * @param buf    Input: raw bytes (address + function + payload, no CRC)
 * @param len    Length of buf in bytes
 */
void modbus_decode(modbus_frame_t *frame, const uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_DECODE_H */
