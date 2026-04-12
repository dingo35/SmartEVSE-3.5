/*
 * test_ocpp_resilience.c — silent OCPP session loss detection
 *
 * Tests the pure C decision function ocpp_silence_decide() that schedules
 * heartbeat probes and triggers WebSocket reconnects when the OCPP backend
 * stops responding at the application layer (even if the WS transport stays
 * up via ping/pong).
 *
 * Background: integration of upstream commit ecd088b ("OCPP: recover from
 * silent OCPP session loss"). The pure decision is extracted from esp32.cpp
 * so the timing logic can be exhaustively tested without MicroOcpp or millis().
 */

#include "test_framework.h"
#include "ocpp_logic.h"

/* ---- Disconnected transport ---- */

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-100
 * @scenario No action while WebSocket is disconnected
 * @given ws_connected is false, all timers stale
 * @when ocpp_silence_decide is called
 * @then Returns NO_ACTION because the WS layer handles reconnection itself
 */
void test_silence_no_action_when_disconnected(void) {
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   false,
        /*now_ms*/         1000000UL,
        /*last_response*/  0UL,
        /*last_probe*/     0UL);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_NO_ACTION, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-100
 * @scenario Disconnected transport ignores stale response timestamp
 * @given ws_connected is false, last_response is 10 minutes ago
 * @when ocpp_silence_decide is called
 * @then Returns NO_ACTION — disconnected transport short-circuits everything
 */
void test_silence_disconnected_ignores_stale_response(void) {
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   false,
        /*now_ms*/         1000000UL,
        /*last_response*/  1000000UL - 600000UL,  /* 10 min ago */
        /*last_probe*/     1000000UL);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_NO_ACTION, a);
}

/* ---- Probe scheduling ---- */

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-101
 * @scenario First probe fires when probe interval has elapsed since boot
 * @given ws_connected, last_response is 1 second ago, last_probe is 0
 *        (cold-start: no probe sent yet, now_ms == OCPP_PROBE_INTERVAL_MS)
 * @when ocpp_silence_decide is called
 * @then Returns SEND_PROBE because (now - last_probe) >= probe interval
 */
void test_silence_first_probe_at_interval(void) {
    /* now_ms exactly == probe interval; last_probe = 0 */
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         OCPP_PROBE_INTERVAL_MS,
        /*last_response*/  OCPP_PROBE_INTERVAL_MS - 1000UL,
        /*last_probe*/     0UL);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_SEND_PROBE, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-101
 * @scenario No probe fires before the interval elapses
 * @given ws_connected, last_probe was 1 second ago, response fresh
 * @when ocpp_silence_decide is called
 * @then Returns NO_ACTION — too soon to probe again
 */
void test_silence_no_probe_before_interval(void) {
    unsigned long now = 1000000UL;
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         now,
        /*last_response*/  now - 500UL,
        /*last_probe*/     now - 1000UL);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_NO_ACTION, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-101
 * @scenario Probe interval boundary is inclusive
 * @given ws_connected, last_probe was exactly OCPP_PROBE_INTERVAL_MS ago
 * @when ocpp_silence_decide is called
 * @then Returns SEND_PROBE — boundary value triggers a new probe
 */
void test_silence_probe_at_boundary(void) {
    unsigned long now = 1000000UL;
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         now,
        /*last_response*/  now - 100UL,
        /*last_probe*/     now - OCPP_PROBE_INTERVAL_MS);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_SEND_PROBE, a);
}

/* ---- Force reconnect ---- */

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-102
 * @scenario Force reconnect when backend has been silent past timeout
 * @given ws_connected, last_response is OCPP_SILENCE_TIMEOUT_MS+1 ago
 * @when ocpp_silence_decide is called
 * @then Returns FORCE_RECONNECT (priority over probe)
 */
void test_silence_force_reconnect_after_timeout(void) {
    unsigned long now = 10UL * 60UL * 1000UL;  /* 10 min */
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         now,
        /*last_response*/  now - (OCPP_SILENCE_TIMEOUT_MS + 1UL),
        /*last_probe*/     now - 1000UL);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_FORCE_RECONNECT, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-102
 * @scenario Reconnect priority — probe interval elapsed AND silence timeout exceeded
 * @given ws_connected, both probe interval and silence timeout exceeded
 * @when ocpp_silence_decide is called
 * @then Returns FORCE_RECONNECT, not SEND_PROBE — reconnect takes priority
 */
void test_silence_reconnect_priority_over_probe(void) {
    unsigned long now = 10UL * 60UL * 1000UL;
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         now,
        /*last_response*/  now - OCPP_SILENCE_TIMEOUT_MS,  /* exactly at timeout */
        /*last_probe*/     now - OCPP_PROBE_INTERVAL_MS);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_FORCE_RECONNECT, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-103
 * @scenario Cold-boot guard — last_response_ms == 0 must not force reconnect
 * @given ws_connected, last_response is 0 (uninitialized), now is far in
 *        the future
 * @when ocpp_silence_decide is called
 * @then Returns SEND_PROBE (probe is fine to send) but NEVER FORCE_RECONNECT,
 *       because the 0-guard prevents a stale-init reconnect storm
 */
void test_silence_zero_response_does_not_force_reconnect(void) {
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         99999999UL,
        /*last_response*/  0UL,
        /*last_probe*/     99999999UL - OCPP_PROBE_INTERVAL_MS);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_SEND_PROBE, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-103
 * @scenario Cold-boot guard with no probe due either
 * @given ws_connected, last_response is 0, last_probe is also recent
 * @when ocpp_silence_decide is called
 * @then Returns NO_ACTION — no reconnect (zero-guard) and no probe (interval not elapsed)
 */
void test_silence_zero_response_no_probe_due(void) {
    unsigned long now = 1000UL;
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         now,
        /*last_response*/  0UL,
        /*last_probe*/     now);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_NO_ACTION, a);
}

/*
 * @feature OCPP Silence Detection
 * @req REQ-OCPP-104
 * @scenario Healthy steady state — fresh response, recent probe
 * @given ws_connected, response 1s ago, probe 30s ago (less than interval)
 * @when ocpp_silence_decide is called
 * @then Returns NO_ACTION
 */
void test_silence_healthy_steady_state(void) {
    unsigned long now = 5UL * 60UL * 1000UL;
    ocpp_silence_action_t a = ocpp_silence_decide(
        /*ws_connected*/   true,
        /*now_ms*/         now,
        /*last_response*/  now - 1000UL,
        /*last_probe*/     now - 30000UL);
    TEST_ASSERT_EQUAL_INT(OCPP_SILENCE_NO_ACTION, a);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Silence Detection");

    RUN_TEST(test_silence_no_action_when_disconnected);
    RUN_TEST(test_silence_disconnected_ignores_stale_response);
    RUN_TEST(test_silence_first_probe_at_interval);
    RUN_TEST(test_silence_no_probe_before_interval);
    RUN_TEST(test_silence_probe_at_boundary);
    RUN_TEST(test_silence_force_reconnect_after_timeout);
    RUN_TEST(test_silence_reconnect_priority_over_probe);
    RUN_TEST(test_silence_zero_response_does_not_force_reconnect);
    RUN_TEST(test_silence_zero_response_no_probe_due);
    RUN_TEST(test_silence_healthy_steady_state);

    TEST_SUITE_RESULTS();
}
