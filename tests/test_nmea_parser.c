#include "unity.h"
#include "nmea_parser.h"
#include <string.h>
#include <math.h>

static nmea_parser_t* parser;

void setUp(void) {
    parser = nmea_parser_create();
}

void tearDown(void) {
    nmea_parser_destroy(parser);
    parser = NULL;
}

/* Helper: compute NMEA checksum and build full sentence */
static void build_sentence(char* out, size_t out_size, const char* body) {
    uint8_t cs = 0;
    for (const char* p = body; *p; p++) {
        cs ^= (uint8_t)*p;
    }
    snprintf(out, out_size, "$%s*%02X", body, cs);
}

/* T1: valid GGA + RMC pair */
void test_valid_gga_rmc_pair(void) {
    char gga[128], rmc[128];
    build_sentence(gga, sizeof(gga), "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    build_sentence(rmc, sizeof(rmc), "GPRMC,092725.00,A,4717.11399,N,00833.91590,E,0.004,77.52,091202,,,A");

    nmea_result_t r1 = nmea_parser_feed(parser, gga);
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_NONE, r1);

    /* Feed a different-time GGA to trigger epoch completion */
    char gga2[128];
    build_sentence(gga2, sizeof(gga2), "GPGGA,092726.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");

    nmea_parser_feed(parser, rmc);
    nmea_result_t r3 = nmea_parser_feed(parser, gga2);
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_FIX_READY, r3);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_TRUE(fix.flags & GPS_FIX_VALID);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_TIME);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_DATE);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_LATLON);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_ALTITUDE);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_SPEED);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_COURSE);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_HDOP);
    TEST_ASSERT_EQUAL_UINT8(9, fix.hour);
    TEST_ASSERT_EQUAL_UINT8(27, fix.minute);
    TEST_ASSERT_EQUAL_UINT8(25, fix.second);
}

