#ifndef HOST_BUILD

#include "nmea_parser.h"
#include "gps_filter.h"
#include "data_storage.h"
#include "power_mgmt.h"
#include "hal/hal.h"

#define GPS_BAUD_RATE 9600

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

    /* 6. Main loop */
    char line_buf[NMEA_MAX_SENTENCE_LEN + 1];

    while (1) {
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

        /* Filter */
        if (gps_filter_process(&filter, &fix) != FILTER_ACCEPT) continue;

        /* Store */
        data_storage_write_fix(&storage, &fix);
    }
}

#endif /* !HOST_BUILD */
