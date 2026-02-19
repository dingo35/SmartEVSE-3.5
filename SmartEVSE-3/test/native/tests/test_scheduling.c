/*
 * test_scheduling.c - Priority-Based Power Scheduling
 *
 * Tests priority sorting, allocation under shortage, idle detection,
 * round-robin rotation, power increase reactivation, and regression
 * scenarios for the priority scheduling feature (Issue #316).
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* Helper: set up master with N EVSEs all in STATE_C */
static void setup_master_n(int n) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 1;
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 64;
    ctx.MaxMains = 50;
    ctx.ChargeCurrent = 320;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = NOT_PRESENT;
    ctx.phasesLastUpdateFlag = true;

    for (int i = 0; i < n && i < NR_EVSES; i++) {
        ctx.BalancedState[i] = STATE_C;
        ctx.BalancedMax[i] = 320;
        ctx.Balanced[i] = 100;
        ctx.Node[i].Online = 1;
        ctx.Node[i].IntTimer = 100;
    }
}

/* ================================================================
 * 1. Priority Sorting
 * ================================================================ */

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-100
 * @scenario MODBUS_ADDR strategy produces ascending address order
 * @given Master (LoadBl=1) with 4 EVSEs in STATE_C
 * @and PrioStrategy = PRIO_MODBUS_ADDR
 * @and ConnectedTime = {100, 50, 200, 150} (irrelevant for this strategy)
 * @when evse_sort_priority() is called
 * @then Priority[] = {0, 1, 2, 3}
 */