/* T2: coordinate north/east */
void test_coordinate_north_east(void) {
    char gga[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    build_sentence(trigger, sizeof(trigger), "GPGGA,092726.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FLOAT_WITHIN(0.000001, 47.285233, fix.latitude);
    TEST_ASSERT_FLOAT_WITHIN(0.000001, 8.565265, fix.longitude);
}

/* T3: coordinate south/west */
void test_coordinate_south_west(void) {
    char gga[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,100000.00,3402.54320,S,11832.10990,W,1,06,1.50,100.0,M,0.0,M,,");
    build_sentence(trigger, sizeof(trigger), "GPGGA,100001.00,3402.54320,S,11832.10990,W,1,06,1.50,100.0,M,0.0,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FLOAT_WITHIN(0.000001, -34.042387, fix.latitude);
    TEST_ASSERT_FLOAT_WITHIN(0.000001, -118.535165, fix.longitude);
}

/* T4: speed conversion (knots to km/h) */
void test_speed_conversion(void) {
    char gga[128], rmc[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,110000.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    build_sentence(rmc, sizeof(rmc), "GPRMC,110000.00,A,4717.11399,N,00833.91590,E,5.400,77.52,091202,,,A");
    build_sentence(trigger, sizeof(trigger), "GPGGA,110001.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, rmc);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 10.00, fix.speed_kmh);
}

/* T5: no fix GGA (fix_quality=0) */
void test_no_fix_gga(void) {
    char gga[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,120000.00,,,,,0,00,99.99,,M,,M,,");
    build_sentence(trigger, sizeof(trigger), "GPGGA,120001.00,,,,,0,00,99.99,,M,,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FALSE(fix.flags & GPS_FIX_VALID);
}

/* T6: no fix RMC (status=V) */
void test_no_fix_rmc(void) {
    char gga[128], rmc[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,130000.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    build_sentence(rmc, sizeof(rmc), "GPRMC,130000.00,V,4717.11399,N,00833.91590,E,0.0,,091202,,,N");
    build_sentence(trigger, sizeof(trigger), "GPGGA,130001.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, rmc);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FALSE(fix.flags & GPS_FIX_VALID);
}

/* T7: bad checksum */
void test_bad_checksum(void) {
    nmea_result_t r = nmea_parser_feed(parser, "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*FF");
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_ERROR, r);
}

/* T8: missing checksum */
void test_missing_checksum(void) {
    nmea_result_t r = nmea_parser_feed(parser, "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_ERROR, r);
}

/* T9: truncated sentence */
void test_truncated_sentence(void) {
    nmea_result_t r = nmea_parser_feed(parser, "$GPGGA,09272");
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_ERROR, r);
}

/* T10: empty string */
void test_empty_string(void) {
    nmea_result_t r = nmea_parser_feed(parser, "");
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_ERROR, r);
}

/* T11: null pointer */
void test_null_pointer(void) {
    nmea_result_t r = nmea_parser_feed(parser, NULL);
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_ERROR, r);
}

/* T12: garbage input */
void test_garbage_input(void) {
    nmea_result_t r = nmea_parser_feed(parser, "hello world\n");
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_ERROR, r);
}

/* T13: ignored sentences */
void test_ignored_sentences(void) {
    char gsv[128], gsa[128], vtg[128];
    build_sentence(gsv, sizeof(gsv), "GPGSV,3,1,12,01,40,083,46,02,17,308,44,12,07,344,39,14,22,228,45");
    build_sentence(gsa, sizeof(gsa), "GPGSA,A,3,01,02,12,14,,,,,,,,,2.0,1.01,1.7");
    build_sentence(vtg, sizeof(vtg), "GPVTG,77.52,T,,M,0.004,N,0.008,K,A");

    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_NONE, nmea_parser_feed(parser, gsv));
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_NONE, nmea_parser_feed(parser, gsa));
    TEST_ASSERT_EQUAL_INT(NMEA_RESULT_NONE, nmea_parser_feed(parser, vtg));
}

/* T14: missing optional fields */
void test_missing_optional_fields(void) {
    char rmc[128], trigger[128];
    build_sentence(rmc, sizeof(rmc), "GPRMC,140000.00,A,4717.11399,N,00833.91590,E,5.0,,091202,,,A");
    build_sentence(trigger, sizeof(trigger), "GPRMC,140001.00,A,4717.11399,N,00833.91590,E,5.0,,091202,,,A");

    nmea_parser_feed(parser, rmc);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FALSE(fix.flags & GPS_HAS_COURSE);
    TEST_ASSERT_FALSE(fix.flags & GPS_HAS_ALTITUDE);
}

/* T15: mixed talker IDs */
void test_mixed_talker_ids(void) {
    char gga[128], rmc[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,150000.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    build_sentence(rmc, sizeof(rmc), "GNRMC,150000.00,A,4717.11399,N,00833.91590,E,5.0,77.52,091202,,,A");
    build_sentence(trigger, sizeof(trigger), "GPGGA,150001.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, rmc);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_TRUE(fix.flags & GPS_FIX_VALID);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_SPEED);
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_ALTITUDE);
}

/* T16: RMC date parsing */
void test_rmc_date_parsing(void) {
    char rmc[128], trigger[128];
    build_sentence(rmc, sizeof(rmc), "GPRMC,160000.00,A,4717.11399,N,00833.91590,E,5.0,77.52,091202,,,A");
    build_sentence(trigger, sizeof(trigger), "GPRMC,160001.00,A,4717.11399,N,00833.91590,E,5.0,77.52,091202,,,A");

    nmea_parser_feed(parser, rmc);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_TRUE(fix.flags & GPS_HAS_DATE);
    TEST_ASSERT_EQUAL_UINT8(9, fix.day);
    TEST_ASSERT_EQUAL_UINT8(12, fix.month);
    TEST_ASSERT_EQUAL_UINT16(2002, fix.year);
}

/* T17: sequential fixes */
void test_sequential_fixes(void) {
    char gga1[128], rmc1[128], gga2[128], rmc2[128], gga3[128], rmc3[128], trigger[128];
    build_sentence(gga1, sizeof(gga1), "GPGGA,170000.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,");
    build_sentence(rmc1, sizeof(rmc1), "GPRMC,170000.00,A,4717.11399,N,00833.91590,E,5.0,77.52,091202,,,A");
    build_sentence(gga2, sizeof(gga2), "GPGGA,170001.00,4718.00000,N,00834.00000,E,1,08,1.01,500.0,M,48.0,M,,");
    build_sentence(rmc2, sizeof(rmc2), "GPRMC,170001.00,A,4718.00000,N,00834.00000,E,10.0,80.00,091202,,,A");
    build_sentence(gga3, sizeof(gga3), "GPGGA,170002.00,4719.00000,N,00835.00000,E,1,08,1.01,501.0,M,48.0,M,,");
    build_sentence(rmc3, sizeof(rmc3), "GPRMC,170002.00,A,4719.00000,N,00835.00000,E,15.0,85.00,091202,,,A");
    build_sentence(trigger, sizeof(trigger), "GPGGA,170003.00,4719.00000,N,00835.00000,E,1,08,1.01,501.0,M,48.0,M,,");

    /* First fix epoch */
    nmea_parser_feed(parser, gga1);
    nmea_parser_feed(parser, rmc1);

    /* Second fix epoch triggers emission of first */
    nmea_parser_feed(parser, gga2);
    gps_fix_t fix1;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix1));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 47.285233, fix1.latitude);

    nmea_parser_feed(parser, rmc2);

    /* Third fix epoch triggers emission of second */
    nmea_parser_feed(parser, gga3);
    gps_fix_t fix2;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix2));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 47.300000, fix2.latitude);

    nmea_parser_feed(parser, rmc3);
    nmea_parser_feed(parser, trigger);
    gps_fix_t fix3;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix3));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 47.316667, fix3.latitude);
}

/* T18: empty position fields (cold start) */
void test_empty_position_fields(void) {
    char gga[128], trigger[128];
    build_sentence(gga, sizeof(gga), "GPGGA,180000.00,,,,,0,00,99.99,,M,,M,,");
    build_sentence(trigger, sizeof(trigger), "GPGGA,180001.00,,,,,0,00,99.99,,M,,M,,");

    nmea_parser_feed(parser, gga);
    nmea_parser_feed(parser, trigger);

    gps_fix_t fix;
    TEST_ASSERT_TRUE(nmea_parser_get_fix(parser, &fix));
    TEST_ASSERT_FALSE(fix.flags & GPS_HAS_LATLON);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_gga_rmc_pair);
    RUN_TEST(test_coordinate_north_east);
    RUN_TEST(test_coordinate_south_west);
    RUN_TEST(test_speed_conversion);
    RUN_TEST(test_no_fix_gga);
    RUN_TEST(test_no_fix_rmc);
    RUN_TEST(test_bad_checksum);
    RUN_TEST(test_missing_checksum);
    RUN_TEST(test_truncated_sentence);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_null_pointer);
    RUN_TEST(test_garbage_input);
    RUN_TEST(test_ignored_sentences);
    RUN_TEST(test_missing_optional_fields);
    RUN_TEST(test_mixed_talker_ids);
    RUN_TEST(test_rmc_date_parsing);
    RUN_TEST(test_sequential_fixes);
    RUN_TEST(test_empty_position_fields);
    return UNITY_END();
}
