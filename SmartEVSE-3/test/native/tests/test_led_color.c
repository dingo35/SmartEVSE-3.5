/*
 * test_led_color.c — Native tests for LED color computation logic
 * extracted from BlinkLed_singlerun() in main.cpp.
 */

#include "test_framework.h"
#include "led_color.h"
#include <string.h>

/* State/mode/error constants */
#define STATE_A             0
#define STATE_B             1
#define STATE_C             2
#define STATE_B1            9
#define STATE_MODEM_REQUEST 11
#define STATE_MODEM_WAIT    12
#define STATE_MODEM_DENIED  14
#define MODE_NORMAL         0
#define MODE_SMART          1
#define MODE_SOLAR          2
#define ACCESS_OFF          0
#define ACCESS_ON           1
#define CT_NOCOMM           2
#define TEMP_HIGH           4
#define EV_NOCOMM           8
#define RCM_TRIPPED        16
#define RCM_TEST           32

/* Default color presets matching firmware defaults */
static const uint8_t COLOR_OFF[3]    = {0, 0, 0};
static const uint8_t COLOR_NORMAL[3] = {0, 255, 0};
static const uint8_t COLOR_SMART[3]  = {0, 255, 0};
static const uint8_t COLOR_SOLAR[3]  = {255, 170, 0};
static const uint8_t COLOR_CUSTOM[3] = {0, 0, 255};

/* Helper to initialize a default state snapshot */
static led_state_t make_default_state(void) {
    led_state_t s;
    memset(&s, 0, sizeof(s));
    s.access_status = ACCESS_ON;
    s.state = STATE_A;
    s.mode = MODE_NORMAL;
    memcpy(s.color_off, COLOR_OFF, 3);
    memcpy(s.color_normal, COLOR_NORMAL, 3);
    memcpy(s.color_smart, COLOR_SMART, 3);
    memcpy(s.color_solar, COLOR_SOLAR, 3);
    memcpy(s.color_custom, COLOR_CUSTOM, 3);
    return s;
}

/* ================================================================
 * Error State Tests
 * ================================================================ */

/*
 * @feature LED Status Indication
 * @req REQ-LED-001
 * @scenario RCM tripped error produces red flashing pattern on ESP32
 * @given ErrorFlags has RCM_TRIPPED set on ESP32 platform
 * @when led_compute_color is called repeatedly
 * @then LED alternates between red and off
 */
