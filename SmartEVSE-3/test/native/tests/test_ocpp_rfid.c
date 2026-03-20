/*
 * test_ocpp_rfid.c - OCPP RFID hex formatting tests
 *
 * Tests the pure C RFID-to-hex conversion extracted from esp32.cpp ocppLoop().
 * The function must handle old reader format (rfid[0]==0x01, 6-byte UID at [1])
 * and new reader format (7-byte UID at [0]).
 */

#include "test_framework.h"
#include "ocpp_logic.h"
#include <string.h>

/* ---- New reader format (7-byte UID) ---- */

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-054
 * @scenario New reader 7-byte UUID formatted as 14-char hex string
 * @given RFID bytes {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67} (new reader)
 * @when ocpp_format_rfid_hex is called
 * @then Output is "ABCDEF01234567" (7 bytes * 2 hex chars)
 */
void test_rfid_new_reader_7byte(void) {
    uint8_t rfid[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67};
    char out[OCPP_RFID_HEX_MAX];
    ocpp_format_rfid_hex(rfid, 7, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ABCDEF01234567", out);
}

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-052
 * @scenario UUID with leading zeros preserved in hex output
 * @given RFID bytes {0x00, 0x00, 0x0A, 0x0B, 0x00, 0x00, 0x00} (new reader with leading zeros)
 * @when ocpp_format_rfid_hex is called
 * @then Output is "0000 0A0B000000" with leading zeros preserved
 */
void test_rfid_leading_zeros_preserved(void) {
    uint8_t rfid[] = {0x00, 0x00, 0x0A, 0x0B, 0x00, 0x00, 0x00};
    char out[OCPP_RFID_HEX_MAX];
    ocpp_format_rfid_hex(rfid, 7, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("00000A0B000000", out);
}

/* ---- Old reader format (rfid[0]==0x01, 6-byte UID at [1]) ---- */

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-053
 * @scenario Old reader format uses RFID[1..6] offset
 * @given RFID bytes {0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF} (old reader flag at [0])
 * @when ocpp_format_rfid_hex is called
 * @then Output is "AABBCCDDEEFF" (6 bytes from [1] to [6])
 */
void test_rfid_old_reader_6byte(void) {
    uint8_t rfid[] = {0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    char out[OCPP_RFID_HEX_MAX];
    ocpp_format_rfid_hex(rfid, 7, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF", out);
}

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-053
 * @scenario Old reader with leading zero bytes preserved
 * @given RFID bytes {0x01, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04} (old reader)
 * @when ocpp_format_rfid_hex is called
 * @then Output is "000001020304" with leading zeros from [1] preserved
 */
void test_rfid_old_reader_leading_zeros(void) {
    uint8_t rfid[] = {0x01, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04};
    char out[OCPP_RFID_HEX_MAX];
    ocpp_format_rfid_hex(rfid, 7, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("000001020304", out);
}

/* ---- 4-byte UID (short format, new reader) ---- */

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-050
 * @scenario 4-byte UUID formatted as 8-char hex string
 * @given RFID bytes {0xDE, 0xAD, 0xBE, 0xEF} (4-byte UID, new reader)
 * @when ocpp_format_rfid_hex is called with rfid_len=4
 * @then Output is "DEADBEEF" (4 bytes * 2 hex chars)
 */
void test_rfid_4byte_new_reader(void) {
    uint8_t rfid[] = {0xDE, 0xAD, 0xBE, 0xEF};
    char out[OCPP_RFID_HEX_MAX];
    ocpp_format_rfid_hex(rfid, 4, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("DEADBEEF", out);
}

/* ---- Edge cases ---- */

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-050
 * @scenario NULL RFID input produces empty string
 * @given RFID pointer is NULL
 * @when ocpp_format_rfid_hex is called
 * @then Output is empty string
 */
void test_rfid_null_input(void) {
    char out[OCPP_RFID_HEX_MAX];
    out[0] = 'X';  /* Pre-fill to detect overwrite */
    ocpp_format_rfid_hex(NULL, 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-050
 * @scenario Zero-length RFID produces empty string
 * @given RFID bytes exist but length is 0
 * @when ocpp_format_rfid_hex is called
 * @then Output is empty string
 */
void test_rfid_zero_length(void) {
    uint8_t rfid[] = {0xAA};
    char out[OCPP_RFID_HEX_MAX];
    ocpp_format_rfid_hex(rfid, 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

/* ---- Small buffer boundary ---- */

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-095
 * @scenario 3-byte output buffer fits exactly one hex byte plus null
 * @given RFID bytes {0xAB} and output buffer of size 3
 * @when ocpp_format_rfid_hex is called
 * @then Output is "AB" (2 hex chars + null fits exactly in 3 bytes)
 */
void test_rfid_format_small_buffer_boundary(void) {
    uint8_t rfid[] = {0xAB, 0xCD};
    char out[3];
    ocpp_format_rfid_hex(rfid, 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("AB", out);
}

/*
 * @feature OCPP RFID Formatting
 * @req REQ-OCPP-095
 * @scenario 2-byte output buffer cannot fit any hex byte (needs 3: 2 chars + null)
 * @given RFID bytes {0xAB} and output buffer of size 2
 * @when ocpp_format_rfid_hex is called
 * @then Output is empty string because 2 hex chars + null requires 3 bytes
 */
void test_rfid_format_2byte_buffer_empty(void) {
    uint8_t rfid[] = {0xAB};
    char out[2];
    ocpp_format_rfid_hex(rfid, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP RFID Formatting");

    RUN_TEST(test_rfid_new_reader_7byte);
    RUN_TEST(test_rfid_leading_zeros_preserved);
    RUN_TEST(test_rfid_old_reader_6byte);
    RUN_TEST(test_rfid_old_reader_leading_zeros);
    RUN_TEST(test_rfid_4byte_new_reader);
    RUN_TEST(test_rfid_null_input);
    RUN_TEST(test_rfid_zero_length);
    RUN_TEST(test_rfid_format_small_buffer_boundary);
    RUN_TEST(test_rfid_format_2byte_buffer_empty);

    TEST_SUITE_RESULTS();
}
