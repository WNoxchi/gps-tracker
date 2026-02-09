#include "nmea_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_FIELDS 20
#define MAX_FIELD_LEN 16

struct nmea_parser {
    gps_fix_t current_fix;
    gps_fix_t completed_fix;
    bool has_completed_fix;
    bool has_gga;
    bool has_rmc;
    uint8_t epoch_hour;
    uint8_t epoch_minute;
    uint8_t epoch_second;
    uint8_t epoch_centisecond;
    bool epoch_started;
};

/* ---- Helpers ---- */

static bool validate_checksum(const char* sentence, size_t len) {
    if (len < 4) return false;
    if (sentence[0] != '$') return false;

    const char* star = NULL;
    for (size_t i = len; i > 0; i--) {
        if (sentence[i - 1] == '*') {
            star = &sentence[i - 1];
            break;
        }
    }
    if (!star) return false;
    if (star + 3 > sentence + len) return false;

    uint8_t calc = 0;
    for (const char* p = sentence + 1; p < star; p++) {
        calc ^= (uint8_t)*p;
    }

    char hex[3] = { star[1], star[2], '\0' };
    unsigned long expected = strtoul(hex, NULL, 16);
    return calc == (uint8_t)expected;
}

static int split_fields(const char* sentence, char fields[][MAX_FIELD_LEN], int max_fields) {
    /* Skip '$' prefix, stop at '*' */
    const char* start = sentence + 1;
    const char* star = strchr(start, '*');
    if (!star) return -1;

    int count = 0;
    const char* p = start;
    while (p < star && count < max_fields) {
        const char* comma = p;
        while (comma < star && *comma != ',') comma++;
        size_t flen = (size_t)(comma - p);
        if (flen >= MAX_FIELD_LEN) flen = MAX_FIELD_LEN - 1;
        memcpy(fields[count], p, flen);
        fields[count][flen] = '\0';
        count++;
        p = (comma < star) ? comma + 1 : star;
    }
    return count;
}

static bool parse_time(const char* field, uint8_t* hour, uint8_t* minute,
                       uint8_t* second, uint8_t* centisecond) {
    if (strlen(field) < 6) return false;
    *hour = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    *minute = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    *second = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
    *centisecond = 0;
    if (field[6] == '.' && field[7] != '\0') {
        int cs = 0;
        if (field[7] >= '0' && field[7] <= '9') {
            cs = (field[7] - '0') * 10;
            if (field[8] >= '0' && field[8] <= '9') {
                cs += (field[8] - '0');
            }
        }
        *centisecond = (uint8_t)cs;
    }
    return true;
}

static bool parse_date(const char* field, uint8_t* day, uint8_t* month, uint16_t* year) {
    if (strlen(field) < 6) return false;
    *day   = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    *month = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    int yy = (field[4] - '0') * 10 + (field[5] - '0');
    *year  = (uint16_t)(2000 + yy);
    return true;
}

static bool parse_coordinate(const char* coord, const char* hemisphere, double* out) {
    if (coord[0] == '\0' || hemisphere[0] == '\0') return false;

    double raw = strtod(coord, NULL);
    int degrees;
    double minutes;

    if (hemisphere[0] == 'N' || hemisphere[0] == 'S') {
        /* Latitude: DDmm.mmmm */
        degrees = (int)(raw / 100.0);
        minutes = raw - degrees * 100.0;
    } else {
        /* Longitude: DDDmm.mmmm */
        degrees = (int)(raw / 100.0);
        minutes = raw - degrees * 100.0;
    }

    *out = degrees + minutes / 60.0;
    if (hemisphere[0] == 'S' || hemisphere[0] == 'W') {
        *out = -(*out);
    }
    return true;
}

static bool times_match(uint8_t h1, uint8_t m1, uint8_t s1, uint8_t cs1,
                        uint8_t h2, uint8_t m2, uint8_t s2, uint8_t cs2) {
    return h1 == h2 && m1 == m2 && s1 == s2 && cs1 == cs2;
}

/* ---- Epoch management ---- */

static void start_new_epoch(nmea_parser_t* parser, uint8_t h, uint8_t m,
                            uint8_t s, uint8_t cs) {
    /* If we had a previous epoch, emit it */
    if (parser->epoch_started) {
        parser->completed_fix = parser->current_fix;
        parser->has_completed_fix = true;
    }
    memset(&parser->current_fix, 0, sizeof(gps_fix_t));
    parser->has_gga = false;
    parser->has_rmc = false;
    parser->epoch_hour = h;
    parser->epoch_minute = m;
    parser->epoch_second = s;
    parser->epoch_centisecond = cs;
    parser->epoch_started = true;
}

/* ---- GGA parsing ---- */

static void parse_gga(nmea_parser_t* parser, char fields[][MAX_FIELD_LEN], int nfields) {
    if (nfields < 10) return;

    uint8_t h, m, s, cs;
    if (!parse_time(fields[1], &h, &m, &s, &cs)) return;

    if (!parser->epoch_started ||
        !times_match(parser->epoch_hour, parser->epoch_minute,
                     parser->epoch_second, parser->epoch_centisecond, h, m, s, cs)) {
        start_new_epoch(parser, h, m, s, cs);
    }

    gps_fix_t* fix = &parser->current_fix;
    fix->hour = h;
    fix->minute = m;
    fix->second = s;
    fix->centisecond = cs;
    fix->flags |= GPS_HAS_TIME;

    /* Fix quality */
    if (fields[6][0] != '\0') {
        fix->fix_quality = (uint8_t)(fields[6][0] - '0');
    }

    /* Satellites */
    if (fields[7][0] != '\0') {
        fix->satellites = (uint8_t)atoi(fields[7]);
    }

    /* HDOP */
    if (fields[8][0] != '\0') {
        fix->hdop = (float)strtod(fields[8], NULL);
        fix->flags |= GPS_HAS_HDOP;
    }

    /* Altitude */
    if (fields[9][0] != '\0') {
        fix->altitude_m = (float)strtod(fields[9], NULL);
        fix->flags |= GPS_HAS_ALTITUDE;
    }

    /* Lat/Lon */
    double lat, lon;
    if (parse_coordinate(fields[2], fields[3], &lat) &&
        parse_coordinate(fields[4], fields[5], &lon)) {
        fix->latitude = lat;
        fix->longitude = lon;
        fix->flags |= GPS_HAS_LATLON;
    }

    /* Set valid if fix quality >= 1 */
    if (fix->fix_quality >= 1) {
        fix->flags |= GPS_FIX_VALID;
    } else {
        fix->flags &= ~GPS_FIX_VALID;
    }

    parser->has_gga = true;
}

