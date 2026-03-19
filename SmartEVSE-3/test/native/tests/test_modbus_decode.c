/*
 * test_modbus_decode.c - Modbus frame parser tests
 *
 * Tests:
 *   - Frame init zeros all fields
 *   - FC03/04 request parsing (6-byte read holding/input register)
 *   - FC03/04 response parsing (variable-length data response)
 *   - FC06 write single register (request vs response disambiguation)
 *   - FC10 write multiple registers (request and response)
 *   - Exception frame parsing (3-byte error)
 *   - Too-short buffer handling
 *   - Invalid data length mismatch
 *   - Request-Response register carry-over for FC03/04
 *   - NULL pointer safety
 *   - Data pointer correctness
 *   - FC06 broadcast treated as request
 */

#include "test_framework.h"
#include "modbus_decode.h"

static modbus_frame_t frame;

/* ---- Init ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-020
 * @scenario Frame init zeros all fields and sets Type to MODBUS_INVALID
 * @given An uninitialized modbus_frame_t
 * @when modbus_frame_init is called
 * @then All fields are zero and Type is MODBUS_INVALID
 */
void test_frame_init(void) {
    memset(&frame, 0xFF, sizeof(frame));
    modbus_frame_init(&frame);

    TEST_ASSERT_EQUAL_INT(0, frame.Address);
    TEST_ASSERT_EQUAL_INT(0, frame.Function);
    TEST_ASSERT_EQUAL_INT(0, frame.Register);
    TEST_ASSERT_EQUAL_INT(0, frame.RegisterCount);
    TEST_ASSERT_EQUAL_INT(0, frame.Value);
    TEST_ASSERT_EQUAL_INT(0, frame.DataLength);
    TEST_ASSERT_EQUAL_INT(MODBUS_INVALID, frame.Type);
    TEST_ASSERT_EQUAL_INT(0, frame.Exception);
    TEST_ASSERT_EQUAL_INT(0, frame.RequestAddress);
    TEST_ASSERT_EQUAL_INT(0, frame.RequestFunction);
    TEST_ASSERT_EQUAL_INT(0, frame.RequestRegister);
}

/* ---- FC03/04 Request ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-021
 * @scenario FC04 read input register request is parsed correctly
 * @given A 6-byte FC04 request: address=0x0A, register=0x0006, count=12
 * @when modbus_decode is called
 * @then Type=MODBUS_REQUEST, Address=0x0A, Function=0x04, Register=0x0006, RegisterCount=12
 */
void test_fc04_read_request(void) {
    /* addr=0x0A, func=0x04, reg=0x0006, count=0x000C */
    uint8_t buf[] = {0x0A, 0x04, 0x00, 0x06, 0x00, 0x0C};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_REQUEST, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x0A, frame.Address);
    TEST_ASSERT_EQUAL_INT(0x04, frame.Function);
    TEST_ASSERT_EQUAL_INT(0x0006, frame.Register);
    TEST_ASSERT_EQUAL_INT(12, frame.RegisterCount);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-022
 * @scenario FC03 read holding register request is parsed the same as FC04
 * @given A 6-byte FC03 request: address=0x01, register=0x5B0C, count=16
 * @when modbus_decode is called
 * @then Type=MODBUS_REQUEST, Function=0x03, Register=0x5B0C, RegisterCount=16
 */
void test_fc03_read_request(void) {
    uint8_t buf[] = {0x01, 0x03, 0x5B, 0x0C, 0x00, 0x10};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_REQUEST, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x03, frame.Function);
    TEST_ASSERT_EQUAL_INT(0x5B0C, frame.Register);
    TEST_ASSERT_EQUAL_INT(16, frame.RegisterCount);
}

/* ---- FC03/04 Response ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-023
 * @scenario FC04 response with 4 bytes of data is parsed correctly
 * @given A FC04 response: address=0x0A, bytecount=4, data=[0x00,0x64,0x00,0xC8]
 * @when modbus_decode is called with a pending request for register 0x0006
 * @then Type=MODBUS_RESPONSE, DataLength=4, Register=0x0006, Data points to payload
 */