void test_error_rcm_tripped_esp32(void) {
    led_state_t s = make_default_state();
    s.error_flags = RCM_TRIPPED;
    s.is_ch32 = false;
    led_context_t ctx = {0, 0};

    /* Call multiple times to see flashing */
    bool saw_red = false, saw_off = false;
    for (int i = 0; i < 20; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        TEST_ASSERT_EQUAL_INT(0, rgb.g);
        TEST_ASSERT_EQUAL_INT(0, rgb.b);
        if (rgb.r > 0) saw_red = true;
        else saw_off = true;
    }
    TEST_ASSERT_TRUE(saw_red);
    TEST_ASSERT_TRUE(saw_off);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-001
 * @scenario CT_NOCOMM error shows red flashing
 * @given ErrorFlags has CT_NOCOMM set
 * @when led_compute_color is called multiple times
 * @then LED flashes red
 */
void test_error_ct_nocomm(void) {
    led_state_t s = make_default_state();
    s.error_flags = CT_NOCOMM;
    led_context_t ctx = {0, 0};

    bool saw_red = false;
    for (int i = 0; i < 20; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        if (rgb.r > 0) saw_red = true;
        TEST_ASSERT_EQUAL_INT(0, rgb.g);
        TEST_ASSERT_EQUAL_INT(0, rgb.b);
    }
    TEST_ASSERT_TRUE(saw_red);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-001
 * @scenario TEMP_HIGH error shows red flashing
 * @given ErrorFlags has TEMP_HIGH set
 * @when led_compute_color is called
 * @then LED flashes red
 */
void test_error_temp_high(void) {
    led_state_t s = make_default_state();
    s.error_flags = TEMP_HIGH;
    led_context_t ctx = {0, 0};

    bool saw_red = false;
    for (int i = 0; i < 20; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        if (rgb.r > 0) saw_red = true;
    }
    TEST_ASSERT_TRUE(saw_red);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-001
 * @scenario CH32 RCM mismatch with no test counter shows error
 * @given CH32 platform with RCM_TRIPPED set but not RCM_TEST, counter=0
 * @when led_compute_color is called
 * @then LED shows red (error condition)
 */
void test_error_ch32_rcm_mismatch(void) {
    led_state_t s = make_default_state();
    s.is_ch32 = true;
    s.error_flags = RCM_TRIPPED; /* RCM_TRIPPED set, RCM_TEST not set */
    s.rcm_test_counter = 0;
    led_context_t ctx = {0, 0};

    bool saw_red = false;
    for (int i = 0; i < 20; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        if (rgb.r > 0) saw_red = true;
    }
    TEST_ASSERT_TRUE(saw_red);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-001
 * @scenario CH32 RCM test in progress does not show error flash
 * @given CH32 platform with RCM_TRIPPED set, counter > 0 (test running)
 * @when led_compute_color is called
 * @then LED does NOT show rapid red error flash (enters waiting blink instead)
 */
void test_no_error_ch32_rcm_test_active(void) {
    led_state_t s = make_default_state();
    s.is_ch32 = true;
    s.error_flags = RCM_TRIPPED;
    s.rcm_test_counter = 5; /* Test in progress */
    led_context_t ctx = {0, 0};

    /* Call enough times to verify it's slow blink, not rapid red flash */
    bool saw_rapid_red = false;
    for (int i = 0; i < 10; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        /* Rapid red flash increments count by 20; slow blink by 2 */
        /* After 10 calls at +2 each: count=20, still < 230, so all off */
        if (rgb.r > 0 && rgb.g == 0 && rgb.b == 0 && i < 5)
            saw_rapid_red = true;
    }
    /* Should NOT see rapid red flash pattern */
    TEST_ASSERT_FALSE(saw_rapid_red);
}

/* ================================================================
 * Access OFF Tests
 * ================================================================ */

/*
 * @feature LED Status Indication
 * @req REQ-LED-002
 * @scenario Access OFF shows off color
 * @given Access status is OFF, no custom button
 * @when led_compute_color is called
 * @then LED shows ColorOff values
 */
void test_access_off_default(void) {
    led_state_t s = make_default_state();
    s.access_status = ACCESS_OFF;
    s.custom_button = false;
    s.color_off[0] = 10;
    s.color_off[1] = 20;
    s.color_off[2] = 30;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(10, rgb.r);
    TEST_ASSERT_EQUAL_INT(20, rgb.g);
    TEST_ASSERT_EQUAL_INT(30, rgb.b);
}

/*
 * @feature LED Color Configuration
 * @req REQ-LED-004
 * @scenario Custom button active when access OFF shows custom color
 * @given Access OFF and CustomButton is true
 * @when led_compute_color is called
 * @then LED shows ColorCustom values
 */
void test_access_off_custom_button(void) {
    led_state_t s = make_default_state();
    s.access_status = ACCESS_OFF;
    s.custom_button = true;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(COLOR_CUSTOM[0], rgb.r);
    TEST_ASSERT_EQUAL_INT(COLOR_CUSTOM[1], rgb.g);
    TEST_ASSERT_EQUAL_INT(COLOR_CUSTOM[2], rgb.b);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-002
 * @scenario MODEM_DENIED state shows off color
 * @given State is STATE_MODEM_DENIED, access ON
 * @when led_compute_color is called
 * @then LED shows ColorOff values (same as access OFF)
 */
void test_modem_denied_shows_off(void) {
    led_state_t s = make_default_state();
    s.state = STATE_MODEM_DENIED;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(COLOR_OFF[0], rgb.r);
    TEST_ASSERT_EQUAL_INT(COLOR_OFF[1], rgb.g);
    TEST_ASSERT_EQUAL_INT(COLOR_OFF[2], rgb.b);
}

/* ================================================================
 * Waiting / ChargeDelay Tests
 * ================================================================ */

/*
 * @feature LED Status Indication
 * @req REQ-LED-003
 * @scenario Solar mode with charge delay shows slow blink
 * @given Solar mode, ChargeDelay > 0, no errors
 * @when led_compute_color is called repeatedly
 * @then LED blinks with solar color (orange)
 */
void test_waiting_solar_blink(void) {
    led_state_t s = make_default_state();
    s.mode = MODE_SOLAR;
    s.charge_delay = 10;
    led_context_t ctx = {0, 0};

    bool saw_on = false, saw_off = false;
    for (int i = 0; i < 200; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        if (rgb.r > 0 || rgb.g > 0 || rgb.b > 0) saw_on = true;
        else saw_off = true;
    }
    TEST_ASSERT_TRUE(saw_on);
    TEST_ASSERT_TRUE(saw_off);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-003
 * @scenario Smart mode waiting shows smart color blink
 * @given Smart mode, ChargeDelay > 0
 * @when led_compute_color is called when LED is on
 * @then Color matches ColorSmart
 */
void test_waiting_smart_color(void) {
    led_state_t s = make_default_state();
    s.mode = MODE_SMART;
    s.charge_delay = 5;
    /* Start counter just before the threshold so LED turns on */
    led_context_t ctx = {229, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    /* After count increments by 2: 229+2=231 > 230, so LED on */
    /* Green channel: 255 * 255 / 255 = 255 */
    TEST_ASSERT_EQUAL_INT(0, rgb.r);
    TEST_ASSERT_TRUE(rgb.g > 0);
    TEST_ASSERT_EQUAL_INT(0, rgb.b);
}

/*
 * @feature LED Color Configuration
 * @req REQ-LED-004
 * @scenario Custom button waiting shows custom color
 * @given Custom button active, ChargeDelay > 0
 * @when led_compute_color is called when LED is on
 * @then Color matches ColorCustom
 */
void test_waiting_custom_button(void) {
    led_state_t s = make_default_state();
    s.mode = MODE_NORMAL;
    s.charge_delay = 5;
    s.custom_button = true;
    led_context_t ctx = {229, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    /* After count 229+2=231>230, LED on with custom color (blue) */
    TEST_ASSERT_EQUAL_INT(0, rgb.r);
    TEST_ASSERT_EQUAL_INT(0, rgb.g);
    TEST_ASSERT_TRUE(rgb.b > 0);
}

/* ================================================================
 * Charging State Tests
 * ================================================================ */

/*
 * @feature LED Status Indication
 * @req REQ-LED-005
 * @scenario State A shows dimmed LED
 * @given State A, Normal mode, no errors
 * @when led_compute_color is called
 * @then LED shows dimmed green (STATE_A_LED_BRIGHTNESS)
 */
void test_state_a_dimmed(void) {
    led_state_t s = make_default_state();
    s.state = STATE_A;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    /* Normal mode: green, brightness = 40 */
    TEST_ASSERT_EQUAL_INT(0, rgb.r);
    TEST_ASSERT_EQUAL_INT(40 * 255 / 255, rgb.g); /* 40 */
    TEST_ASSERT_EQUAL_INT(0, rgb.b);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-005
 * @scenario State B shows full brightness LED
 * @given State B, Normal mode, no errors
 * @when led_compute_color is called
 * @then LED shows full brightness green
 */
void test_state_b_full_brightness(void) {
    led_state_t s = make_default_state();
    s.state = STATE_B;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(0, rgb.r);
    TEST_ASSERT_EQUAL_INT(255, rgb.g);
    TEST_ASSERT_EQUAL_INT(0, rgb.b);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-005
 * @scenario State B1 shows full brightness (same as B)
 * @given State B1, Normal mode
 * @when led_compute_color is called
 * @then LED shows full brightness green
 */
void test_state_b1_full_brightness(void) {
    led_state_t s = make_default_state();
    s.state = STATE_B1;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(0, rgb.r);
    TEST_ASSERT_EQUAL_INT(255, rgb.g);
    TEST_ASSERT_EQUAL_INT(0, rgb.b);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-005
 * @scenario State B sets led_count to 128 for smooth C transition
 * @given State B entered
 * @when led_compute_color is called
 * @then led_count is set to 128
 */
void test_state_b_sets_count_128(void) {
    led_state_t s = make_default_state();
    s.state = STATE_B;
    led_context_t ctx = {0, 0};

    led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(128, ctx.led_count);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-006
 * @scenario State C shows breathing animation
 * @given State C, Normal mode
 * @when led_compute_color is called multiple times
 * @then LED brightness varies (breathing effect)
 */
void test_state_c_breathing(void) {
    led_state_t s = make_default_state();
    s.state = STATE_C;
    led_context_t ctx = {128, 0}; /* Start at peak from state B */

    uint8_t min_g = 255, max_g = 0;
    for (int i = 0; i < 128; i++) {
        led_rgb_t rgb = led_compute_color(&s, &ctx);
        if (rgb.g < min_g) min_g = rgb.g;
        if (rgb.g > max_g) max_g = rgb.g;
    }
    /* Breathing should produce varying brightness */
    TEST_ASSERT_TRUE(max_g > min_g);
}

/*
 * @feature LED Status Indication
 * @req REQ-LED-006
 * @scenario State C Solar mode has slower breathing
 * @given State C, Solar mode
 * @when led_compute_color is called
 * @then led_count increments by 1 per call (vs 2 for other modes)
 */
void test_state_c_solar_slower(void) {
    led_state_t s = make_default_state();
    s.state = STATE_C;
    s.mode = MODE_SOLAR;
    led_context_t ctx = {0, 0};

    led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.led_count); /* Increments by 1 in solar */

    led_context_t ctx2 = {0, 0};
    s.mode = MODE_NORMAL;
    led_compute_color(&s, &ctx2);
    TEST_ASSERT_EQUAL_INT(2, ctx2.led_count); /* Increments by 2 normally */
}

/* ================================================================
 * Color Mode Tests
 * ================================================================ */

/*
 * @feature LED Color Configuration
 * @req REQ-LED-004
 * @scenario Solar mode State A shows solar orange (dimmed)
 * @given State A, Solar mode
 * @when led_compute_color is called
 * @then LED shows orange tint at STATE_A brightness
 */
void test_state_a_solar_color(void) {
    led_state_t s = make_default_state();
    s.state = STATE_A;
    s.mode = MODE_SOLAR;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    /* Solar: (255,170,0) at brightness 40 */
    TEST_ASSERT_EQUAL_INT(40 * 255 / 255, rgb.r); /* 40 */
    TEST_ASSERT_EQUAL_INT(40 * 170 / 255, rgb.g); /* 26 */
    TEST_ASSERT_EQUAL_INT(0, rgb.b);
}

/*
 * @feature LED Color Configuration
 * @req REQ-LED-004
 * @scenario Custom button overrides mode color in State B
 * @given State B, Normal mode, CustomButton true
 * @when led_compute_color is called
 * @then LED shows custom blue at full brightness
 */
void test_state_b_custom_override(void) {
    led_state_t s = make_default_state();
    s.state = STATE_B;
    s.custom_button = true;
    led_context_t ctx = {0, 0};

    led_rgb_t rgb = led_compute_color(&s, &ctx);
    TEST_ASSERT_EQUAL_INT(0, rgb.r);
    TEST_ASSERT_EQUAL_INT(0, rgb.g);
    TEST_ASSERT_EQUAL_INT(255, rgb.b);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    TEST_SUITE_BEGIN("LED Color Computation");

    /* Error states */
    RUN_TEST(test_error_rcm_tripped_esp32);
    RUN_TEST(test_error_ct_nocomm);
    RUN_TEST(test_error_temp_high);
    RUN_TEST(test_error_ch32_rcm_mismatch);
    RUN_TEST(test_no_error_ch32_rcm_test_active);

    /* Access OFF */
    RUN_TEST(test_access_off_default);
    RUN_TEST(test_access_off_custom_button);
    RUN_TEST(test_modem_denied_shows_off);

    /* Waiting / ChargeDelay */
    RUN_TEST(test_waiting_solar_blink);
    RUN_TEST(test_waiting_smart_color);
    RUN_TEST(test_waiting_custom_button);

    /* Charging states */
    RUN_TEST(test_state_a_dimmed);
    RUN_TEST(test_state_b_full_brightness);
    RUN_TEST(test_state_b1_full_brightness);
    RUN_TEST(test_state_b_sets_count_128);
    RUN_TEST(test_state_c_breathing);
    RUN_TEST(test_state_c_solar_slower);

    /* Color modes */
    RUN_TEST(test_state_a_solar_color);
    RUN_TEST(test_state_b_custom_override);

    TEST_SUITE_RESULTS();
}