/* ---- RMC parsing ---- */

static void parse_rmc(nmea_parser_t* parser, char fields[][MAX_FIELD_LEN], int nfields) {
    if (nfields < 10) return;

    uint8_t h, m, s, cs;
    if (!parse_time(fields[1], &h, &m, &s, &cs)) return;

    if (!parser->epoch_started ||
        !times_match(parser->epoch_hour, parser->epoch_minute,
                     parser->epoch_second, parser->epoch_centisecond, h, m, s, cs)) {
        start_new_epoch(parser, h, m, s, cs);
    }

    gps_fix_t* fix = &parser->current_fix;
    fix->hour = h;
    fix->minute = m;
    fix->second = s;
    fix->centisecond = cs;
    fix->flags |= GPS_HAS_TIME;

    /* Status: A=valid, V=void */
    bool active = (fields[2][0] == 'A');
    if (!active) {
        fix->flags &= ~GPS_FIX_VALID;
    }

    /* Lat/Lon (RMC fields 3-6) */
    if (!parser->has_gga) {
        double lat, lon;
        if (parse_coordinate(fields[3], fields[4], &lat) &&
            parse_coordinate(fields[5], fields[6], &lon)) {
            fix->latitude = lat;
            fix->longitude = lon;
            fix->flags |= GPS_HAS_LATLON;
        }
    }

    /* Speed */
    if (fields[7][0] != '\0') {
        double knots = strtod(fields[7], NULL);
        fix->speed_kmh = (float)(knots * KNOTS_TO_KMH);
        fix->flags |= GPS_HAS_SPEED;
    }

    /* Course */
    if (fields[8][0] != '\0') {
        fix->course_deg = (float)strtod(fields[8], NULL);
        fix->flags |= GPS_HAS_COURSE;
    }

    /* Date (field 9) */
    if (nfields > 9 && fields[9][0] != '\0') {
        uint8_t day, month;
        uint16_t year;
        if (parse_date(fields[9], &day, &month, &year)) {
            fix->day = day;
            fix->month = month;
            fix->year = year;
            fix->flags |= GPS_HAS_DATE;
        }
    }

    /* Validity: need both GGA fix_quality >= 1 AND RMC status 'A' */
    if (active && parser->has_gga && fix->fix_quality >= 1) {
        fix->flags |= GPS_FIX_VALID;
    } else if (!active) {
        fix->flags &= ~GPS_FIX_VALID;
    }

    parser->has_rmc = true;
}

/* ---- Public API ---- */

nmea_parser_t* nmea_parser_create(void) {
    nmea_parser_t* p = calloc(1, sizeof(nmea_parser_t));
    return p;
}

void nmea_parser_destroy(nmea_parser_t* parser) {
    free(parser);
}

nmea_result_t nmea_parser_feed(nmea_parser_t* parser, const char* sentence) {
    if (!parser || !sentence) return NMEA_RESULT_ERROR;

    size_t len = strlen(sentence);
    /* Strip trailing CR/LF */
    while (len > 0 && (sentence[len - 1] == '\r' || sentence[len - 1] == '\n')) {
        len--;
    }
    if (len == 0) return NMEA_RESULT_ERROR;
    if (sentence[0] != '$') return NMEA_RESULT_ERROR;

    /* Work with a mutable copy */
    char buf[NMEA_MAX_SENTENCE_LEN + 1];
    if (len > NMEA_MAX_SENTENCE_LEN) len = NMEA_MAX_SENTENCE_LEN;
    memcpy(buf, sentence, len);
    buf[len] = '\0';

    if (!validate_checksum(buf, len)) return NMEA_RESULT_ERROR;

    /* Determine sentence type from positions 3-5 after '$' */
    if (len < 6) return NMEA_RESULT_ERROR;
    char type[4] = { buf[3], buf[4], buf[5], '\0' };

    char fields[MAX_FIELDS][MAX_FIELD_LEN];
    memset(fields, 0, sizeof(fields));
    int nfields = split_fields(buf, fields, MAX_FIELDS);
    if (nfields < 0) return NMEA_RESULT_ERROR;

    /* Reset completed fix flag before processing */
    parser->has_completed_fix = false;

    if (strcmp(type, "GGA") == 0) {
        parse_gga(parser, fields, nfields);
    } else if (strcmp(type, "RMC") == 0) {
        parse_rmc(parser, fields, nfields);
    } else {
        return NMEA_RESULT_NONE;
    }

    if (parser->has_completed_fix) {
        return NMEA_RESULT_FIX_READY;
    }
    return NMEA_RESULT_NONE;
}

bool nmea_parser_get_fix(nmea_parser_t* parser, gps_fix_t* out_fix) {
    if (!parser || !out_fix) return false;
    if (!parser->has_completed_fix) return false;
    *out_fix = parser->completed_fix;
    parser->has_completed_fix = false;
    return true;
}
