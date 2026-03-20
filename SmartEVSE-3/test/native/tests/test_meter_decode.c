/*
 * test_meter_decode.c - Meter byte decoding tests
 *
 * Tests:
 *   - combineBytes for all 4 endianness modes x 3 data types
 *   - decodeMeasurement with positive/negative divisors
 *   - INT16 sign extension
 *   - FLOAT32 decoding with divisor
 *   - Register index offset calculation
 *   - Edge cases: zero divisor, NULL pointers, invalid datatype
 */

#include "test_framework.h"
#include "meter_decode.h"

/* Helper: encode a 32-bit float into big-endian bytes */
static void float_to_be_bytes(float f, uint8_t *out) {
    uint8_t *p = (uint8_t *)&f;
    /* Assuming little-endian host (ESP32, x86, ARM) */
    out[0] = p[3]; out[1] = p[2]; out[2] = p[1]; out[3] = p[0];
}

/* ---- meter_register_size ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-040
 * @scenario Register size returns correct byte count per data type
 * @given The three supported data types
 * @when meter_register_size is called for each
 * @then INT16 returns 2, INT32 returns 4, FLOAT32 returns 4
 */
void test_register_size(void) {
    TEST_ASSERT_EQUAL_INT(4, meter_register_size(METER_DATATYPE_INT32));
    TEST_ASSERT_EQUAL_INT(4, meter_register_size(METER_DATATYPE_FLOAT32));
    TEST_ASSERT_EQUAL_INT(2, meter_register_size(METER_DATATYPE_INT16));
}

/* ---- combineBytes: HBF_HWF (big endian) ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-041
 * @scenario HBF_HWF INT32: big-endian 0x00000064 decodes to 100
 * @given 4 bytes in big-endian order representing value 100
 * @when meter_combine_bytes is called with ENDIANNESS_HBF_HWF, INT32
 * @then Output equals 100
 */
void test_combine_hbf_hwf_int32(void) {
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x64};
    int32_t val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_HBF_HWF, METER_DATATYPE_INT32);
    TEST_ASSERT_EQUAL_INT(100, val);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-042
 * @scenario HBF_HWF INT16: big-endian 0x00C8 decodes to 200
 * @given 2 bytes in big-endian order representing value 200
 * @when meter_combine_bytes is called with ENDIANNESS_HBF_HWF, INT16
 * @then Output equals 200
 */
void test_combine_hbf_hwf_int16(void) {
    uint8_t buf[] = {0x00, 0xC8};
    int32_t val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_HBF_HWF, METER_DATATYPE_INT16);
    TEST_ASSERT_EQUAL_INT(200, (int16_t)val);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-043
 * @scenario HBF_HWF FLOAT32: big-endian IEEE 754 42.5f decodes correctly
 * @given 4 bytes representing float 42.5 in big-endian
 * @when meter_combine_bytes is called with ENDIANNESS_HBF_HWF, FLOAT32
 * @then Output float equals 42.5
 */
void test_combine_hbf_hwf_float32(void) {
    uint8_t buf[4];
    float_to_be_bytes(42.5f, buf);
    float val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_HBF_HWF, METER_DATATYPE_FLOAT32);
    /* Compare as int to avoid float comparison issues; 42.5 is exact in IEEE754 */
    TEST_ASSERT_EQUAL_INT(42, (int)val);
}

/* ---- combineBytes: LBF_LWF (little endian) ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-044
 * @scenario LBF_LWF INT32: little-endian 0x64000000 in bytes decodes to 100
 * @given 4 bytes in little-endian order representing value 100
 * @when meter_combine_bytes is called with ENDIANNESS_LBF_LWF, INT32
 * @then Output equals 100
 */
void test_combine_lbf_lwf_int32(void) {
    uint8_t buf[] = {0x64, 0x00, 0x00, 0x00};
    int32_t val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_LBF_LWF, METER_DATATYPE_INT32);
    TEST_ASSERT_EQUAL_INT(100, val);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-045
 * @scenario LBF_LWF INT16: little-endian 0xC800 in bytes decodes to 200
 * @given 2 bytes in little-endian order representing value 200
 * @when meter_combine_bytes is called with ENDIANNESS_LBF_LWF, INT16
 * @then Output equals 200
 */