void test_sort_modbus_addr(void) {
    setup_master_n(4);
    ctx.PrioStrategy = PRIO_MODBUS_ADDR;
    ctx.ConnectedTime[0] = 100;
    ctx.ConnectedTime[1] = 50;
    ctx.ConnectedTime[2] = 200;
    ctx.ConnectedTime[3] = 150;

    evse_sort_priority(&ctx);

    TEST_ASSERT_EQUAL_INT(0, ctx.Priority[0]);
    TEST_ASSERT_EQUAL_INT(1, ctx.Priority[1]);
    TEST_ASSERT_EQUAL_INT(2, ctx.Priority[2]);
    TEST_ASSERT_EQUAL_INT(3, ctx.Priority[3]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-101
 * @scenario FIRST_CONNECTED strategy orders by earliest connection time
 * @given Master with 3 EVSEs in STATE_C
 * @and PrioStrategy = PRIO_FIRST_CONNECTED
 * @and ConnectedTime = {300, 100, 200} (EVSE[1] connected first)
 * @when evse_sort_priority() is called
 * @then Priority[] = {1, 2, 0, ...} (EVSE[1] first, EVSE[0] last)
 */
void test_sort_first_connected(void) {
    setup_master_n(3);
    ctx.PrioStrategy = PRIO_FIRST_CONNECTED;
    ctx.ConnectedTime[0] = 300;
    ctx.ConnectedTime[1] = 100;
    ctx.ConnectedTime[2] = 200;

    evse_sort_priority(&ctx);

    TEST_ASSERT_EQUAL_INT(1, ctx.Priority[0]);
    TEST_ASSERT_EQUAL_INT(2, ctx.Priority[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Priority[2]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-102
 * @scenario LAST_CONNECTED strategy orders by most recent connection time
 * @given Master with 3 EVSEs in STATE_C
 * @and PrioStrategy = PRIO_LAST_CONNECTED
 * @and ConnectedTime = {300, 100, 200} (EVSE[0] connected last)
 * @when evse_sort_priority() is called
 * @then Priority[] = {0, 2, 1, ...} (EVSE[0] first, EVSE[1] last)
 */
void test_sort_last_connected(void) {
    setup_master_n(3);
    ctx.PrioStrategy = PRIO_LAST_CONNECTED;
    ctx.ConnectedTime[0] = 300;
    ctx.ConnectedTime[1] = 100;
    ctx.ConnectedTime[2] = 200;

    evse_sort_priority(&ctx);

    TEST_ASSERT_EQUAL_INT(0, ctx.Priority[0]);
    TEST_ASSERT_EQUAL_INT(2, ctx.Priority[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx.Priority[2]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-103
 * @scenario Disconnected EVSEs are sorted to end regardless of strategy
 * @given Master with 4 EVSEs: [0]=STATE_C, [1]=STATE_A, [2]=STATE_C, [3]=STATE_A
 * @and PrioStrategy = PRIO_MODBUS_ADDR
 * @when evse_sort_priority() is called
 * @then Priority[] = {0, 2, 1, 3} (active EVSEs first, then disconnected)
 */
void test_sort_disconnected_to_end(void) {
    setup_master_n(4);
    ctx.BalancedState[1] = STATE_A;
    ctx.BalancedState[3] = STATE_A;
    ctx.PrioStrategy = PRIO_MODBUS_ADDR;

    evse_sort_priority(&ctx);

    TEST_ASSERT_EQUAL_INT(0, ctx.Priority[0]);
    TEST_ASSERT_EQUAL_INT(2, ctx.Priority[1]);
    /* Inactive EVSEs at the end (order between them is by address) */
    TEST_ASSERT_EQUAL_INT(1, ctx.Priority[2]);
    TEST_ASSERT_EQUAL_INT(3, ctx.Priority[3]);
}

/* ================================================================
 * 2. Priority Allocation (shortage path)
 * ================================================================ */

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-110
 * @scenario Insufficient power for 3 EVSEs: first 2 in priority get MinCurrent
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A, MaxCircuit=20A
 * @and PrioStrategy = PRIO_MODBUS_ADDR
 * @and Available power = 12A (enough for 2 * 6A, not 3 * 6A)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Balanced[0] >= 60 and Balanced[1] >= 60 and Balanced[2] == 0
 */
void test_shortage_first_two_get_current(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 20;  /* IsetBalanced = 200 - 0 = 200; need 3*60=180 but
                             we want shortage, so reduce further */
    ctx.MaxCircuit = 12;  /* IsetBalanced = 120; need 180 → shortage */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[2]);
    TEST_ASSERT_TRUE(ctx.BalancedError[2] & LESS_6A);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[1]);
    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[2]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-111
 * @scenario Power for only 1 EVSE: highest priority gets it all
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A
 * @and Available power = 8A (enough for 1 EVSE, not 2)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Balanced[0] == 80 and Balanced[1] == 0 and Balanced[2] == 0
 */
void test_shortage_one_evse_gets_all(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 8;   /* IsetBalanced = 80; need 180 → only enough for 1 */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(80, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[2]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[1]);
    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[2]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-112
 * @scenario Sufficient power: all EVSEs get current, no scheduling needed
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A
 * @and Available power = 30A (10A per EVSE, well above 3*6A=18A)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then All EVSEs get current, no LESS_6A errors, NoCurrent == 0
 */
void test_sufficient_power_no_scheduling(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 30;  /* IsetBalanced = 300; need 180 → plenty */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_THAN(0, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_THAN(0, ctx.Balanced[1]);
    TEST_ASSERT_GREATER_THAN(0, ctx.Balanced[2]);
    TEST_ASSERT_FALSE(ctx.BalancedError[0] & LESS_6A);
    TEST_ASSERT_FALSE(ctx.BalancedError[1] & LESS_6A);
    TEST_ASSERT_FALSE(ctx.BalancedError[2] & LESS_6A);
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-113
 * @scenario Surplus above MinCurrent distributed fairly among active EVSEs
 * @given Master with 2 EVSEs in STATE_C, MinCurrent=6A, BalancedMax={320,320}
 * @and Available power = 20A (after 2*6A=12A min, surplus = 8A)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Balanced[0] == 100 and Balanced[1] == 100 (10A each = 6A + 4A surplus)
 */
void test_surplus_distributed_fairly(void) {
    setup_master_n(2);
    ctx.MaxCircuit = 20;  /* IsetBalanced = 200; need 120 → shortage triggers
                             when only 2 EVSEs; 200 >= 120, so NO shortage */
    /* Actually 200 >= 2*6*10=120, so no shortage. Need shortage to test surplus. */
    /* With 3 EVSEs at MaxCircuit=20, IsetBalanced=200 < 3*60=180? No, 200 >= 180.
     * Use 2 EVSEs with MaxCircuit=10: IsetBalanced=100 < 2*60=120 → shortage.
     * available=100. EVSE[0] gets 60 (avail=40). EVSE[1] gets 0 (40<60). Not fair.
     * Use MaxCircuit=13: IsetBalanced=130 < 120? No, 130 >= 120 → no shortage.
     * For fair surplus test, no shortage is needed. Use sufficient power path. */
    ctx.MaxCircuit = 20;
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* No shortage: standard distribution 200/2 = 100 each */
    TEST_ASSERT_EQUAL_INT(100, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(100, ctx.Balanced[1]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-114
 * @scenario Standalone mode (LoadBl=0) does not use priority scheduling
 * @given Single EVSE (LoadBl=0) in STATE_C, MinCurrent=6A, Mode=SMART
 * @and Available power = 4A (below MinCurrent)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then NoCurrent increments (original behavior preserved), no ScheduleState changes
 */
void test_standalone_no_scheduling(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxMains = 18;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 180;  /* Baseload = 180 - 40 = 140 */
    ctx.ChargeCurrent = 160;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 40;
    ctx.phasesLastUpdateFlag = true;
    ctx.IsetBalanced = 40;  /* Below MinCurrent*10=60 */
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* Standalone hard shortage: IsetBalanced(60) > MaxMains*10 - Baseload(40),
     * so NoCurrent increments (original behavior preserved) */
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.NoCurrent);
    /* ScheduleState stays at init default (SCHED_INACTIVE) */
    TEST_ASSERT_EQUAL_INT(SCHED_INACTIVE, ctx.ScheduleState[0]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-115
 * @scenario Solar mode: paused EVSEs get NO_SUN error instead of LESS_6A
 * @given Master with 2 EVSEs in STATE_C, Mode=MODE_SOLAR
 * @and Available power = 5A (below 2*6A=12A)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Balanced[1] == 0 and BalancedError[1] has NO_SUN set
 */
void test_solar_paused_gets_no_sun(void) {
    setup_master_n(2);
    ctx.Mode = MODE_SOLAR;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 25;
    /* Set up so IsetBalanced < 2*60=120 in Solar mode.
     * Solar regulation: IsetBalanced += solar adjustments.
     * Set IsetBalanced directly to a low value and use mod=0. */
    ctx.MainsMeterImeasured = 200;
    ctx.Balanced[0] = 50;
    ctx.Balanced[1] = 50;
    ctx.EVMeterImeasured = 0;
    ctx.IsetBalanced = 50;  /* Will decrease via regulation */
    ctx.Isum = 200;         /* Importing power */
    ctx.ImportCurrent = 0;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.Node[0].IntTimer = 100;
    ctx.Node[1].IntTimer = 100;
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* EVSE[1] should be paused with NO_SUN (not LESS_6A) in solar mode */
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[1]);
    TEST_ASSERT_TRUE(ctx.BalancedError[1] & NO_SUN);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-116
 * @scenario Capped EVSE surplus redistributed to uncapped ones
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A
 * @and BalancedMax = {320, 80, 320} (EVSE[1] capped at 8A)
 * @and Available power = 24A (sufficient, no shortage)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Balanced[1] == 80 (capped) and Balanced[0] + Balanced[2] == 160
 */
void test_capped_surplus_redistribution(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 24;  /* IsetBalanced = 240; need 180 → no shortage */
    ctx.BalancedMax[1] = 80;
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* EVSE[1] capped at 80; remaining 160 split between [0] and [2] */
    TEST_ASSERT_EQUAL_INT(80, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0] + ctx.Balanced[2]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-117
 * @scenario Power exactly equals MinCurrent for 1 EVSE
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A
 * @and Available power = 6A (exactly 1 * MinCurrent)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Exactly 1 EVSE has Balanced >= 60, exactly 2 have Balanced == 0
 */
void test_exactly_one_mincurrent(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 6;   /* IsetBalanced = 60; need 180 → only enough for 1 */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;

    evse_calc_balanced_current(&ctx, 0);

    int with_current = 0;
    int without_current = 0;
    for (int i = 0; i < 3; i++) {
        if (ctx.Balanced[i] >= 60)
            with_current++;
        else if (ctx.Balanced[i] == 0)
            without_current++;
    }
    TEST_ASSERT_EQUAL_INT(1, with_current);
    TEST_ASSERT_EQUAL_INT(2, without_current);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-118
 * @scenario Zero available power pauses all EVSEs
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A
 * @and Available power = 0A (baseload exceeds MaxCircuit)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then All Balanced[] == 0, all paused, NoCurrent increments
 */
void test_zero_power_pauses_all(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 1;   /* IsetBalanced = 10; way below 180 */
    /* Even 10 < 60, so nobody gets MinCurrent */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 0);

    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[i]);
        TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[i]);
    }
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.NoCurrent);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-119
 * @scenario NoCurrent does NOT increment when priority scheduling pauses some EVSEs
 * @given Master with 3 EVSEs in STATE_C, MinCurrent=6A
 * @and Available power = 10A (enough for 1 EVSE but not all 3)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then NoCurrent == 0, EVSE[0] is charging
 */
void test_no_current_not_incremented_on_deliberate_pause(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 10;  /* IsetBalanced = 100; need 180 → shortage for 3, but 1 can charge */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
}

/* ================================================================
 * 3. Idle Detection
 * ================================================================ */

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-120
 * @scenario EVSE drawing <1A when IdleTimer expires gets paused
 * @given Master with 2 EVSEs in STATE_C, EVSE[0] active, EVSE[1] paused
 * @and IdleTimeout = 60, IdleTimer[0] = 59
 * @and EVSE[0] allocated but drawing <1A
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[0] paused, EVSE[1] activated with IdleTimer[1] = 0
 */
void test_idle_evse_paused_at_timeout(void) {
    setup_master_n(2);
    ctx.IdleTimeout = 60;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.IdleTimer[0] = 59;
    ctx.Balanced[0] = 60;
    ctx.Balanced[1] = 0;
    ctx.EVMeterImeasured = 5;  /* < IDLE_CURRENT_THRESHOLD (10 = 1.0A) */
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.IdleTimer[1]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-121
 * @scenario EVSE not paused before IdleTimeout expires (anti-flap)
 * @given EVSE[0] active with IdleTimer[0] = 30, IdleTimeout = 60
 * @and EVSE[0] drawing 0A (would be idle)
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[0] remains active
 */
void test_antiflap_not_paused_early(void) {
    setup_master_n(2);
    ctx.IdleTimeout = 60;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.IdleTimer[0] = 30;
    ctx.Balanced[0] = 60;
    ctx.EVMeterImeasured = 0;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(31, ctx.IdleTimer[0]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-122
 * @scenario EVSE drawing power when IdleTimer expires stays active
 * @given EVSE[0] active with IdleTimer[0] = 59, IdleTimeout = 60
 * @and EVSE[0] drawing 8A (measured current >= 1A)
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[0] stays active, RotationTimer starts if RotationInterval > 0
 */
void test_charging_evse_stays_active(void) {
    setup_master_n(2);
    ctx.IdleTimeout = 60;
    ctx.RotationInterval = 30;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.IdleTimer[0] = 59;
    ctx.Balanced[0] = 80;
    ctx.EVMeterImeasured = 80;  /* 8A, well above 1A threshold */
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;
    ctx.RotationTimer = 0;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    /* RotationTimer set to 1800 then decremented by 1 in same tick */
    TEST_ASSERT_EQUAL_INT(1799, ctx.RotationTimer);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-123
 * @scenario Full idle cycle: all EVSEs tried, recircle to first
 * @given 3 EVSEs in STATE_C, EVSE[2] active (last tried), EVSE[0] and [1] paused
 * @and EVSE[2] IdleTimer reaches IdleTimeout, still drawing 0A
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[2] paused, EVSE[0] reactivated (wraps around)
 */
void test_idle_cycle_wraps_around(void) {
    setup_master_n(3);
    ctx.IdleTimeout = 60;
    ctx.ScheduleState[0] = SCHED_PAUSED;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.ScheduleState[2] = SCHED_ACTIVE;
    ctx.IdleTimer[2] = 59;
    ctx.Balanced[2] = 60;
    ctx.EVMeterImeasured = 0;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;
    ctx.ConnectedTime[2] = 3;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[2]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx.IdleTimer[0]);
}

/* ================================================================
 * 4. Round-Robin Rotation
 * ================================================================ */

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-140
 * @scenario RotationTimer expiry pauses current EVSE and activates next
 * @given 3 EVSEs in STATE_C, EVSE[0] active, RotationInterval=30
 * @and RotationTimer = 1 (about to expire)
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[0] paused, EVSE[1] activated, RotationTimer reset to 1800
 */
void test_rotation_timer_expires(void) {
    setup_master_n(3);
    ctx.RotationInterval = 30;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.ScheduleState[2] = SCHED_PAUSED;
    ctx.IdleTimer[0] = 100;  /* Past idle timeout */
    ctx.Balanced[0] = 60;
    ctx.EVMeterImeasured = 60;  /* Drawing power (not idle) */
    ctx.RotationTimer = 1;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;
    ctx.ConnectedTime[2] = 3;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.IdleTimer[1]);
    TEST_ASSERT_EQUAL_INT(1800, ctx.RotationTimer);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-141
 * @scenario RotationInterval=0 disables rotation entirely
 * @given 2 EVSEs, EVSE[0] active, RotationInterval = 0
 * @and Many seconds elapse
 * @when Checking ScheduleState
 * @then EVSE[0] still active (never rotated)
 */
void test_rotation_disabled(void) {
    setup_master_n(2);
    ctx.RotationInterval = 0;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.IdleTimer[0] = 100;  /* Past idle timeout */
    ctx.Balanced[0] = 60;
    ctx.EVMeterImeasured = 60;  /* Drawing power */
    ctx.RotationTimer = 0;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;

    /* Run 100 ticks — should never rotate */
    for (int i = 0; i < 100; i++) {
        evse_schedule_tick_1s(&ctx);
    }

    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx.RotationTimer);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-142
 * @scenario Rotation wraps from last priority to first
 * @given 3 EVSEs in priority order {0,1,2}, EVSE[2] active
 * @and RotationTimer = 1
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[2] paused, EVSE[0] activated
 */
void test_rotation_wraps_to_first(void) {
    setup_master_n(3);
    ctx.RotationInterval = 30;
    ctx.ScheduleState[0] = SCHED_PAUSED;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.ScheduleState[2] = SCHED_ACTIVE;
    ctx.IdleTimer[2] = 100;
    ctx.Balanced[2] = 60;
    ctx.EVMeterImeasured = 60;
    ctx.RotationTimer = 1;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;
    ctx.ConnectedTime[2] = 3;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[2]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-143
 * @scenario Rotation skips disconnected EVSEs (STATE_A)
 * @given 3 EVSEs: [0] active, [1] STATE_A disconnected, [2] paused
 * @and RotationTimer = 1
 * @when evse_schedule_tick_1s() is called
 * @then EVSE[0] paused, EVSE[2] activated (EVSE[1] skipped)
 */
void test_rotation_skips_disconnected(void) {
    setup_master_n(3);
    ctx.BalancedState[1] = STATE_A;
    ctx.RotationInterval = 30;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_INACTIVE;
    ctx.ScheduleState[2] = SCHED_PAUSED;
    ctx.IdleTimer[0] = 100;
    ctx.Balanced[0] = 60;
    ctx.EVMeterImeasured = 60;
    ctx.RotationTimer = 1;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 0;  /* Disconnected */
    ctx.ConnectedTime[2] = 3;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[2]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-144
 * @scenario Newly activated EVSE gets idle check before rotation timer applies
 * @given EVSE[1] just activated via rotation, IdleTimeout=60, RotationInterval=30
 * @and EVSE[1] drawing 0A
 * @when 60 seconds pass
 * @then EVSE[1] paused due to idle (not waiting for rotation)
 */
void test_idle_check_before_rotation(void) {
    setup_master_n(3);
    ctx.IdleTimeout = 60;
    ctx.RotationInterval = 30;
    ctx.ScheduleState[0] = SCHED_PAUSED;
    ctx.ScheduleState[1] = SCHED_ACTIVE;
    ctx.ScheduleState[2] = SCHED_PAUSED;
    ctx.IdleTimer[1] = 0;
    ctx.Balanced[1] = 60;
    ctx.EVMeterImeasured = 0;  /* Not drawing power */
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;
    ctx.ConnectedTime[2] = 3;

    /* Run 60 ticks to reach IdleTimeout */
    for (int i = 0; i < 60; i++) {
        evse_schedule_tick_1s(&ctx);
    }

    /* EVSE[1] should be paused after idle detection (60 seconds, not 30 minutes) */
    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[1]);
}

/* ================================================================
 * 5. Power Increase Reactivation
 * ================================================================ */

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-150
 * @scenario Power increases: paused EVSE reactivated immediately
 * @given 3 EVSEs, EVSE[0] active, EVSE[1] and [2] paused
 * @and Available power increases to enough for 2 at MinCurrent
 * @when evse_calc_balanced_current(ctx, 0) is called with new power
 * @then EVSE[1] reactivated, IdleTimer reset
 */
void test_power_increase_reactivates(void) {
    setup_master_n(3);
    /* Start with shortage: MaxCircuit=8, only EVSE[0] can charge */
    ctx.MaxCircuit = 8;
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 80;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.ScheduleState[2] = SCHED_PAUSED;
    ctx.IdleTimer[1] = 30;

    /* Power increases: MaxCircuit now 14A (enough for 2) */
    ctx.MaxCircuit = 14;

    evse_calc_balanced_current(&ctx, 0);

    /* With 140 available and 3*60=180 needed, still shortage.
     * EVSE[0] gets 60, EVSE[1] gets 60 (140-60=80>=60), EVSE[2] gets 0 (80-60=20<60) */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[1]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-151
 * @scenario Reactivation follows priority order
 * @given 3 EVSEs paused, PrioStrategy=PRIO_MODBUS_ADDR
 * @and Power increases to enough for 2 EVSEs
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then EVSE[0] and EVSE[1] activated (not [0] and [2])
 */
void test_reactivation_follows_priority(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 14;  /* 140 available, enough for 2 at MinCurrent */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 0;
    ctx.Balanced[1] = 0;
    ctx.Balanced[2] = 0;
    ctx.ScheduleState[0] = SCHED_PAUSED;
    ctx.ScheduleState[1] = SCHED_PAUSED;
    ctx.ScheduleState[2] = SCHED_PAUSED;
    ctx.PrioStrategy = PRIO_MODBUS_ADDR;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[0]);
    TEST_ASSERT_EQUAL_INT(SCHED_ACTIVE, ctx.ScheduleState[1]);
    TEST_ASSERT_EQUAL_INT(SCHED_PAUSED, ctx.ScheduleState[2]);
}

/* ================================================================
 * 6. Regression / Integration
 * ================================================================ */

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-160
 * @scenario Original bug: 2 EVSEs, power drops, only 1 stops (no oscillation)
 * @given Master with 2 EVSEs in STATE_C, MinCurrent=6A, MaxCircuit=11A
 * @and Available power = 11A (was 14A, enough for both; now only 1)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then Exactly 1 EVSE continues, 1 paused, NoCurrent == 0
 */
void test_regression_no_oscillation(void) {
    setup_master_n(2);
    ctx.MaxCircuit = 11;  /* IsetBalanced = 110; need 120 → shortage */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 70;
    ctx.Balanced[1] = 70;
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 0);

    int charging = 0;
    int paused = 0;
    for (int i = 0; i < 2; i++) {
        if (ctx.Balanced[i] >= 60) charging++;
        if (ctx.Balanced[i] == 0) paused++;
    }
    TEST_ASSERT_EQUAL_INT(1, charging);
    TEST_ASSERT_EQUAL_INT(1, paused);
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-161
 * @scenario 6 EVSEs, power for 5: lowest priority paused
 * @given Master with 6 EVSEs in STATE_C, MinCurrent=6A
 * @and Available power = 32A (enough for 5*6A=30A, not 6*6A=36A)
 * @when evse_calc_balanced_current(ctx, 0) is called
 * @then EVSEs [0]-[4] receive current, EVSE[5] paused
 */
void test_six_evse_lowest_paused(void) {
    setup_master_n(6);
    ctx.MaxCircuit = 32;  /* IsetBalanced = 320; need 6*60=360 → shortage */
    ctx.EVMeterImeasured = 0;
    for (int i = 0; i < 6; i++)
        ctx.Balanced[i] = 0;

    evse_calc_balanced_current(&ctx, 0);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[i]);
    }
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[5]);
    TEST_ASSERT_TRUE(ctx.BalancedError[5] & LESS_6A);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-162
 * @scenario Node goes offline: removed from scheduling
 * @given 3 EVSEs, EVSE[1] goes offline (STATE_A)
 * @and EVSE[1] was previously SCHED_ACTIVE
 * @when evse_schedule_tick_1s() runs
 * @then EVSE[1] gets ScheduleState = SCHED_INACTIVE
 */
void test_offline_node_removed(void) {
    setup_master_n(3);
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_ACTIVE;
    ctx.ScheduleState[2] = SCHED_PAUSED;
    ctx.ConnectedTime[0] = 1;
    ctx.ConnectedTime[1] = 2;
    ctx.ConnectedTime[2] = 3;

    /* EVSE[1] goes offline */
    ctx.BalancedState[1] = STATE_A;

    evse_schedule_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(SCHED_INACTIVE, ctx.ScheduleState[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.ConnectedTime[1]);
}

/*
 * @feature Priority-Based Power Scheduling
 * @req REQ-LB-163
 * @scenario New EVSE join during shortage doesn't displace active ones
 * @given 2 EVSEs charging, power for 2 at MinCurrent
 * @and New EVSE[2] joins (STATE_C)
 * @when evse_calc_balanced_current(ctx, 1) is called
 * @then EVSE[0] and EVSE[1] keep allocation, EVSE[2] gets 0
 */
void test_new_evse_doesnt_displace(void) {
    setup_master_n(3);
    ctx.MaxCircuit = 13;  /* IsetBalanced = 130; need 3*60=180 → shortage */
    ctx.EVMeterImeasured = 0;
    ctx.Balanced[0] = 60;
    ctx.Balanced[1] = 60;
    ctx.Balanced[2] = 0;
    ctx.ScheduleState[0] = SCHED_ACTIVE;
    ctx.ScheduleState[1] = SCHED_ACTIVE;

    evse_calc_balanced_current(&ctx, 1);

    /* Priority scheduling: available=130. EVSE[0]=60 (avail=70), EVSE[1]=60 (avail=10),
     * EVSE[2]=0 (10<60, paused). Surplus 10 distributed to [0] and [1]. */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[2]);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Priority-Based Power Scheduling");

    /* 1. Priority Sorting */
    RUN_TEST(test_sort_modbus_addr);
    RUN_TEST(test_sort_first_connected);
    RUN_TEST(test_sort_last_connected);
    RUN_TEST(test_sort_disconnected_to_end);

    /* 2. Priority Allocation */
    RUN_TEST(test_shortage_first_two_get_current);
    RUN_TEST(test_shortage_one_evse_gets_all);
    RUN_TEST(test_sufficient_power_no_scheduling);
    RUN_TEST(test_surplus_distributed_fairly);
    RUN_TEST(test_standalone_no_scheduling);
    RUN_TEST(test_solar_paused_gets_no_sun);
    RUN_TEST(test_capped_surplus_redistribution);
    RUN_TEST(test_exactly_one_mincurrent);
    RUN_TEST(test_zero_power_pauses_all);
    RUN_TEST(test_no_current_not_incremented_on_deliberate_pause);

    /* 3. Idle Detection */
    RUN_TEST(test_idle_evse_paused_at_timeout);
    RUN_TEST(test_antiflap_not_paused_early);
    RUN_TEST(test_charging_evse_stays_active);
    RUN_TEST(test_idle_cycle_wraps_around);

    /* 4. Rotation */
    RUN_TEST(test_rotation_timer_expires);
    RUN_TEST(test_rotation_disabled);
    RUN_TEST(test_rotation_wraps_to_first);
    RUN_TEST(test_rotation_skips_disconnected);
    RUN_TEST(test_idle_check_before_rotation);

    /* 5. Power Increase */
    RUN_TEST(test_power_increase_reactivates);
    RUN_TEST(test_reactivation_follows_priority);

    /* 6. Regression */
    RUN_TEST(test_regression_no_oscillation);
    RUN_TEST(test_six_evse_lowest_paused);
    RUN_TEST(test_offline_node_removed);
    RUN_TEST(test_new_evse_doesnt_displace);

    TEST_SUITE_RESULTS();
}
