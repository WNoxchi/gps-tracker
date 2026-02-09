#ifndef HOST_BUILD

#include "nmea_parser.h"
#include "gps_filter.h"
#include "data_storage.h"
#include "power_mgmt.h"
#include "hal/hal.h"
#include <stdio.h>
#include "pico/stdlib.h"

#define GPS_BAUD_RATE 9600
#define HW_TEST_DURATION_MS 300000

int main(void) {
    stdio_init_all();

#ifdef HW_VALIDATION_TEST
    /* Wait for USB serial connection */
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
    bool storage_ok = false;
    storage_error_t serr = data_storage_init(&storage);
#ifdef HW_VALIDATION_TEST
    if (serr != STORAGE_OK) {
        printf("WARN: storage init failed (code %d) — continuing without SD\n", serr);
        printf("  1=MOUNT 2=OPEN 3=WRITE 4=SYNC 5=FULL 6=TOO_MANY\n");
    } else {
        printf("Storage OK, file: %s\n", data_storage_get_filename(&storage));
        storage_ok = true;
    }
#else
    if (serr != STORAGE_OK) {
        while (1) { /* halt */ }
    }
    storage_ok = true;
#endif

    /* 4. Initialize NMEA parser */
    nmea_parser_t* parser = nmea_parser_create();

    /* 5. Initialize GPS filter (COLD_START) */
    gps_filter_t filter;
    gps_filter_init(&filter);

#ifdef HW_VALIDATION_TEST
    uint32_t start_ms = hal_time_ms();
    uint32_t nmea_count = 0;
#endif

    /* 6. Main loop */
    char line_buf[NMEA_MAX_SENTENCE_LEN + 1];

    while (1) {
#ifdef HW_VALIDATION_TEST
        if (hal_time_ms() - start_ms > HW_TEST_DURATION_MS) {
            printf("\n--- 30s window complete ---\n");
            printf("NMEA sentences received: %lu\n", (unsigned long)nmea_count);
            if (storage_ok) {
                data_storage_shutdown(&storage);
                printf("Storage shutdown OK\n");
            }
            if (parser) nmea_parser_destroy(parser);
            while (1) { /* halt */ }
        }
#endif

        if (power_mgmt_is_shutdown_requested()) {
            if (storage_ok) data_storage_shutdown(&storage);
            if (parser) nmea_parser_destroy(parser);
            while (1) { /* halt */ }
        }

        int len = hal_uart_read_line(line_buf, sizeof(line_buf), 1100);
        if (len <= 0) continue;

#ifdef HW_VALIDATION_TEST
        nmea_count++;
        printf("NMEA: %s\n", line_buf);
#endif

        if (!parser) continue;
        nmea_result_t result = nmea_parser_feed(parser, line_buf);
        if (result != NMEA_RESULT_FIX_READY) continue;

        gps_fix_t fix;
        if (!nmea_parser_get_fix(parser, &fix)) continue;

        if (!(fix.flags & GPS_FIX_VALID) || !(fix.flags & GPS_HAS_LATLON)) continue;

#ifndef HW_VALIDATION_TEST
        if (gps_filter_process(&filter, &fix) != FILTER_ACCEPT) continue;
#endif

        if (storage_ok) {
            data_storage_write_fix(&storage, &fix);
#ifdef HW_VALIDATION_TEST
            printf("FIX WRITTEN: %.6f,%.6f\n", fix.latitude, fix.longitude);
#endif
        }
    }
}

#endif /* !HOST_BUILD */
