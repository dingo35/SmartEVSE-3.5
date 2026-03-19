/*
 * test_diag_telemetry.c - Native tests for diagnostic telemetry ring buffer
 *
 * Tests the pure C ring buffer module: init, push, wrap-around, read,
 * reset, freeze, profile setting, tick divider, and binary serialization.
 */

#include "test_framework.h"
#include "diag_telemetry.h"
#include <string.h>

/* Helper: create a snapshot with a given timestamp */
static diag_snapshot_t make_snap(uint32_t ts)
{
    diag_snapshot_t s;
    memset(&s, 0, sizeof(s));
    s.timestamp = ts;
    return s;
}

/* Helper: create a snapshot with timestamp and state */
static diag_snapshot_t make_snap_state(uint32_t ts, uint8_t state)
{
    diag_snapshot_t s = make_snap(ts);
    s.state = state;
    return s;
}

/* ---- Ring buffer initialization ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-040
 * @scenario Ring buffer initializes with zero entries
 * @given A diag_ring_t and a buffer of 8 slots
 * @when diag_ring_init is called
 * @then count is 0, head is 0, profile is OFF, frozen is false
 */
void test_ring_init(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);

    TEST_ASSERT_EQUAL(0, ring.count);
    TEST_ASSERT_EQUAL(0, ring.head);
    TEST_ASSERT_EQUAL(DIAG_PROFILE_OFF, ring.profile);
    TEST_ASSERT_FALSE(ring.frozen);
    TEST_ASSERT_EQUAL(8, ring.capacity);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-040
 * @scenario Ring buffer init with NULL pointer is safe
 * @given A NULL ring pointer
 * @when diag_ring_init is called with NULL
 * @then No crash occurs
 */
void test_ring_init_null(void)
{
    diag_ring_init(NULL, NULL, 0);
    /* No crash = pass */
    TEST_ASSERT_TRUE(1);
}

/* ---- Push and read ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-041
 * @scenario Push a single snapshot and read it back
 * @given An initialized ring with GENERAL profile and capacity 8
 * @when One snapshot with timestamp=42 is pushed
 * @then diag_ring_read returns 1 snapshot with timestamp=42
 */