void test_fc04_response(void) {
    /* addr=0x0A, func=0x04, bytecount=4, data=0x00640x00C8 */
    uint8_t buf[] = {0x0A, 0x04, 0x04, 0x00, 0x64, 0x00, 0xC8};
    modbus_frame_init(&frame);

    /* Simulate pending request */
    frame.RequestAddress = 0x0A;
    frame.RequestFunction = 0x04;
    frame.RequestRegister = 0x0006;

    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_RESPONSE, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x0A, frame.Address);
    TEST_ASSERT_EQUAL_INT(0x04, frame.Function);
    TEST_ASSERT_EQUAL_INT(4, frame.DataLength);
    /* Register should be carried from request */
    TEST_ASSERT_EQUAL_INT(0x0006, frame.Register);
    /* Data pointer should point to the data portion */
    TEST_ASSERT_TRUE(frame.Data != NULL);
    TEST_ASSERT_EQUAL_INT(0x00, frame.Data[0]);
    TEST_ASSERT_EQUAL_INT(0x64, frame.Data[1]);
}

/* ---- FC06 Write Single Register ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-024
 * @scenario FC06 write single register as initial request (no pending request)
 * @given A 6-byte FC06 packet: address=0x02, register=0x0100, value=0x0020
 * @when modbus_decode is called with no pending request
 * @then Type=MODBUS_REQUEST, Register=0x0100, Value=0x0020
 */
void test_fc06_as_request(void) {
    uint8_t buf[] = {0x02, 0x06, 0x01, 0x00, 0x00, 0x20};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_REQUEST, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x02, frame.Address);
    TEST_ASSERT_EQUAL_INT(0x06, frame.Function);
    TEST_ASSERT_EQUAL_INT(0x0100, frame.Register);
    TEST_ASSERT_EQUAL_INT(0x0020, frame.Value);
    TEST_ASSERT_EQUAL_INT(1, frame.RegisterCount);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-025
 * @scenario FC06 echo treated as response when matching pending request
 * @given A 6-byte FC06 packet with a pending request matching address and function
 * @when modbus_decode is called
 * @then Type=MODBUS_RESPONSE (disambiguated from MODBUS_OK)
 */
void test_fc06_as_response(void) {
    uint8_t buf[] = {0x02, 0x06, 0x01, 0x00, 0x00, 0x20};
    modbus_frame_init(&frame);
    /* Simulate pending request */
    frame.RequestAddress = 0x02;
    frame.RequestFunction = 0x06;
    frame.RequestRegister = 0x0100;

    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_RESPONSE, frame.Type);
    /* Request fields should be cleared after matching */
    TEST_ASSERT_EQUAL_INT(0, frame.RequestAddress);
    TEST_ASSERT_EQUAL_INT(0, frame.RequestFunction);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-026
 * @scenario FC06 to broadcast address is always treated as request
 * @given A 6-byte FC06 packet to broadcast address 0x09 with matching pending request
 * @when modbus_decode is called
 * @then Type=MODBUS_REQUEST (broadcast is never a response)
 */
void test_fc06_broadcast_is_request(void) {
    uint8_t buf[] = {0x09, 0x06, 0x01, 0x00, 0x00, 0x20};
    modbus_frame_init(&frame);
    frame.RequestAddress = 0x09;
    frame.RequestFunction = 0x06;

    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_REQUEST, frame.Type);
}

/* ---- FC10 Write Multiple Registers ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-027
 * @scenario FC10 response (6 bytes) is parsed correctly
 * @given A 6-byte FC10 response: address=0x01, register=0x0020, count=8
 * @when modbus_decode is called
 * @then Type=MODBUS_RESPONSE, Register=0x0020, RegisterCount=8
 */
void test_fc10_response(void) {
    uint8_t buf[] = {0x01, 0x10, 0x00, 0x20, 0x00, 0x08};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_RESPONSE, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x0020, frame.Register);
    TEST_ASSERT_EQUAL_INT(8, frame.RegisterCount);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-028
 * @scenario FC10 request with data payload is parsed correctly
 * @given An FC10 request: address=0x09, register=0x0020, count=2, bytecount=4, data=[0x00,0x3C,0x00,0x50]
 * @when modbus_decode is called
 * @then Type=MODBUS_REQUEST, DataLength=4, Data points to the 4-byte payload
 */
void test_fc10_request_with_data(void) {
    /* addr=0x09, func=0x10, reg=0x0020, count=2, bytecount=4, data */
    uint8_t buf[] = {0x09, 0x10, 0x00, 0x20, 0x00, 0x02, 0x04,
                     0x00, 0x3C, 0x00, 0x50};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_REQUEST, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x0020, frame.Register);
    TEST_ASSERT_EQUAL_INT(2, frame.RegisterCount);
    TEST_ASSERT_EQUAL_INT(4, frame.DataLength);
    TEST_ASSERT_TRUE(frame.Data != NULL);
    TEST_ASSERT_EQUAL_INT(0x00, frame.Data[0]);
    TEST_ASSERT_EQUAL_INT(0x3C, frame.Data[1]);
    TEST_ASSERT_EQUAL_INT(0x00, frame.Data[2]);
    TEST_ASSERT_EQUAL_INT(0x50, frame.Data[3]);
}

