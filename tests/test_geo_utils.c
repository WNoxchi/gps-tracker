#include "unity.h"
#include "geo_utils.h"
#include <math.h>

void setUp(void) { }

void tearDown(void) { }

void test_haversine_known_distance(void) {
    /* Zurich (47.3769, 8.5417) to Bern (46.9480, 7.4474) ≈ 95,493m */
    double d = haversine_distance_m(47.3769, 8.5417, 46.9480, 7.4474);
    TEST_ASSERT_FLOAT_WITHIN(500.0, 95493.0, d);
}

void test_haversine_zero_distance(void) {
    double d = haversine_distance_m(47.3769, 8.5417, 47.3769, 8.5417);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, d);
}

void test_haversine_antipodal(void) {
    /* (0,0) to (0,180) ≈ half Earth circumference ≈ 20,015,087m */
    double d = haversine_distance_m(0.0, 0.0, 0.0, 180.0);
    TEST_ASSERT_FLOAT_WITHIN(1000.0, 20015087.0, d);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_haversine_known_distance);
    RUN_TEST(test_haversine_zero_distance);
    RUN_TEST(test_haversine_antipodal);
    return UNITY_END();
}