void test_combine_lbf_lwf_int16(void) {
    uint8_t buf[] = {0xC8, 0x00};
    int32_t val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_LBF_LWF, METER_DATATYPE_INT16);
    TEST_ASSERT_EQUAL_INT(200, (int16_t)val);
}

/* ---- combineBytes: HBF_LWF (high byte, low word) ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-046
 * @scenario HBF_LWF INT32: word-swapped big-endian decodes correctly
 * @given 4 bytes: low word [0x00, 0x01] then high word [0x00, 0x00] = value 1
 * @when meter_combine_bytes is called with ENDIANNESS_HBF_LWF, INT32
 * @then Output equals 1
 */
void test_combine_hbf_lwf_int32(void) {
    /* HBF_LWF: bytes are high-byte-first within each word, low word first
     * Value 0x00010000 = 65536: words are [0x00,0x00] (low) [0x00,0x01] (high)
     * But in HBF_LWF memory layout: buf[0..1]=low_word_hbf, buf[2..3]=high_word_hbf
     * Phoenix Contact uses this.
     * For value 100 = 0x00000064: low word = 0x0064, high word = 0x0000
     * HBF within word: [0x00, 0x64, 0x00, 0x00] */
    uint8_t buf[] = {0x00, 0x64, 0x00, 0x00};
    int32_t val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_HBF_LWF, METER_DATATYPE_INT32);
    TEST_ASSERT_EQUAL_INT(100, val);
}

/* ---- combineBytes: LBF_HWF (low byte, high word) ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-047
 * @scenario LBF_HWF INT32: word-swapped little-endian decodes correctly
 * @given 4 bytes: high word [0x00, 0x00] then low word [0x64, 0x00] = value 100
 * @when meter_combine_bytes is called with ENDIANNESS_LBF_HWF, INT32
 * @then Output equals 100
 */
void test_combine_lbf_hwf_int32(void) {
    /* LBF_HWF: bytes are low-byte-first within each word, high word first
     * Value 100 = 0x00000064: high word = 0x0000, low word = 0x0064
     * LBF within word: high=[0x00, 0x00], low=[0x64, 0x00]
     * Memory: [0x00, 0x00, 0x64, 0x00] */
    uint8_t buf[] = {0x00, 0x00, 0x64, 0x00};
    int32_t val = 0;
    meter_combine_bytes(&val, buf, 0, ENDIANNESS_LBF_HWF, METER_DATATYPE_INT32);
    TEST_ASSERT_EQUAL_INT(100, val);
}

/* ---- decodeMeasurement: INT32 with divisors ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-048
 * @scenario INT32 with zero divisor returns raw value
 * @given Big-endian INT32 buffer with value 12345
 * @when meter_decode_value is called with divisor=0
 * @then Result value is 12345
 */
