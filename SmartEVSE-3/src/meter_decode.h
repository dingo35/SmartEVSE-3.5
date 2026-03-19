/*
 * meter_decode.h - Pure C meter byte decoding for Modbus energy meters
 *
 * Extracted from meter.cpp combineBytes() and decodeMeasurement()
 * for native testability. No platform dependencies.
 */

#ifndef METER_DECODE_H
#define METER_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Endianness modes (byte order + word order) */
#define ENDIANNESS_LBF_LWF 0  /* low byte first, low word first (little endian) */
#define ENDIANNESS_LBF_HWF 1  /* low byte first, high word first */
#define ENDIANNESS_HBF_LWF 2  /* high byte first, low word first */
#define ENDIANNESS_HBF_HWF 3  /* high byte first, high word first (big endian) */

/* Modbus data types */
typedef enum {
    METER_DATATYPE_INT32   = 0,
    METER_DATATYPE_FLOAT32 = 1,
    METER_DATATYPE_INT16   = 2,
    METER_DATATYPE_MAX
} meter_datatype_t;

/*
 * Decoded measurement result.
 */
typedef struct {
    int32_t value;    /* Decoded value after endianness conversion and divisor */
    uint8_t valid;    /* 1 if decode succeeded, 0 on error */
} meter_reading_t;

/*
 * Combine raw Modbus bytes into a 32-bit value according to endianness
 * and data type. Writes to *out_value (int32_t for INT32/INT16, or
 * reinterpreted float bits for FLOAT32).
 *
 * @param out       Output: combined bytes (caller casts to int32_t or float)
 * @param buf       Input: raw Modbus response data bytes
 * @param pos       Byte offset into buf where this register starts
 * @param endianness ENDIANNESS_* constant
 * @param datatype   METER_DATATYPE_* constant
 *
 * The caller must ensure buf has at least pos + 4 bytes available
 * (or pos + 2 for INT16).
 */
void meter_combine_bytes(void *out, const uint8_t *buf, uint8_t pos,
                         uint8_t endianness, meter_datatype_t datatype);

/*
 * Decode a single measurement value from a Modbus response buffer.
 *
 * @param buf        Raw data bytes from Modbus response
 * @param index      Register index (0-based); byte offset = index * register_size
 * @param endianness ENDIANNESS_* constant
 * @param datatype   METER_DATATYPE_* constant
 * @param divisor    Power-of-10 divisor: positive = divide, negative = multiply
 *                   e.g., divisor=1 divides by 10, divisor=-3 multiplies by 1000
 * @return           Decoded measurement result
 */
meter_reading_t meter_decode_value(const uint8_t *buf, uint8_t index,
                                   uint8_t endianness, meter_datatype_t datatype,
                                   int8_t divisor);

/*
 * Returns the byte size of a single register for the given data type.
 * INT16 = 2, INT32/FLOAT32 = 4.
 */
uint8_t meter_register_size(meter_datatype_t datatype);

#ifdef __cplusplus
}
#endif

#endif /* METER_DECODE_H */
