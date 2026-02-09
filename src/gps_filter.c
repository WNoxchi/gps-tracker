#include "gps_filter.h"
#include "geo_utils.h"
#include <string.h>

void gps_filter_init(gps_filter_t* filter) {
    if (!filter) return;
    memset(filter, 0, sizeof(gps_filter_t));
    filter->state = FILTER_STATE_COLD_START;
    filter->has_last_fix = false;
}

static double fix_to_epoch_seconds(const gps_fix_t* fix) {
    double s = 0.0;
    if (fix->flags & GPS_HAS_DATE) {
        /* Approximate days since epoch for comparison — exact value not needed,
           just consistent offsets between fixes */
        s += (double)fix->year * 365.25 * 86400.0;
        s += (double)fix->month * 30.44 * 86400.0;
        s += (double)fix->day * 86400.0;
    }
    s += (double)fix->hour * 3600.0;
    s += (double)fix->minute * 60.0;
    s += (double)fix->second;
    s += (double)fix->centisecond / 100.0;
    return s;
}

static bool is_stationary(const gps_fix_t* fix) {
    if (!(fix->flags & GPS_HAS_SPEED)) return true;
    return fix->speed_kmh < GPS_FILTER_STATIONARY_THRESHOLD_KMH;
}

gps_filter_result_t gps_filter_process(gps_filter_t* filter, const gps_fix_t* fix) {
    if (!filter || !fix) return FILTER_REJECT_INVALID;

    /* 1. Validity gate */
    if (!(fix->flags & GPS_FIX_VALID)) return FILTER_REJECT_INVALID;
    if (!(fix->flags & GPS_HAS_LATLON)) return FILTER_REJECT_INVALID;

    bool stationary = is_stationary(fix);

    /* 2. State machine + stationary rejection */
    switch (filter->state) {
    case FILTER_STATE_COLD_START:
        if (stationary) return FILTER_REJECT_STATIONARY;
        /* First valid, non-stationary fix — accept and go to MOVING */
        filter->state = FILTER_STATE_MOVING;
        filter->last_accepted_fix = *fix;
        filter->has_last_fix = true;
        return FILTER_ACCEPT;

    case FILTER_STATE_MOVING:
        /* 3. Speed gate (outlier rejection) — only if we have a previous fix */
        if (filter->has_last_fix) {
            double dt = fix_to_epoch_seconds(fix) - fix_to_epoch_seconds(&filter->last_accepted_fix);
            if (dt < 0.0) return FILTER_REJECT_NO_TIME_DELTA;
            if (dt == 0.0) return FILTER_REJECT_NO_TIME_DELTA;
            if (dt >= 0.5) {
                double dist = haversine_distance_m(
                    filter->last_accepted_fix.latitude,
                    filter->last_accepted_fix.longitude,
                    fix->latitude, fix->longitude);
                double implied_kmh = (dist / dt) * 3.6;
                if (implied_kmh > GPS_FILTER_MAX_SPEED_KMH) {
                    return FILTER_REJECT_OUTLIER;
                }
            }
        }

        if (stationary) {
            /* Stop point: accept this fix, transition to STOPPED */
            filter->state = FILTER_STATE_STOPPED;
            filter->last_accepted_fix = *fix;
            return FILTER_ACCEPT;
        }

        /* Normal moving fix */
        filter->last_accepted_fix = *fix;
        return FILTER_ACCEPT;

    case FILTER_STATE_STOPPED:
        if (!stationary) {
            /* Resume point: accept and transition to MOVING */
            filter->state = FILTER_STATE_MOVING;
            filter->last_accepted_fix = *fix;
            return FILTER_ACCEPT;
        }
        return FILTER_REJECT_STATIONARY;
    }

    return FILTER_REJECT_INVALID;
}

gps_filter_state_t gps_filter_get_state(const gps_filter_t* filter) {
    if (!filter) return FILTER_STATE_COLD_START;
    return filter->state;
}