void test_decode_int32_divisor_zero(void) {
    /* 12345 = 0x00003039, big-endian */
    uint8_t buf[] = {0x00, 0x00, 0x30, 0x39};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(12345, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-049
 * @scenario INT32 with positive divisor divides by power of 10
 * @given Big-endian INT32 buffer with value 12345
 * @when meter_decode_value is called with divisor=2 (divide by 100)
 * @then Result value is 123
 */
void test_decode_int32_positive_divisor(void) {
    uint8_t buf[] = {0x00, 0x00, 0x30, 0x39};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, 2);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(123, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-050
 * @scenario INT32 with negative divisor multiplies by power of 10
 * @given Big-endian INT32 buffer with value 42
 * @when meter_decode_value is called with divisor=-3 (multiply by 1000)
 * @then Result value is 42000
 */
void test_decode_int32_negative_divisor(void) {
    /* 42 = 0x0000002A */
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x2A};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, -3);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(42000, r.value);
}

/* ---- decodeMeasurement: INT16 ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-051
 * @scenario INT16 positive value with divisor
 * @given Big-endian INT16 buffer with value 2500
 * @when meter_decode_value is called with divisor=1 (divide by 10)
 * @then Result value is 250
 */
void test_decode_int16_positive(void) {
    /* 2500 = 0x09C4 */
    uint8_t buf[] = {0x09, 0xC4};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT16, 1);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(250, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-052
 * @scenario INT16 sign extension: negative value 0xFFCE = -50
 * @given Big-endian INT16 buffer with value -50 (0xFFCE)
 * @when meter_decode_value is called with divisor=0
 * @then Result value is -50 (sign-extended to 32-bit)
 */
void test_decode_int16_sign_extension(void) {
    /* -50 = 0xFFCE */
    uint8_t buf[] = {0xFF, 0xCE};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT16, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(-50, r.value);
}

/* ---- decodeMeasurement: FLOAT32 ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-053
 * @scenario FLOAT32 with zero divisor returns truncated integer
 * @given Big-endian FLOAT32 buffer with value 230.5
 * @when meter_decode_value is called with divisor=0
 * @then Result value is 230 (truncated from 230.5)
 */
void test_decode_float32_divisor_zero(void) {
    uint8_t buf[4];
    float_to_be_bytes(230.5f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(230, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-054
 * @scenario FLOAT32 with negative divisor: multiply 2.345 by 1000
 * @given Big-endian FLOAT32 buffer with value 2.345
 * @when meter_decode_value is called with divisor=-3 (multiply by 1000)
 * @then Result value is 2345
 */
void test_decode_float32_negative_divisor(void) {
    uint8_t buf[4];
    float_to_be_bytes(2.345f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, -3);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(2345, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-055
 * @scenario FLOAT32 with positive divisor: divide 23450.0 by 10
 * @given Big-endian FLOAT32 buffer with value 23450.0
 * @when meter_decode_value is called with divisor=1 (divide by 10)
 * @then Result value is 2345
 */
void test_decode_float32_positive_divisor(void) {
    uint8_t buf[4];
    float_to_be_bytes(23450.0f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 1);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(2345, r.value);
}

/* ---- Register index offset ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-056
 * @scenario Index parameter selects correct register from buffer
 * @given A buffer with 3 INT32 values in big-endian: [100, 200, 300]
 * @when meter_decode_value is called with index=0, 1, and 2
 * @then Returns 100, 200, 300 respectively
 */
void test_decode_index_offset(void) {
    /* 100=0x00000064, 200=0x000000C8, 300=0x0000012C */
    uint8_t buf[] = {
        0x00, 0x00, 0x00, 0x64,
        0x00, 0x00, 0x00, 0xC8,
        0x00, 0x00, 0x01, 0x2C
    };
    meter_reading_t r0 = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_INT32, 0);
    meter_reading_t r1 = meter_decode_value(buf, 1, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_INT32, 0);
    meter_reading_t r2 = meter_decode_value(buf, 2, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_INT32, 0);
    TEST_ASSERT_EQUAL_INT(100, r0.value);
    TEST_ASSERT_EQUAL_INT(200, r1.value);
    TEST_ASSERT_EQUAL_INT(300, r2.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-057
 * @scenario INT16 index offset uses 2-byte stride
 * @given A buffer with 3 INT16 values in big-endian: [10, 20, 30]
 * @when meter_decode_value is called with index=0, 1, and 2
 * @then Returns 10, 20, 30 respectively
 */
void test_decode_int16_index_offset(void) {
    /* 10=0x000A, 20=0x0014, 30=0x001E */
    uint8_t buf[] = {0x00, 0x0A, 0x00, 0x14, 0x00, 0x1E};
    meter_reading_t r0 = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_INT16, 0);
    meter_reading_t r1 = meter_decode_value(buf, 1, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_INT16, 0);
    meter_reading_t r2 = meter_decode_value(buf, 2, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_INT16, 0);
    TEST_ASSERT_EQUAL_INT(10, r0.value);
    TEST_ASSERT_EQUAL_INT(20, r1.value);
    TEST_ASSERT_EQUAL_INT(30, r2.value);
}

/* ---- Negative INT32 values ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-058
 * @scenario Negative INT32 value decodes correctly
 * @given Big-endian INT32 buffer with value -1000 (0xFFFFFC18)
 * @when meter_decode_value is called with divisor=0
 * @then Result value is -1000
 */
void test_decode_int32_negative(void) {
    /* -1000 = 0xFFFFFC18 */
    uint8_t buf[] = {0xFF, 0xFF, 0xFC, 0x18};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(-1000, r.value);
}

/* ---- Negative FLOAT32 ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-059
 * @scenario Negative FLOAT32 value decodes correctly
 * @given Big-endian FLOAT32 buffer with value -5.0
 * @when meter_decode_value is called with divisor=0
 * @then Result value is -5
 */
void test_decode_float32_negative(void) {
    uint8_t buf[4];
    float_to_be_bytes(-5.0f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(-5, r.value);
}

/* ---- Edge cases ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-060
 * @scenario NULL buffer returns invalid result
 * @given A NULL buffer pointer
 * @when meter_decode_value is called
 * @then Result valid is 0
 */
void test_decode_null_buffer(void) {
    meter_reading_t r = meter_decode_value(NULL, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, 0);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-061
 * @scenario Invalid datatype returns invalid result
 * @given A valid buffer but datatype=METER_DATATYPE_MAX (out of range)
 * @when meter_decode_value is called
 * @then Result valid is 0
 */
void test_decode_invalid_datatype(void) {
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x64};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_MAX, 0);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-062
 * @scenario Divisor out of pow10 range returns invalid result
 * @given A valid buffer with divisor=10 (exceeds pow10_table size)
 * @when meter_decode_value is called
 * @then Result valid is 0
 */
void test_decode_divisor_out_of_range(void) {
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x64};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, 10);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-063
 * @scenario NULL pointer to meter_combine_bytes does not crash
 * @given NULL out and buf pointers
 * @when meter_combine_bytes is called
 * @then No crash occurs
 */
void test_combine_null_safety(void) {
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x64};
    int32_t val = 42;
    meter_combine_bytes(NULL, buf, 0, ENDIANNESS_HBF_HWF, METER_DATATYPE_INT32);
    meter_combine_bytes(&val, NULL, 0, ENDIANNESS_HBF_HWF, METER_DATATYPE_INT32);
    /* val unchanged because buf was NULL */
    TEST_ASSERT_EQUAL_INT(42, val);
}

/* ---- Real-world meter scenario: Phoenix Contact ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-064
 * @scenario Phoenix Contact meter HBF_LWF INT32 current reading
 * @given Phoenix Contact response with current 23.12A encoded as 23120 mA in HBF_LWF INT32
 * @when meter_decode_value is called with divisor=3 (divide by 1000 to get 0.1A units)
 * @then Result value is 23 (23.12A in deciAmpere after /1000 = 23)
 */
void test_phoenix_contact_current(void) {
    /* 23120 = 0x00005A50; HBF_LWF: low_word=[0x5A,0x50], high_word=[0x00,0x00] */
    uint8_t buf[] = {0x5A, 0x50, 0x00, 0x00};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_LWF,
                                           METER_DATATYPE_INT32, 3);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(23, r.value);
}

/* ---- Real-world meter scenario: Eastron SDM630 ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-065
 * @scenario Eastron SDM630 HBF_HWF FLOAT32 current reading
 * @given Eastron response with 16.5A encoded as IEEE 754 float in HBF_HWF
 * @when meter_decode_value is called with divisor=-3 (multiply by 1000 for mA)
 * @then Result value is 16500 (mA)
 */
void test_eastron_float_current(void) {
    uint8_t buf[4];
    float_to_be_bytes(16.5f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, -3);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(16500, r.value);
}

/* ---- Real-world meter scenario: Orno WE-517 (3-phase) ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-066
 * @scenario Orno WE-517 3-phase current reading at register 0x0C
 * @given Orno response with 3 phase currents [8.5A, 12.3A, 6.7A] as FLOAT32 HBF_HWF
 * @when meter_decode_value is called for indices 0, 1, 2 with divisor=0
 * @then Returns 8, 12, 6 (truncated integer amps)
 */
void test_orno3p_current(void) {
    /* 3 FLOAT32 values at register 0x0C: L1=8.5, L2=12.3, L3=6.7 */
    uint8_t buf[12];
    float_to_be_bytes(8.5f, buf);
    float_to_be_bytes(12.3f, buf + 4);
    float_to_be_bytes(6.7f, buf + 8);

    meter_reading_t r0 = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_FLOAT32, 0);
    meter_reading_t r1 = meter_decode_value(buf, 1, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_FLOAT32, 0);
    meter_reading_t r2 = meter_decode_value(buf, 2, ENDIANNESS_HBF_HWF,
                                            METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(8, r0.value);
    TEST_ASSERT_EQUAL_INT(12, r1.value);
    TEST_ASSERT_EQUAL_INT(6, r2.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-067
 * @scenario Orno WE-517 total active power reading
 * @given Orno response with total power 3456.7W as FLOAT32 HBF_HWF at register 0x1C
 * @when meter_decode_value is called with divisor=0
 * @then Returns 3456
 */
void test_orno3p_power(void) {
    uint8_t buf[4];
    float_to_be_bytes(3456.7f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(3456, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-068
 * @scenario Orno WE-517 import energy reading in kWh
 * @given Orno response with 1234.567 kWh as FLOAT32 HBF_HWF at register 0x0100
 * @when meter_decode_value is called with divisor=-3 (multiply by 1000 to get Wh)
 * @then Returns 1234567 (Wh)
 */
void test_orno3p_energy(void) {
    uint8_t buf[4];
    float_to_be_bytes(1234.567f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, -3);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    /* 1234.567 * 1000 = 1234567 (float precision allows exact representation) */
    TEST_ASSERT_EQUAL_INT(1234567, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-069
 * @scenario Orno WE-517 negative power during export (solar feed-in)
 * @given Orno response with -1500.0W as FLOAT32 HBF_HWF
 * @when meter_decode_value is called with divisor=0
 * @then Returns -1500
 */
void test_orno3p_negative_power(void) {
    uint8_t buf[4];
    float_to_be_bytes(-1500.0f, buf);
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(-1500, r.value);
}

/* ---- Input validation hardening ---- */

/*
 * @feature Meter Decoding
 * @req REQ-MTR-087
 * @scenario INT8_MIN divisor (-128) is rejected to avoid negation UB
 * @given A valid buffer and divisor=-128 (INT8_MIN)
 * @when meter_decode_value is called
 * @then Result is invalid because -128 is outside pow10 table range
 */
void test_decode_divisor_int8_min(void) {
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x64};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, -128);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-088
 * @scenario FLOAT32 NaN value from corrupt meter data is rejected
 * @given Buffer containing IEEE 754 NaN bit pattern (0x7FC00000)
 * @when meter_decode_value is called with FLOAT32 datatype
 * @then Result is invalid
 */
void test_decode_float32_nan_rejected(void) {
    /* IEEE 754 quiet NaN: 0x7FC00000 in big-endian */
    uint8_t buf[] = {0x7F, 0xC0, 0x00, 0x00};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-089
 * @scenario FLOAT32 Infinity value from corrupt meter data is rejected
 * @given Buffer containing IEEE 754 +Infinity bit pattern (0x7F800000)
 * @when meter_decode_value is called with FLOAT32 datatype
 * @then Result is invalid
 */
void test_decode_float32_inf_rejected(void) {
    /* IEEE 754 +Infinity: 0x7F800000 in big-endian */
    uint8_t buf[] = {0x7F, 0x80, 0x00, 0x00};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_FLOAT32, 0);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-090
 * @scenario INT32 multiplication overflow is detected and rejected
 * @given Buffer with INT32 value near INT32_MAX/1000 and divisor=-3
 * @when meter_decode_value is called
 * @then Result is invalid because value * 1000 would overflow int32_t
 */
void test_decode_int32_multiply_overflow(void) {
    /* INT32_MAX / 1000 = 2147483 (rounded down). Use value 2147484 to trigger overflow.
     * 2147484 = 0x0020C49C in big-endian */
    uint8_t buf[] = {0x00, 0x20, 0xC4, 0x9C};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, -3);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-091
 * @scenario INT32 multiplication that fits is still accepted
 * @given Buffer with INT32 value 2147483 and divisor=-3
 * @when meter_decode_value is called
 * @then Result is valid with value 2147483000
 */
void test_decode_int32_multiply_max_valid(void) {
    /* 2147483 = 0x0020C49B in big-endian */
    uint8_t buf[] = {0x00, 0x20, 0xC4, 0x9B};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, -3);
    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(2147483000, r.value);
}

/*
 * @feature Meter Decoding
 * @req REQ-MTR-092
 * @scenario Negative INT32 multiplication overflow is detected
 * @given Buffer with large negative INT32 value and divisor=-3
 * @when meter_decode_value is called
 * @then Result is invalid because value * 1000 would overflow
 */
void test_decode_int32_negative_multiply_overflow(void) {
    /* -2147484 = 0xFFDF3B64 in big-endian */
    uint8_t buf[] = {0xFF, 0xDF, 0x3B, 0x64};
    meter_reading_t r = meter_decode_value(buf, 0, ENDIANNESS_HBF_HWF,
                                           METER_DATATYPE_INT32, -3);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Meter Decoding");

    RUN_TEST(test_register_size);
    RUN_TEST(test_combine_hbf_hwf_int32);
    RUN_TEST(test_combine_hbf_hwf_int16);
    RUN_TEST(test_combine_hbf_hwf_float32);
    RUN_TEST(test_combine_lbf_lwf_int32);
    RUN_TEST(test_combine_lbf_lwf_int16);
    RUN_TEST(test_combine_hbf_lwf_int32);
    RUN_TEST(test_combine_lbf_hwf_int32);
    RUN_TEST(test_decode_int32_divisor_zero);
    RUN_TEST(test_decode_int32_positive_divisor);
    RUN_TEST(test_decode_int32_negative_divisor);
    RUN_TEST(test_decode_int16_positive);
    RUN_TEST(test_decode_int16_sign_extension);
    RUN_TEST(test_decode_float32_divisor_zero);
    RUN_TEST(test_decode_float32_negative_divisor);
    RUN_TEST(test_decode_float32_positive_divisor);
    RUN_TEST(test_decode_index_offset);
    RUN_TEST(test_decode_int16_index_offset);
    RUN_TEST(test_decode_int32_negative);
    RUN_TEST(test_decode_float32_negative);
    RUN_TEST(test_decode_null_buffer);
    RUN_TEST(test_decode_invalid_datatype);
    RUN_TEST(test_decode_divisor_out_of_range);
    RUN_TEST(test_combine_null_safety);
    RUN_TEST(test_phoenix_contact_current);
    RUN_TEST(test_eastron_float_current);
    RUN_TEST(test_orno3p_current);
    RUN_TEST(test_orno3p_power);
    RUN_TEST(test_orno3p_energy);
    RUN_TEST(test_orno3p_negative_power);

    // Input validation hardening
    RUN_TEST(test_decode_divisor_int8_min);
    RUN_TEST(test_decode_float32_nan_rejected);
    RUN_TEST(test_decode_float32_inf_rejected);
    RUN_TEST(test_decode_int32_multiply_overflow);
    RUN_TEST(test_decode_int32_multiply_max_valid);
    RUN_TEST(test_decode_int32_negative_multiply_overflow);

    TEST_SUITE_RESULTS();
}
