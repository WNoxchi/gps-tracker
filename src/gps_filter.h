#ifndef GPS_FILTER_H
#define GPS_FILTER_H

#include "nmea_parser.h"

#define GPS_FILTER_STATIONARY_THRESHOLD_KMH 3.0f
#define GPS_FILTER_MAX_SPEED_KMH            250.0f

typedef enum {
    FILTER_STATE_COLD_START = 0,
    FILTER_STATE_MOVING,
    FILTER_STATE_STOPPED
} gps_filter_state_t;

typedef struct {
    gps_filter_state_t state;
    gps_fix_t last_accepted_fix;
    bool has_last_fix;
} gps_filter_t;

typedef enum {
    FILTER_ACCEPT = 0,
    FILTER_REJECT_INVALID,
    FILTER_REJECT_STATIONARY,
    FILTER_REJECT_OUTLIER,
    FILTER_REJECT_NO_TIME_DELTA
} gps_filter_result_t;

void               gps_filter_init(gps_filter_t* filter);
gps_filter_result_t gps_filter_process(gps_filter_t* filter, const gps_fix_t* fix);
gps_filter_state_t  gps_filter_get_state(const gps_filter_t* filter);

#endif
