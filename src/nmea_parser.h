#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#define NMEA_MAX_SENTENCE_LEN 82
#define KNOTS_TO_KMH          1.852

#define GPS_FIX_VALID    (1 << 0)
#define GPS_HAS_TIME     (1 << 1)
#define GPS_HAS_DATE     (1 << 2)
#define GPS_HAS_LATLON   (1 << 3)
#define GPS_HAS_ALTITUDE (1 << 4)
#define GPS_HAS_SPEED    (1 << 5)
#define GPS_HAS_COURSE   (1 << 6)
#define GPS_HAS_HDOP     (1 << 7)

typedef struct {
    uint32_t flags;

    uint8_t hour, minute, second, centisecond;
    uint8_t day, month;
    uint16_t year;

    double latitude;
    double longitude;
    float altitude_m;
    float speed_kmh;
    float course_deg;
    uint8_t fix_quality;
    uint8_t satellites;
    float hdop;
} gps_fix_t;

typedef struct nmea_parser nmea_parser_t;

typedef enum {
    NMEA_RESULT_NONE      =  0,
    NMEA_RESULT_FIX_READY =  1,
    NMEA_RESULT_ERROR     = -1
} nmea_result_t;

nmea_parser_t* nmea_parser_create(void);
void           nmea_parser_destroy(nmea_parser_t* parser);
nmea_result_t  nmea_parser_feed(nmea_parser_t* parser, const char* sentence);
bool           nmea_parser_get_fix(nmea_parser_t* parser, gps_fix_t* out_fix);

#endif