/* ---- Exception ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-029
 * @scenario 3-byte exception frame is parsed correctly
 * @given A 3-byte exception: address=0x0A, function=0x84, exception=0x02
 * @when modbus_decode is called
 * @then Type=MODBUS_EXCEPTION, Exception=0x02
 */
void test_exception_frame(void) {
    uint8_t buf[] = {0x0A, 0x84, 0x02};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_EXCEPTION, frame.Type);
    TEST_ASSERT_EQUAL_INT(0x0A, frame.Address);
    TEST_ASSERT_EQUAL_INT(0x84, frame.Function);
    TEST_ASSERT_EQUAL_INT(0x02, frame.Exception);
}

/* ---- Edge Cases ---- */

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-030
 * @scenario Buffer too short (< 3 bytes) results in MODBUS_INVALID
 * @given A 2-byte buffer
 * @when modbus_decode is called
 * @then Type=MODBUS_INVALID
 */
void test_too_short_buffer(void) {
    uint8_t buf[] = {0x0A, 0x04};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, 2);

    TEST_ASSERT_EQUAL_INT(MODBUS_INVALID, frame.Type);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-031
 * @scenario 4-byte buffer (between exception and minimum data) results in MODBUS_INVALID
 * @given A 4-byte buffer that is neither an exception nor a valid data packet
 * @when modbus_decode is called
 * @then Type=MODBUS_INVALID
 */
void test_four_byte_buffer_invalid(void) {
    uint8_t buf[] = {0x0A, 0x04, 0x01, 0x02};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, 4);

    TEST_ASSERT_EQUAL_INT(MODBUS_INVALID, frame.Type);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-032
 * @scenario FC04 response with mismatched byte count results in MODBUS_INVALID
 * @given A FC04 response where bytecount (10) does not match actual data length
 * @when modbus_decode is called
 * @then Type=MODBUS_INVALID because DataLength != len - 3
 */
void test_fc04_response_length_mismatch(void) {
    /* addr=0x0A, func=0x04, bytecount=10 but only 4 bytes follow */
    uint8_t buf[] = {0x0A, 0x04, 0x0A, 0x00, 0x64, 0x00, 0xC8};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_INVALID, frame.Type);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-033
 * @scenario NULL pointer arguments do not crash
 * @given NULL frame or buffer pointer
 * @when modbus_decode is called
 * @then No crash occurs
 */
void test_null_safety(void) {
    uint8_t buf[] = {0x0A, 0x04, 0x00, 0x06, 0x00, 0x0C};

    modbus_decode(NULL, buf, sizeof(buf));
    modbus_decode(&frame, NULL, 6);
    modbus_frame_init(NULL);

    /* If we got here, no crash */
    TEST_ASSERT_TRUE(1);
}

/*
 * @feature Modbus Frame Decoding
 * @req REQ-MTR-034
 * @scenario Unknown function code results in MODBUS_INVALID
 * @given A 6-byte frame with function code 0x01 (read coils, not supported)
 * @when modbus_decode is called
 * @then Type=MODBUS_INVALID
 */
void test_unknown_function_code(void) {
    uint8_t buf[] = {0x0A, 0x01, 0x00, 0x00, 0x00, 0x08};
    modbus_frame_init(&frame);
    modbus_decode(&frame, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(MODBUS_INVALID, frame.Type);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Modbus Frame Decoding");

    RUN_TEST(test_frame_init);
    RUN_TEST(test_fc04_read_request);
    RUN_TEST(test_fc03_read_request);
    RUN_TEST(test_fc04_response);
    RUN_TEST(test_fc06_as_request);
    RUN_TEST(test_fc06_as_response);
    RUN_TEST(test_fc06_broadcast_is_request);
    RUN_TEST(test_fc10_response);
    RUN_TEST(test_fc10_request_with_data);
    RUN_TEST(test_exception_frame);
    RUN_TEST(test_too_short_buffer);
    RUN_TEST(test_four_byte_buffer_invalid);
    RUN_TEST(test_fc04_response_length_mismatch);
    RUN_TEST(test_null_safety);
    RUN_TEST(test_unknown_function_code);

    TEST_SUITE_RESULTS();
}