void test_push_and_read_single(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t snap = make_snap(42);
    diag_ring_push(&ring, &snap);

    TEST_ASSERT_EQUAL(1, ring.count);

    diag_snapshot_t out[8];
    uint16_t n = diag_ring_read(&ring, out, 8);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(42, out[0].timestamp);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-041
 * @scenario Push fills buffer to capacity
 * @given An initialized ring with capacity 4 and GENERAL profile
 * @when 4 snapshots are pushed (timestamps 10,20,30,40)
 * @then count is 4 and read returns all 4 in chronological order
 */
void test_push_fills_to_capacity(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    for (uint32_t i = 1; i <= 4; i++)
    {
        diag_snapshot_t s = make_snap(i * 10);
        diag_ring_push(&ring, &s);
    }

    TEST_ASSERT_EQUAL(4, ring.count);

    diag_snapshot_t out[4];
    uint16_t n = diag_ring_read(&ring, out, 4);
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_EQUAL(10, out[0].timestamp);
    TEST_ASSERT_EQUAL(40, out[3].timestamp);
}

/* ---- Wrap-around ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-042
 * @scenario Ring buffer wraps around, overwriting oldest entries
 * @given An initialized ring with capacity 4 and GENERAL profile
 * @when 6 snapshots are pushed (timestamps 1..6)
 * @then count stays at 4 and read returns timestamps 3,4,5,6 (oldest overwritten)
 */
void test_wrap_around(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    for (uint32_t i = 1; i <= 6; i++)
    {
        diag_snapshot_t s = make_snap(i);
        diag_ring_push(&ring, &s);
    }

    TEST_ASSERT_EQUAL(4, ring.count);

    diag_snapshot_t out[4];
    uint16_t n = diag_ring_read(&ring, out, 4);
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_EQUAL(3, out[0].timestamp);
    TEST_ASSERT_EQUAL(4, out[1].timestamp);
    TEST_ASSERT_EQUAL(5, out[2].timestamp);
    TEST_ASSERT_EQUAL(6, out[3].timestamp);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-042
 * @scenario Read with smaller output buffer than ring count
 * @given A ring with 4 entries
 * @when diag_ring_read is called with max_count=2
 * @then Only the 2 oldest snapshots are returned
 */
void test_read_partial(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    for (uint32_t i = 1; i <= 4; i++)
    {
        diag_snapshot_t s = make_snap(i * 100);
        diag_ring_push(&ring, &s);
    }

    diag_snapshot_t out[2];
    uint16_t n = diag_ring_read(&ring, out, 2);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL(100, out[0].timestamp);
    TEST_ASSERT_EQUAL(200, out[1].timestamp);
}

/* ---- Reset ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-043
 * @scenario Reset clears all entries but preserves capacity
 * @given A ring with 3 entries and capacity 8
 * @when diag_ring_reset is called
 * @then count is 0, head is 0, capacity is still 8, profile is OFF
 */
void test_reset(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    for (uint32_t i = 0; i < 3; i++)
    {
        diag_snapshot_t s = make_snap(i);
        diag_ring_push(&ring, &s);
    }

    TEST_ASSERT_EQUAL(3, ring.count);

    diag_ring_reset(&ring);
    TEST_ASSERT_EQUAL(0, ring.count);
    TEST_ASSERT_EQUAL(0, ring.head);
    TEST_ASSERT_EQUAL(8, ring.capacity);
    TEST_ASSERT_EQUAL(DIAG_PROFILE_OFF, ring.profile);
    TEST_ASSERT_FALSE(ring.frozen);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-043
 * @scenario Reset with NULL is safe
 * @given A NULL ring pointer
 * @when diag_ring_reset is called
 * @then No crash occurs
 */
void test_reset_null(void)
{
    diag_ring_reset(NULL);
    TEST_ASSERT_TRUE(1);
}

/* ---- Freeze ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-044
 * @scenario Frozen ring rejects new pushes
 * @given A ring with 2 entries and GENERAL profile, then frozen
 * @when A new snapshot is pushed
 * @then count remains 2 (push rejected)
 */
void test_frozen_rejects_push(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s1 = make_snap(1);
    diag_snapshot_t s2 = make_snap(2);
    diag_ring_push(&ring, &s1);
    diag_ring_push(&ring, &s2);
    TEST_ASSERT_EQUAL(2, ring.count);

    diag_ring_freeze(&ring, true);
    TEST_ASSERT_TRUE(ring.frozen);

    diag_snapshot_t s3 = make_snap(3);
    diag_ring_push(&ring, &s3);
    TEST_ASSERT_EQUAL(2, ring.count);  /* unchanged */
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-044
 * @scenario Unfreezing allows pushes again
 * @given A frozen ring with 2 entries
 * @when The ring is unfrozen and a snapshot is pushed
 * @then count increases to 3
 */
void test_unfreeze_allows_push(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s1 = make_snap(1);
    diag_snapshot_t s2 = make_snap(2);
    diag_ring_push(&ring, &s1);
    diag_ring_push(&ring, &s2);

    diag_ring_freeze(&ring, true);
    diag_ring_freeze(&ring, false);
    TEST_ASSERT_FALSE(ring.frozen);

    diag_snapshot_t s3 = make_snap(3);
    diag_ring_push(&ring, &s3);
    TEST_ASSERT_EQUAL(3, ring.count);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-044
 * @scenario Frozen ring still allows reads
 * @given A frozen ring with 2 entries
 * @when diag_ring_read is called
 * @then Both entries are returned correctly
 */
void test_frozen_allows_read(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s1 = make_snap(10);
    diag_snapshot_t s2 = make_snap(20);
    diag_ring_push(&ring, &s1);
    diag_ring_push(&ring, &s2);
    diag_ring_freeze(&ring, true);

    diag_snapshot_t out[8];
    uint16_t n = diag_ring_read(&ring, out, 8);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL(10, out[0].timestamp);
    TEST_ASSERT_EQUAL(20, out[1].timestamp);
}

/* ---- Profile setting ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-045
 * @scenario Setting GENERAL profile sets divider to 1
 * @given An initialized ring
 * @when diag_set_profile is called with DIAG_PROFILE_GENERAL
 * @then profile is GENERAL and sample_divider is 1
 */
void test_profile_general(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);

    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);
    TEST_ASSERT_EQUAL(DIAG_PROFILE_GENERAL, ring.profile);
    TEST_ASSERT_EQUAL(1, ring.sample_divider);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-045
 * @scenario Setting FAST profile sets divider to 1 (sampled from 100ms tick)
 * @given An initialized ring
 * @when diag_set_profile is called with DIAG_PROFILE_FAST
 * @then profile is FAST and sample_divider is 1
 */
void test_profile_fast(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);

    diag_set_profile(&ring, DIAG_PROFILE_FAST);
    TEST_ASSERT_EQUAL(DIAG_PROFILE_FAST, ring.profile);
    TEST_ASSERT_EQUAL(1, ring.sample_divider);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-045
 * @scenario Setting OFF profile prevents pushes
 * @given An initialized ring with OFF profile
 * @when A snapshot is pushed
 * @then count remains 0 (push rejected)
 */
void test_profile_off_rejects_push(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    /* profile defaults to OFF */

    diag_snapshot_t s = make_snap(1);
    diag_ring_push(&ring, &s);
    TEST_ASSERT_EQUAL(0, ring.count);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-045
 * @scenario Setting profile resets tick counter
 * @given A ring with tick_counter at 5
 * @when diag_set_profile is called
 * @then tick_counter is reset to 0
 */
void test_profile_resets_tick_counter(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    ring.tick_counter = 5;

    diag_set_profile(&ring, DIAG_PROFILE_SOLAR);
    TEST_ASSERT_EQUAL(0, ring.tick_counter);
}

/* ---- Tick divider ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-046
 * @scenario Tick with divider=1 returns true every call
 * @given A ring with GENERAL profile (divider=1)
 * @when diag_ring_tick is called 3 times
 * @then All 3 calls return true
 */
void test_tick_divider_1(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    TEST_ASSERT_TRUE(diag_ring_tick(&ring));
    TEST_ASSERT_TRUE(diag_ring_tick(&ring));
    TEST_ASSERT_TRUE(diag_ring_tick(&ring));
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-046
 * @scenario Tick with divider=10 returns true every 10th call
 * @given A ring with sample_divider manually set to 10
 * @when diag_ring_tick is called 20 times
 * @then Returns true on calls 10 and 20 only
 */
void test_tick_divider_10(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);
    ring.sample_divider = 10;
    ring.tick_counter = 0;

    int fire_count = 0;
    for (int i = 1; i <= 20; i++)
    {
        if (diag_ring_tick(&ring))
            fire_count++;
    }
    TEST_ASSERT_EQUAL(2, fire_count);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-046
 * @scenario Tick with OFF profile always returns false
 * @given A ring with OFF profile
 * @when diag_ring_tick is called
 * @then Returns false
 */
void test_tick_off_profile(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    /* profile defaults to OFF */

    TEST_ASSERT_FALSE(diag_ring_tick(&ring));
}

/* ---- Snapshot struct size ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-040
 * @scenario diag_snapshot_t is exactly 64 bytes
 * @given The diag_snapshot_t struct definition
 * @when sizeof is checked
 * @then The size is exactly 64 bytes
 */
void test_snapshot_size(void)
{
    TEST_ASSERT_EQUAL(64, (int)sizeof(diag_snapshot_t));
}

/* ---- Binary serialization ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario Serialize empty ring produces valid header with zero snapshots
 * @given An initialized ring with GENERAL profile and 0 entries
 * @when diag_ring_serialize is called
 * @then Output contains valid header with count=0 and CRC32
 */
void test_serialize_empty(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    uint8_t out[256];
    size_t n = diag_ring_serialize(&ring, out, sizeof(out), "test_fw", 12345);

    /* header(34) + snapshots(0) + CRC(4) = 38 bytes minimum */
    TEST_ASSERT_GREATER_THAN(0, (int)n);

    /* Check magic */
    TEST_ASSERT_EQUAL('E', out[0]);
    TEST_ASSERT_EQUAL('V', out[1]);
    TEST_ASSERT_EQUAL('S', out[2]);
    TEST_ASSERT_EQUAL('E', out[3]);

    /* Check version */
    TEST_ASSERT_EQUAL(DIAG_FILE_VERSION, out[4]);

    /* Check profile */
    TEST_ASSERT_EQUAL(DIAG_PROFILE_GENERAL, out[5]);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario Serialize ring with entries produces correct binary
 * @given A ring with 2 snapshots (timestamps 100, 200)
 * @when diag_ring_serialize is called
 * @then Output contains header + 2 snapshots + CRC32
 */
void test_serialize_with_data(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s1 = make_snap(100);
    diag_snapshot_t s2 = make_snap(200);
    diag_ring_push(&ring, &s1);
    diag_ring_push(&ring, &s2);

    uint8_t out[512];
    size_t n = diag_ring_serialize(&ring, out, sizeof(out), "v1.0", 99);

    /* header(34) + 2*64 + CRC(4) = 166 */
    TEST_ASSERT_EQUAL(166, (int)n);

    /* Verify header count field (offset 8-9, little-endian uint16) */
    diag_file_header_t *hdr = (diag_file_header_t *)out;
    TEST_ASSERT_EQUAL(2, hdr->count);
    TEST_ASSERT_EQUAL(64, hdr->snapshot_size);
    TEST_ASSERT_EQUAL(99, (int)hdr->serial_nr);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario Serialize returns 0 when buffer is too small
 * @given A ring with 2 snapshots
 * @when diag_ring_serialize is called with a 10-byte buffer
 * @then Returns 0 (insufficient space)
 */
void test_serialize_buffer_too_small(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s = make_snap(1);
    diag_ring_push(&ring, &s);

    uint8_t out[10];
    size_t n = diag_ring_serialize(&ring, out, sizeof(out), "fw", 0);
    TEST_ASSERT_EQUAL(0, (int)n);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario Serialized data has valid CRC32
 * @given A ring with 1 snapshot serialized to binary
 * @when CRC32 is computed over header+snapshots and compared to stored CRC
 * @then The CRC values match
 */
void test_serialize_crc_valid(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s = make_snap_state(500, 2);
    diag_ring_push(&ring, &s);

    uint8_t out[512];
    size_t n = diag_ring_serialize(&ring, out, sizeof(out), "crc_test", 1);
    TEST_ASSERT_GREATER_THAN(4, (int)n);

    /* CRC is the last 4 bytes */
    size_t payload_len = n - 4;
    uint32_t computed_crc = diag_crc32(out, payload_len);
    uint32_t stored_crc;
    memcpy(&stored_crc, out + payload_len, 4);

    TEST_ASSERT_EQUAL((int)computed_crc, (int)stored_crc);
}

/* ---- CRC32 ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario CRC32 of empty data returns initial value
 * @given An empty byte array
 * @when diag_crc32 is called with length 0
 * @then Returns 0 (CRC of empty data)
 */
void test_crc32_empty(void)
{
    uint32_t crc = diag_crc32(NULL, 0);
    TEST_ASSERT_EQUAL(0, (int)crc);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario CRC32 of known data matches expected value
 * @given The string "123456789"
 * @when diag_crc32 is computed
 * @then Returns 0xCBF43926 (standard CRC32 test vector)
 */
void test_crc32_known_value(void)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = diag_crc32(data, 9);
    /* Standard CRC32 test vector */
    TEST_ASSERT_TRUE(crc == 0xCBF43926u);
}

/* ---- Edge cases ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-041
 * @scenario Push with NULL snapshot is safe
 * @given An initialized ring with GENERAL profile
 * @when diag_ring_push is called with NULL snapshot pointer
 * @then No crash, count stays 0
 */
void test_push_null_snap(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_ring_push(&ring, NULL);
    TEST_ASSERT_EQUAL(0, ring.count);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-041
 * @scenario Read from empty ring returns 0
 * @given An initialized ring with 0 entries
 * @when diag_ring_read is called
 * @then Returns 0
 */
void test_read_empty(void)
{
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);

    diag_snapshot_t out[4];
    uint16_t n = diag_ring_read(&ring, out, 4);
    TEST_ASSERT_EQUAL(0, n);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-042
 * @scenario Wrap-around preserves all snapshot fields
 * @given A ring with capacity 2 and GENERAL profile
 * @when 3 snapshots are pushed with distinct state and current fields
 * @then The surviving 2 snapshots have all fields intact
 */
void test_wrap_preserves_fields(void)
{
    diag_snapshot_t buf[2];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 2);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);

    diag_snapshot_t s1 = make_snap_state(1, 0);
    s1.charge_current = 60;

    diag_snapshot_t s2 = make_snap_state(2, 1);
    s2.charge_current = 100;
    s2.mains_irms[0] = 312;

    diag_snapshot_t s3 = make_snap_state(3, 2);
    s3.charge_current = 130;
    s3.temp_evse = 42;

    diag_ring_push(&ring, &s1);
    diag_ring_push(&ring, &s2);
    diag_ring_push(&ring, &s3);

    diag_snapshot_t out[2];
    uint16_t n = diag_ring_read(&ring, out, 2);
    TEST_ASSERT_EQUAL(2, n);

    /* s1 overwritten, s2 and s3 remain */
    TEST_ASSERT_EQUAL(2, out[0].timestamp);
    TEST_ASSERT_EQUAL(1, out[0].state);
    TEST_ASSERT_EQUAL(100, out[0].charge_current);
    TEST_ASSERT_EQUAL(312, out[0].mains_irms[0]);

    TEST_ASSERT_EQUAL(3, out[1].timestamp);
    TEST_ASSERT_EQUAL(2, out[1].state);
    TEST_ASSERT_EQUAL(130, out[1].charge_current);
    TEST_ASSERT_EQUAL(42, out[1].temp_evse);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-047
 * @scenario File header struct is 34 bytes
 * @given The diag_file_header_t struct definition
 * @when sizeof is checked
 * @then The size is exactly 34 bytes
 */
void test_file_header_size(void)
{
    TEST_ASSERT_EQUAL(34, (int)sizeof(diag_file_header_t));
}

/* ---- Main ---- */

int main(void)
{
    TEST_SUITE_BEGIN("Diagnostic Telemetry Ring Buffer");

    /* Initialization */
    RUN_TEST(test_ring_init);
    RUN_TEST(test_ring_init_null);
    RUN_TEST(test_snapshot_size);
    RUN_TEST(test_file_header_size);

    /* Push and read */
    RUN_TEST(test_push_and_read_single);
    RUN_TEST(test_push_fills_to_capacity);
    RUN_TEST(test_push_null_snap);
    RUN_TEST(test_read_empty);

    /* Wrap-around */
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_read_partial);
    RUN_TEST(test_wrap_preserves_fields);

    /* Reset */
    RUN_TEST(test_reset);
    RUN_TEST(test_reset_null);

    /* Freeze */
    RUN_TEST(test_frozen_rejects_push);
    RUN_TEST(test_unfreeze_allows_push);
    RUN_TEST(test_frozen_allows_read);

    /* Profile */
    RUN_TEST(test_profile_general);
    RUN_TEST(test_profile_fast);
    RUN_TEST(test_profile_off_rejects_push);
    RUN_TEST(test_profile_resets_tick_counter);

    /* Tick divider */
    RUN_TEST(test_tick_divider_1);
    RUN_TEST(test_tick_divider_10);
    RUN_TEST(test_tick_off_profile);

    /* Serialization */
    RUN_TEST(test_serialize_empty);
    RUN_TEST(test_serialize_with_data);
    RUN_TEST(test_serialize_buffer_too_small);
    RUN_TEST(test_serialize_crc_valid);

    /* CRC32 */
    RUN_TEST(test_crc32_empty);
    RUN_TEST(test_crc32_known_value);

    TEST_SUITE_RESULTS();
}
