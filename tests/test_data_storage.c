#include "unity.h"
#include "data_storage.h"
#include "hal/hal.h"
#include "hal/hal_mock.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static data_storage_t storage;
static char tmpdir[256];

static void create_tmpdir(void) {
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/gps_test_XXXXXX");
    char* result = mkdtemp(tmpdir);
    (void)result;
}

static void remove_tmpdir(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

void setUp(void) {
    hal_mock_reset();
    create_tmpdir();
    hal_mock_fs_set_root(tmpdir);
    memset(&storage, 0, sizeof(storage));
}

void tearDown(void) {
    remove_tmpdir();
}

static void write_file(const char* name, const char* content) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
    }
}

static char* read_file(const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)len + 1);
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static bool file_exists(const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

static gps_fix_t make_test_fix(void) {
    gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.flags = GPS_FIX_VALID | GPS_HAS_TIME | GPS_HAS_DATE | GPS_HAS_LATLON
              | GPS_HAS_SPEED | GPS_HAS_ALTITUDE | GPS_HAS_COURSE | GPS_HAS_HDOP;
    fix.year = 2025; fix.month = 6; fix.day = 15;
    fix.hour = 14; fix.minute = 23; fix.second = 7;
    fix.latitude = 47.285233;
    fix.longitude = 8.565265;
    fix.speed_kmh = 52.30f;
    fix.altitude_m = 499.6f;
    fix.course_deg = 77.5f;
    fix.satellites = 8;
    fix.hdop = 1.01f;
    fix.fix_quality = 1;
    return fix;
}

/* T1: fresh start creates file */
void test_fresh_start_creates_file(void) {
    storage_error_t err = data_storage_init(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    TEST_ASSERT_EQUAL_STRING("track.csv", data_storage_get_filename(&storage));
    TEST_ASSERT_TRUE(file_exists("track.csv"));
    data_storage_shutdown(&storage);
}

/* T2: append to clean file */
void test_append_to_clean_file(void) {
    write_file("track.csv", CSV_HEADER);
    storage_error_t err = data_storage_init(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    TEST_ASSERT_EQUAL_STRING("track.csv", data_storage_get_filename(&storage));
    data_storage_shutdown(&storage);
}

/* T3: dirty flag triggers rotation */
void test_dirty_flag_triggers_rotation(void) {
    write_file("track.csv", CSV_HEADER);
    write_file("_dirty", "");
    storage_error_t err = data_storage_init(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    TEST_ASSERT_EQUAL_STRING("track_1.csv", data_storage_get_filename(&storage));
    data_storage_shutdown(&storage);
}

/* T4: incomplete line triggers rotation */
void test_incomplete_line_triggers_rotation(void) {
    write_file("track.csv", "timestamp,latitude,longitude\n47.285233,8.565");
    /* Note: no trailing newline on the data, so it triggers rotation */
    storage_error_t err = data_storage_init(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    TEST_ASSERT_EQUAL_STRING("track_1.csv", data_storage_get_filename(&storage));
    data_storage_shutdown(&storage);
}

/* T5: sequential rotation */
void test_sequential_rotation(void) {
    write_file("track.csv", CSV_HEADER);
    write_file("track_1.csv", CSV_HEADER);
    write_file("_dirty", "");
    storage_error_t err = data_storage_init(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    TEST_ASSERT_EQUAL_STRING("track_2.csv", data_storage_get_filename(&storage));
    data_storage_shutdown(&storage);
}

/* T6: write fix CSV format */
void test_write_fix_csv_format(void) {
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();
    data_storage_write_fix(&storage, &fix);
    data_storage_shutdown(&storage);

    char* content = read_file("track.csv");
    TEST_ASSERT_NOT_NULL(content);
    /* Find second line (after header) */
    char* line = strchr(content, '\n');
    TEST_ASSERT_NOT_NULL(line);
    line++; /* skip newline */
    TEST_ASSERT_TRUE(strstr(line, "2025-06-15T14:23:07Z") != NULL);
    TEST_ASSERT_TRUE(strstr(line, "47.285233") != NULL);
    TEST_ASSERT_TRUE(strstr(line, "8.565265") != NULL);
    free(content);
}

/* T7: write fix missing altitude */
void test_write_fix_missing_altitude(void) {
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();
    fix.flags &= ~GPS_HAS_ALTITUDE;
    data_storage_write_fix(&storage, &fix);
    data_storage_shutdown(&storage);

    char* content = read_file("track.csv");
    TEST_ASSERT_NOT_NULL(content);
    /* Should have adjacent commas for missing altitude */
    char* line = strchr(content, '\n');
    line++;
    /* Pattern: speed_kmh,,course_deg (empty altitude between two commas) */
    TEST_ASSERT_TRUE(strstr(line, "52.30,,77.5") != NULL);
    free(content);
}

/* T8: sync after interval */
void test_sync_after_interval(void) {
    hal_mock_time_set_ms(0);
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();

    /* Write first fix at t=0 */
    data_storage_write_fix(&storage, &fix);
    /* Advance past sync interval */
    hal_mock_time_advance_ms(6000);
    /* Write second fix — should trigger sync */
    storage_error_t err = data_storage_write_fix(&storage, &fix);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    data_storage_shutdown(&storage);
}

/* T9: no sync before interval */
void test_no_sync_before_interval(void) {
    hal_mock_time_set_ms(0);
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();

    data_storage_write_fix(&storage, &fix);
    hal_mock_time_advance_ms(3000);
    storage_error_t err = data_storage_write_fix(&storage, &fix);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    data_storage_shutdown(&storage);
}

/* T10: shutdown sequence */
void test_shutdown_sequence(void) {
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();
    data_storage_write_fix(&storage, &fix);

    storage_error_t err = data_storage_shutdown(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, err);
    /* Dirty marker should be removed */
    TEST_ASSERT_FALSE(file_exists("_dirty"));
}

/* T11: write after shutdown */
void test_write_after_shutdown(void) {
    data_storage_init(&storage);
    data_storage_shutdown(&storage);

    gps_fix_t fix = make_test_fix();
    storage_error_t err = data_storage_write_fix(&storage, &fix);
    TEST_ASSERT_EQUAL_INT(STORAGE_ERR_WRITE, err);
}

/* T12: max files error */
void test_max_files_error(void) {
    /* Create track.csv through track_999.csv */
    write_file("track.csv", CSV_HEADER);
    for (int i = 1; i <= 999; i++) {
        char name[32];
        snprintf(name, sizeof(name), "track_%d.csv", i);
        write_file(name, CSV_HEADER);
    }
    write_file("_dirty", "");

    storage_error_t err = data_storage_init(&storage);
    TEST_ASSERT_EQUAL_INT(STORAGE_ERR_TOO_MANY_FILES, err);
}

/* T13: CSV header content */
void test_csv_header_content(void) {
    data_storage_init(&storage);
    data_storage_shutdown(&storage);

    char* content = read_file("track.csv");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_TRUE(strstr(content, "timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality\n") == content);
    free(content);
}

/* T14: timestamp format */
void test_timestamp_format(void) {
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();
    data_storage_write_fix(&storage, &fix);
    data_storage_shutdown(&storage);

    char* content = read_file("track.csv");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_TRUE(strstr(content, "2025-06-15T14:23:07Z") != NULL);
    free(content);
}

/* T15: coordinate precision */
void test_coordinate_precision(void) {
    data_storage_init(&storage);
    gps_fix_t fix = make_test_fix();
    fix.latitude = 47.2852333333;
    fix.longitude = 8.5652654321;
    data_storage_write_fix(&storage, &fix);
    data_storage_shutdown(&storage);

    char* content = read_file("track.csv");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_TRUE(strstr(content, "47.285233") != NULL);
    TEST_ASSERT_TRUE(strstr(content, "8.565265") != NULL);
    free(content);
}

int main(void) {
    UNITY_BEGIN();
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
    return UNITY_END();
}
