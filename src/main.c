#ifndef HOST_BUILD

#include "nmea_parser.h"
#include "gps_filter.h"
#include "data_storage.h"
#include "power_mgmt.h"
#include "hal/hal.h"
#include <stdio.h>
#include "pico/stdlib.h"

#define GPS_BAUD_RATE 9600

#ifdef HW_VALIDATION_TEST
#define HW_TEST_WRITE_WINDOW_MS 30000
#endif

int main(void) {
    stdio_init_all();

#ifdef HW_VALIDATION_TEST
    while (!stdio_usb_connected()) {
        hal_sleep_ms(100);
    }
    printf("GPS Tracker HW Validation starting...\n");
#endif

    /* 1. Initialize power management */
    power_mgmt_init();

    /* 2. Initialize UART for GPS */
    hal_uart_init(GPS_BAUD_RATE);

    /* 3. Initialize storage */
    data_storage_t storage;
    if (data_storage_init(&storage) != STORAGE_OK) {
        printf("ERROR: storage init failed\n");
        while (1) { /* halt */ }
    }
    printf("Storage OK, file: %s\n", data_storage_get_filename(&storage));

    /* 4. Initialize NMEA parser */
    nmea_parser_t* parser = nmea_parser_create();
    if (!parser) {
        data_storage_shutdown(&storage);
        while (1) { /* halt */ }
    }

    /* 5. Initialize GPS filter (COLD_START) */
    gps_filter_t filter;
    gps_filter_init(&filter);

#ifdef HW_VALIDATION_TEST
    uint32_t fix_count = 0;
    bool got_first_fix = false;
    uint32_t write_window_start = 0;
#endif

    /* 6. Main loop */
    char line_buf[NMEA_MAX_SENTENCE_LEN + 1];

    while (1) {
#ifdef HW_VALIDATION_TEST
        /* After first fix, run 30s write window then clean shutdown */
        if (got_first_fix && (hal_time_ms() - write_window_start > HW_TEST_WRITE_WINDOW_MS)) {
            printf("\n--- 30s write window complete ---\n");
            printf("Fixes written: %lu\n", (unsigned long)fix_count);
            data_storage_shutdown(&storage);
            nmea_parser_destroy(parser);
            printf("Storage shutdown OK — safe to unplug\n");
            while (1) { /* halt */ }
        }
#endif

        /* Check power — FIRST thing each iteration */
        if (power_mgmt_is_shutdown_requested()) {
            data_storage_shutdown(&storage);
            nmea_parser_destroy(parser);
            while (1) { /* halt, wait for power to die */ }
        }

        /* Read NMEA line from UART */
        int len = hal_uart_read_line(line_buf, sizeof(line_buf), 1100);
        if (len <= 0) continue;

        /* Parse */
        nmea_result_t result = nmea_parser_feed(parser, line_buf);
        if (result != NMEA_RESULT_FIX_READY) continue;

        /* Get completed fix */
        gps_fix_t fix;
        if (!nmea_parser_get_fix(parser, &fix)) continue;

        /* Validity gate */
        if (!(fix.flags & GPS_FIX_VALID) || !(fix.flags & GPS_HAS_LATLON)) continue;

#ifdef HW_VALIDATION_TEST
        /* Skip filter in validation mode (stationary device) */
        if (!got_first_fix) {
            got_first_fix = true;
            write_window_start = hal_time_ms();
            printf("*** FIRST FIX! lat=%.6f lon=%.6f sats=%d ***\n",
                   fix.latitude, fix.longitude, fix.satellites);
            printf("Starting 30s write window...\n");
        }
#else
        /* Filter: reject stationary and outlier fixes */
        if (gps_filter_process(&filter, &fix) != FILTER_ACCEPT) continue;
#endif

        /* Store */
        data_storage_write_fix(&storage, &fix);

#ifdef HW_VALIDATION_TEST
        fix_count++;
        printf("FIX #%lu: %.6f,%.6f sats=%d\n",
               (unsigned long)fix_count, fix.latitude, fix.longitude, fix.satellites);
#endif
    }
}

#endif /* !HOST_BUILD */
