#ifndef HOST_BUILD

#include "nmea_parser.h"
#include "gps_filter.h"
#include "data_storage.h"
#include "power_mgmt.h"
#include "hal/hal.h"
#include <stdio.h>

#define GPS_BAUD_RATE 9600
#define HW_TEST_DURATION_MS 30000

int main(void) {
    /* 1. Initialize power management */
    power_mgmt_init();

    /* 2. Initialize UART for GPS */
    hal_uart_init(GPS_BAUD_RATE);

    /* 3. Initialize storage */
    data_storage_t storage;
    if (data_storage_init(&storage) != STORAGE_OK) {
        while (1) { /* halt */ }
    }

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
    /* Capture start time for 30-second validation window */
    uint32_t start_ms = hal_time_ms();
#endif

    /* 6. Main loop */
    char line_buf[NMEA_MAX_SENTENCE_LEN + 1];

    while (1) {
#ifdef HW_VALIDATION_TEST
        /* Check for 30-second window timeout */
        if (hal_time_ms() - start_ms > HW_TEST_DURATION_MS) {
            data_storage_shutdown(&storage);
            nmea_parser_destroy(parser);
            while (1) { /* halt, validation complete */ }
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

        /* Validity gate: check GPS_FIX_VALID and GPS_HAS_LATLON */
        if (!(fix.flags & GPS_FIX_VALID) || !(fix.flags & GPS_HAS_LATLON)) continue;

#ifndef HW_VALIDATION_TEST
        /* Filter (skip in validation mode, but validity gate already checked) */
        if (gps_filter_process(&filter, &fix) != FILTER_ACCEPT) continue;
#endif

        /* Store */
        data_storage_write_fix(&storage, &fix);

#ifdef HW_VALIDATION_TEST
        /* USB serial echo for real-time monitoring */
        printf("FIX: %.6f,%.6f\n", fix.latitude, fix.longitude);
#endif
    }
}

#endif /* !HOST_BUILD */
