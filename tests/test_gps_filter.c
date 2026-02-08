#include "unity.h"
#include "gps_filter.h"
#include "geo_utils.h"
#include <string.h>

static gps_filter_t filter;

static gps_fix_t make_fix(double lat, double lon, float speed, uint8_t h, uint8_t m, uint8_t s) {
    gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.flags = GPS_FIX_VALID | GPS_HAS_LATLON | GPS_HAS_SPEED | GPS_HAS_TIME;
    fix.latitude = lat;
    fix.longitude = lon;
    fix.speed_kmh = speed;
    fix.hour = h;
    fix.minute = m;
    fix.second = s;
    return fix;
}

void setUp(void) {
    gps_filter_init(&filter);
}

void tearDown(void) { }

/* T1: reject invalid fix */
void test_reject_invalid_fix(void) {
    gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    /* GPS_FIX_VALID not set */
    fix.flags = GPS_HAS_LATLON;
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_INVALID, gps_filter_process(&filter, &fix));
}

/* T2: reject no position */
void test_reject_no_position(void) {
    gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.flags = GPS_FIX_VALID; /* GPS_HAS_LATLON not set */
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_INVALID, gps_filter_process(&filter, &fix));
}

/* T3: accept first moving fix from COLD_START */
void test_accept_first_moving_fix(void) {
    gps_fix_t fix = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, gps_filter_process(&filter, &fix));
    TEST_ASSERT_EQUAL_INT(FILTER_STATE_MOVING, gps_filter_get_state(&filter));
}

/* T4: reject stationary in COLD_START */
void test_reject_stationary_cold_start(void) {
    gps_fix_t fix = make_fix(47.0, 8.0, 1.5, 10, 0, 0);
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_STATIONARY, gps_filter_process(&filter, &fix));
    TEST_ASSERT_EQUAL_INT(FILTER_STATE_COLD_START, gps_filter_get_state(&filter));
}

/* T5: accept moving fix in MOVING state */
void test_accept_moving_fix(void) {
    /* Get to MOVING state */
    gps_fix_t fix1 = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    gps_filter_process(&filter, &fix1);

    /* New fix ~100m away (0.0009 lat = ~100m), 10s later, 40 km/h */
    gps_fix_t fix2 = make_fix(47.0009, 8.0, 40.0, 10, 0, 10);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, gps_filter_process(&filter, &fix2));
}

/* T6: reject outlier */
void test_reject_outlier(void) {
    gps_fix_t fix1 = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    gps_filter_process(&filter, &fix1);

    /* 1 degree lat away (~111km) in 1 second = ~400,000 km/h */
    gps_fix_t fix2 = make_fix(48.0, 8.0, 50.0, 10, 0, 1);
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_OUTLIER, gps_filter_process(&filter, &fix2));
}

/* T7: moving to stopped */
void test_moving_to_stopped(void) {
    gps_fix_t fix1 = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    gps_filter_process(&filter, &fix1);

    gps_fix_t fix2 = make_fix(47.0001, 8.0, 1.0, 10, 0, 1);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, gps_filter_process(&filter, &fix2));
    TEST_ASSERT_EQUAL_INT(FILTER_STATE_STOPPED, gps_filter_get_state(&filter));
}

/* T8: reject while stopped */
void test_reject_while_stopped(void) {
    gps_fix_t fix1 = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    gps_filter_process(&filter, &fix1);

    gps_fix_t fix2 = make_fix(47.0001, 8.0, 1.0, 10, 0, 1);
    gps_filter_process(&filter, &fix2);

    gps_fix_t fix3 = make_fix(47.0001, 8.0, 0.5, 10, 0, 2);
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_STATIONARY, gps_filter_process(&filter, &fix3));
}

/* T9: stopped to moving */
void test_stopped_to_moving(void) {
    gps_fix_t fix1 = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    gps_filter_process(&filter, &fix1);

    gps_fix_t fix2 = make_fix(47.0001, 8.0, 1.0, 10, 0, 1);
    gps_filter_process(&filter, &fix2);

    gps_fix_t fix3 = make_fix(47.0002, 8.0, 15.0, 10, 0, 2);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, gps_filter_process(&filter, &fix3));
    TEST_ASSERT_EQUAL_INT(FILTER_STATE_MOVING, gps_filter_get_state(&filter));
}

