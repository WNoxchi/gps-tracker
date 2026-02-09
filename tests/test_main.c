#include "unity.h"

/* test_geo_utils.c */
extern void test_haversine_known_distance(void);
extern void test_haversine_zero_distance(void);
extern void test_haversine_antipodal(void);

/* test_nmea_parser.c */
extern void test_valid_gga_rmc_pair(void);
extern void test_coordinate_north_east(void);
extern void test_coordinate_south_west(void);
extern void test_speed_conversion(void);
extern void test_no_fix_gga(void);
extern void test_no_fix_rmc(void);
extern void test_bad_checksum(void);
extern void test_missing_checksum(void);
extern void test_truncated_sentence(void);
extern void test_empty_string(void);
extern void test_null_pointer(void);
extern void test_garbage_input(void);
extern void test_ignored_sentences(void);
extern void test_missing_optional_fields(void);
extern void test_mixed_talker_ids(void);
extern void test_rmc_date_parsing(void);
extern void test_sequential_fixes(void);
extern void test_empty_position_fields(void);

/* test_gps_filter.c */
extern void test_reject_invalid_fix(void);
extern void test_reject_no_position(void);
extern void test_accept_first_moving_fix(void);
extern void test_reject_stationary_cold_start(void);
extern void test_accept_moving_fix(void);
extern void test_reject_outlier(void);
extern void test_moving_to_stopped(void);
extern void test_reject_while_stopped(void);
extern void test_stopped_to_moving(void);
extern void test_speed_gate_skipped_first(void);
extern void test_reject_zero_time_delta(void);
extern void test_missing_speed_stationary(void);
extern void test_realistic_driving_sequence(void);

/* test_data_storage.c */
extern void test_fresh_start_creates_file(void);
extern void test_append_to_clean_file(void);
extern void test_dirty_flag_triggers_rotation(void);
extern void test_incomplete_line_triggers_rotation(void);
extern void test_sequential_rotation(void);
extern void test_write_fix_csv_format(void);
extern void test_write_fix_missing_altitude(void);
extern void test_sync_after_interval(void);
extern void test_no_sync_before_interval(void);
extern void test_shutdown_sequence(void);
extern void test_write_after_shutdown(void);
extern void test_max_files_error(void);
extern void test_csv_header_content(void);
extern void test_timestamp_format(void);
extern void test_coordinate_precision(void);

/* test_power_mgmt.c */
extern void test_initial_no_shutdown(void);
extern void test_isr_sets_flag(void);
extern void test_vbus_present(void);
extern void test_vbus_absent(void);
extern void test_shutdown_closes_storage(void);
extern void test_shutdown_idempotent(void);
extern void test_gpio_configured_input(void);
extern void test_falling_edge_registered(void);

int main(void) {
    UNITY_BEGIN();

    /* Geo Utils */
    RUN_TEST(test_haversine_known_distance);
    RUN_TEST(test_haversine_zero_distance);
    RUN_TEST(test_haversine_antipodal);

    /* NMEA Parser */
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

    /* GPS Filter */
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

    /* Data Storage */
    RUN_TEST(test_fresh_start_creates_file);
    RUN_TEST(test_append_to_clean_file);
    RUN_TEST(test_dirty_flag_triggers_rotation);
    RUN_TEST(test_incomplete_line_triggers_rotation);
    RUN_TEST(test_sequential_rotation);
    RUN_TEST(test_write_fix_csv_format);
    RUN_TEST(test_write_fix_missing_altitude);
    RUN_TEST(test_sync_after_interval);
    RUN_TEST(test_no_sync_before_interval);
    RUN_TEST(test_shutdown_sequence);
    RUN_TEST(test_write_after_shutdown);
    RUN_TEST(test_max_files_error);
    RUN_TEST(test_csv_header_content);
    RUN_TEST(test_timestamp_format);
    RUN_TEST(test_coordinate_precision);

    /* Power Management */
    RUN_TEST(test_initial_no_shutdown);
    RUN_TEST(test_isr_sets_flag);
    RUN_TEST(test_vbus_present);
    RUN_TEST(test_vbus_absent);
    RUN_TEST(test_shutdown_closes_storage);
    RUN_TEST(test_shutdown_idempotent);
    RUN_TEST(test_gpio_configured_input);
    RUN_TEST(test_falling_edge_registered);

    return UNITY_END();
}
