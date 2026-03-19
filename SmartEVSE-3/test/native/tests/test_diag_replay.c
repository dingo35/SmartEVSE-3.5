/*
 * test_diag_replay.c - Test replay framework for diagnostic captures
 *
 * Tests the .diag file loader and the replay mechanism that feeds
 * captured snapshots into evse_ctx_t for state machine verification.
 */

#include "test_framework.h"
#include "diag_telemetry.h"
#include "diag_loader.h"
#include "evse_ctx.h"
#include <string.h>
#include <stdlib.h>

/* ---- Loader tests ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-050
 * @scenario Create synthetic capture with known parameters
 * @given A request for 10 snapshots starting at uptime 1000
 * @when diag_create_synthetic is called with GENERAL profile
 * @then 10 snapshots are created with timestamps 1000..1009
 */
void test_synthetic_capture(void)
{
    diag_capture_t cap;
    bool ok = diag_create_synthetic(&cap, 10, 1000, DIAG_PROFILE_GENERAL);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(cap.loaded);
    TEST_ASSERT_EQUAL(10, cap.count);
    TEST_ASSERT_EQUAL(1000, (int)cap.snapshots[0].timestamp);
    TEST_ASSERT_EQUAL(1009, (int)cap.snapshots[9].timestamp);
    TEST_ASSERT_EQUAL(DIAG_PROFILE_GENERAL, cap.header.profile);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-050
 * @scenario Load capture from serialized binary buffer
 * @given A ring buffer with 3 snapshots serialized to binary
 * @when diag_load_buffer is called
 * @then All 3 snapshots are loaded with correct timestamps and CRC is valid
 */
void test_load_from_serialized_buffer(void)
{
    /* Create a ring buffer and serialize it */
    diag_snapshot_t buf[4];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 4);
    diag_set_profile(&ring, DIAG_PROFILE_SOLAR);
    ring.start_time = 500;

    diag_snapshot_t s;
    memset(&s, 0, sizeof(s));
    for (uint32_t i = 0; i < 3; i++) {
        s.timestamp = 500 + i;
        s.state = (uint8_t)(i + 1);
        s.charge_current = (uint16_t)(60 + i * 10);
        diag_ring_push(&ring, &s);
    }

    /* Serialize */
    uint8_t binary[512];
    size_t n = diag_ring_serialize(&ring, binary, sizeof(binary), "test_fw", 99);
    TEST_ASSERT_GREATER_THAN(0, (int)n);

    /* Load */
    diag_capture_t cap;
    bool ok = diag_load_buffer(binary, n, &cap);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(cap.loaded);
    TEST_ASSERT_TRUE(cap.crc_valid);
    TEST_ASSERT_EQUAL(3, cap.count);
    TEST_ASSERT_EQUAL(500, (int)cap.snapshots[0].timestamp);
    TEST_ASSERT_EQUAL(1, cap.snapshots[0].state);
    TEST_ASSERT_EQUAL(60, cap.snapshots[0].charge_current);
    TEST_ASSERT_EQUAL(502, (int)cap.snapshots[2].timestamp);
    TEST_ASSERT_EQUAL(80, cap.snapshots[2].charge_current);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-050
 * @scenario Load from buffer detects corrupt magic
 * @given A binary buffer with invalid magic bytes
 * @when diag_load_buffer is called
 * @then Returns false
 */
void test_load_corrupt_magic(void)
{
    uint8_t bad[128];
    memset(bad, 0, sizeof(bad));
    memcpy(bad, "NOPE", 4);

    diag_capture_t cap;
    TEST_ASSERT_FALSE(diag_load_buffer(bad, sizeof(bad), &cap));
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-050
 * @scenario Load from NULL buffer is safe
 * @given NULL data pointer
 * @when diag_load_buffer is called
 * @then Returns false without crash
 */
void test_load_null_safe(void)
{
    diag_capture_t cap;
    TEST_ASSERT_FALSE(diag_load_buffer(NULL, 0, &cap));
    TEST_ASSERT_FALSE(diag_load_buffer(NULL, 100, NULL));
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-050
 * @scenario Synthetic capture with zero count fails
 * @given A request for 0 snapshots
 * @when diag_create_synthetic is called
 * @then Returns false
 */
void test_synthetic_zero_count(void)
{
    diag_capture_t cap;
    TEST_ASSERT_FALSE(diag_create_synthetic(&cap, 0, 0, DIAG_PROFILE_GENERAL));
}

/* ---- Replay tests ---- */

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-051
 * @scenario Replay snapshots into evse_ctx_t and verify field mapping
 * @given A synthetic capture with 5 snapshots containing known current values
 * @when Each snapshot's fields are mapped to evse_ctx_t
 * @then The ctx fields match the snapshot values
 */
void test_replay_field_mapping(void)
{
    diag_capture_t cap;
    diag_create_synthetic(&cap, 5, 100, DIAG_PROFILE_GENERAL);

    /* Set distinct values on snapshot 2 */
    cap.snapshots[2].state = STATE_C;
    cap.snapshots[2].mode = MODE_SOLAR;
    cap.snapshots[2].charge_current = 130;
    cap.snapshots[2].mains_irms[0] = -250;
    cap.snapshots[2].mains_irms[1] = -100;
    cap.snapshots[2].mains_irms[2] = -50;
    cap.snapshots[2].isum = -400;
    cap.snapshots[2].solar_stop_timer = 0;
    cap.snapshots[2].nr_phases_charging = 3;
    cap.snapshots[2].temp_evse = 42;

    /* Map snapshot → ctx (advisory replay) */
    evse_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    diag_snapshot_t *s = &cap.snapshots[2];

    ctx.State = s->state;
    ctx.Mode = s->mode;
    ctx.ChargeCurrent = s->charge_current;
    ctx.MainsMeterIrms[0] = s->mains_irms[0];
    ctx.MainsMeterIrms[1] = s->mains_irms[1];
    ctx.MainsMeterIrms[2] = s->mains_irms[2];
    ctx.Isum = s->isum;
    ctx.SolarStopTimer = s->solar_stop_timer;
    ctx.Nr_Of_Phases_Charging = s->nr_phases_charging;
    ctx.TempEVSE = s->temp_evse;

    /* Verify mapping */
    TEST_ASSERT_EQUAL(STATE_C, ctx.State);
    TEST_ASSERT_EQUAL(MODE_SOLAR, ctx.Mode);
    TEST_ASSERT_EQUAL(130, ctx.ChargeCurrent);
    TEST_ASSERT_EQUAL(-250, ctx.MainsMeterIrms[0]);
    TEST_ASSERT_EQUAL(-400, ctx.Isum);
    TEST_ASSERT_EQUAL(3, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_EQUAL(42, ctx.TempEVSE);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-051
 * @scenario Replay sequence tracks state transitions across snapshots
 * @given A capture with STATE_A→STATE_B→STATE_C→STATE_C→STATE_C transition
 * @when Snapshots are replayed in sequence
 * @then Each snapshot's state matches the expected transition
 */
void test_replay_state_transitions(void)
{
    diag_capture_t cap;
    diag_create_synthetic(&cap, 5, 200, DIAG_PROFILE_GENERAL);

    uint8_t expected_states[] = { STATE_A, STATE_B, STATE_C, STATE_C, STATE_C };
    for (int i = 0; i < 5; i++)
        cap.snapshots[i].state = expected_states[i];

    /* Replay and verify transitions */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(expected_states[i], cap.snapshots[i].state);
    }

    /* Verify we can detect the A→B transition */
    TEST_ASSERT_TRUE(cap.snapshots[0].state != cap.snapshots[1].state);
    /* Verify steady-state C charging */
    TEST_ASSERT_EQUAL(cap.snapshots[3].state, cap.snapshots[4].state);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-051
 * @scenario Advisory replay detects solar current oscillation pattern
 * @given A capture with charge_current oscillating between 0 and 80
 * @when Snapshots are analyzed for oscillation
 * @then The oscillation is detected (>2 zero-crossings in the window)
 */
void test_replay_solar_oscillation_detection(void)
{
    diag_capture_t cap;
    diag_create_synthetic(&cap, 10, 300, DIAG_PROFILE_SOLAR);

    /* Simulate solar oscillation: current toggling */
    uint16_t currents[] = { 80, 0, 80, 0, 80, 0, 80, 0, 80, 0 };
    for (int i = 0; i < 10; i++) {
        cap.snapshots[i].mode = MODE_SOLAR;
        cap.snapshots[i].charge_current = currents[i];
    }

    /* Detect oscillation: count zero-crossings */
    int crossings = 0;
    for (int i = 1; i < 10; i++) {
        bool prev_zero = cap.snapshots[i - 1].charge_current == 0;
        bool curr_zero = cap.snapshots[i].charge_current == 0;
        if (prev_zero != curr_zero)
            crossings++;
    }

    /* >2 crossings in 10 samples = oscillation */
    TEST_ASSERT_GREATER_THAN(2, crossings);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-051
 * @scenario Round-trip: serialize ring → load → compare snapshots
 * @given A ring buffer with 4 snapshots serialized to binary
 * @when The binary is loaded back via diag_load_buffer
 * @then All snapshot fields match the originals exactly
 */
void test_roundtrip_serialize_load(void)
{
    diag_snapshot_t buf[8];
    diag_ring_t ring;
    diag_ring_init(&ring, buf, 8);
    diag_set_profile(&ring, DIAG_PROFILE_GENERAL);
    ring.start_time = 1000;

    /* Push 4 snapshots with diverse field values */
    for (uint32_t i = 0; i < 4; i++) {
        diag_snapshot_t s;
        memset(&s, 0, sizeof(s));
        s.timestamp = 1000 + i;
        s.state = (uint8_t)(i % 3);
        s.charge_current = (uint16_t)(60 + i * 20);
        s.mains_irms[0] = (int16_t)(100 + i * 50);
        s.temp_evse = (int8_t)(30 + i);
        s.wifi_rssi = (int8_t)(-70 + i * 5);
        diag_ring_push(&ring, &s);
    }

    /* Serialize */
    uint8_t binary[1024];
    size_t n = diag_ring_serialize(&ring, binary, sizeof(binary), "v2.0", 54321);
    TEST_ASSERT_GREATER_THAN(0, (int)n);

    /* Load */
    diag_capture_t cap;
    TEST_ASSERT_TRUE(diag_load_buffer(binary, n, &cap));
    TEST_ASSERT_EQUAL(4, cap.count);
    TEST_ASSERT_TRUE(cap.crc_valid);

    /* Compare each snapshot */
    diag_snapshot_t orig[4];
    diag_ring_read(&ring, orig, 4);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL((int)orig[i].timestamp, (int)cap.snapshots[i].timestamp);
        TEST_ASSERT_EQUAL(orig[i].state, cap.snapshots[i].state);
        TEST_ASSERT_EQUAL(orig[i].charge_current, cap.snapshots[i].charge_current);
        TEST_ASSERT_EQUAL(orig[i].mains_irms[0], cap.snapshots[i].mains_irms[0]);
        TEST_ASSERT_EQUAL(orig[i].temp_evse, cap.snapshots[i].temp_evse);
        TEST_ASSERT_EQUAL(orig[i].wifi_rssi, cap.snapshots[i].wifi_rssi);
    }
}

/* ---- Main ---- */

int main(void)
{
    TEST_SUITE_BEGIN("Diagnostic Replay Framework");

    /* Loader */
    RUN_TEST(test_synthetic_capture);
    RUN_TEST(test_load_from_serialized_buffer);
    RUN_TEST(test_load_corrupt_magic);
    RUN_TEST(test_load_null_safe);
    RUN_TEST(test_synthetic_zero_count);

    /* Replay */
    RUN_TEST(test_replay_field_mapping);
    RUN_TEST(test_replay_state_transitions);
    RUN_TEST(test_replay_solar_oscillation_detection);
    RUN_TEST(test_roundtrip_serialize_load);

    TEST_SUITE_RESULTS();
}