/* T13: speed gate skipped for first fix */
void test_speed_gate_skipped_first(void) {
    gps_fix_t fix = make_fix(47.0, 8.0, 100.0, 10, 0, 0);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, gps_filter_process(&filter, &fix));
}

/* T14: reject zero time delta */
void test_reject_zero_time_delta(void) {
    gps_fix_t fix1 = make_fix(47.0, 8.0, 50.0, 10, 0, 0);
    gps_filter_process(&filter, &fix1);

    gps_fix_t fix2 = make_fix(47.001, 8.0, 50.0, 10, 0, 0);
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_NO_TIME_DELTA, gps_filter_process(&filter, &fix2));
}

/* T15: missing speed treated as stationary */
void test_missing_speed_stationary(void) {
    gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.flags = GPS_FIX_VALID | GPS_HAS_LATLON | GPS_HAS_TIME;
    fix.latitude = 47.0;
    fix.longitude = 8.0;
    fix.hour = 10;
    /* GPS_HAS_SPEED not set */
    TEST_ASSERT_EQUAL_INT(FILTER_REJECT_STATIONARY, gps_filter_process(&filter, &fix));
}

/* T16: realistic driving sequence */
void test_realistic_driving_sequence(void) {
    int accepted = 0;
    gps_filter_result_t r;

    /* Cold start: 3 stationary */
    for (int i = 0; i < 3; i++) {
        gps_fix_t fix = make_fix(47.0, 8.0, 1.0, 10, 0, (uint8_t)i);
        r = gps_filter_process(&filter, &fix);
        TEST_ASSERT_EQUAL_INT(FILTER_REJECT_STATIONARY, r);
    }

    /* Accelerate: first moving fix */
    gps_fix_t accel = make_fix(47.0, 8.0, 20.0, 10, 0, 3);
    r = gps_filter_process(&filter, &accel);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, r);
    accepted++;

    /* 5 moving fixes */
    for (int i = 0; i < 5; i++) {
        gps_fix_t fix = make_fix(47.0 + 0.0002 * (i + 1), 8.0, 40.0, 10, 0, (uint8_t)(4 + i));
        r = gps_filter_process(&filter, &fix);
        TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, r);
        accepted++;
    }

    /* Decelerate: stop point */
    gps_fix_t stop = make_fix(47.001 + 0.0001, 8.0, 1.0, 10, 0, 9);
    r = gps_filter_process(&filter, &stop);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, r); /* stop point kept */
    accepted++;

    /* 3 stationary while stopped */
    for (int i = 0; i < 3; i++) {
        gps_fix_t fix = make_fix(47.0012, 8.0, 0.5, 10, 0, (uint8_t)(10 + i));
        r = gps_filter_process(&filter, &fix);
        TEST_ASSERT_EQUAL_INT(FILTER_REJECT_STATIONARY, r);
    }

    /* Resume: accelerate again */
    gps_fix_t resume = make_fix(47.0012, 8.0, 15.0, 10, 0, 13);
    r = gps_filter_process(&filter, &resume);
    TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, r);
    accepted++;

    /* 3 more moving */
    for (int i = 0; i < 3; i++) {
        gps_fix_t fix = make_fix(47.0012 + 0.0002 * (i + 1), 8.0, 30.0, 10, 0, (uint8_t)(14 + i));
        r = gps_filter_process(&filter, &fix);
        TEST_ASSERT_EQUAL_INT(FILTER_ACCEPT, r);
        accepted++;
    }

    /* Total accepted: 1 (first moving) + 5 (moving) + 1 (stop) + 1 (resume) + 3 (moving) = 11 */
    TEST_ASSERT_EQUAL_INT(11, accepted);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reject_invalid_fix);
    RUN_TEST(test_reject_no_position);
    RUN_TEST(test_accept_first_moving_fix);
    RUN_TEST(test_reject_stationary_cold_start);
    RUN_TEST(test_accept_moving_fix);
    RUN_TEST(test_reject_outlier);
    RUN_TEST(test_moving_to_stopped);
    RUN_TEST(test_reject_while_stopped);
    RUN_TEST(test_stopped_to_moving);
    RUN_TEST(test_speed_gate_skipped_first);
    RUN_TEST(test_reject_zero_time_delta);
    RUN_TEST(test_missing_speed_stationary);
    RUN_TEST(test_realistic_driving_sequence);
    return UNITY_END();
}
