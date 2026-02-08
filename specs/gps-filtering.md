# GPS Data Filter

Reject stationary points and impossible GPS jumps from the fix stream in real-time.

## Overview

A C module (`gps_filter.h` / `gps_filter.c`) that takes `gps_fix_t` records one at a time and returns a keep/reject decision. Pure logic — no hardware dependencies, no file I/O, no allocations. Pipeline position: `[NMEA parser] → [GPS filter] → [CSV storage]`.

## Filter Pipeline

A fix must pass ALL checks sequentially to be accepted.

### 1. Validity Gate

- If `flags & GPS_FIX_VALID == 0` → reject (`FILTER_REJECT_INVALID`)
- If `flags & GPS_HAS_LATLON == 0` → reject (`FILTER_REJECT_INVALID`)

### 2. Stationary Rejection

- GPS speed (`speed_kmh`) below `GPS_FILTER_STATIONARY_THRESHOLD_KMH` → fix is "stationary"
- Stationary fixes are rejected, except: the first fix after transitioning from MOVING to STOPPED is kept (the "stop point")
- If `GPS_HAS_SPEED` is NOT set → treat as stationary
- Constant: **`GPS_FILTER_STATIONARY_THRESHOLD_KMH = 3.0`** (GPS jitter is 0.5-2.5 km/h when still)

### 3. Speed Gate (Outlier Rejection)

- Calculate implied speed: `haversine_distance(prev, curr) / time_delta`
- If `implied_speed > GPS_FILTER_MAX_SPEED_KMH` → reject (`FILTER_REJECT_OUTLIER`)
- "Previous" means last **accepted** fix, not last fed fix
- First fix ever (no previous) → skip this check
- Time delta zero or negative → reject (`FILTER_REJECT_NO_TIME_DELTA`)
- Time delta < 0.5 seconds → skip this check (avoid division by near-zero)
- Constant: **`GPS_FILTER_MAX_SPEED_KMH = 250.0`**

## Haversine Distance

Utility function in `src/lib/geo_utils.h` / `src/lib/geo_utils.c` (shared across modules):

```c
double haversine_distance_m(double lat1, double lon1, double lat2, double lon2);
```

Formula:
```
a = sin(dlat/2)^2 + cos(lat1) * cos(lat2) * sin(dlon/2)^2
c = 2 * atan2(sqrt(a), sqrt(1-a))
distance = R * c
```

Where `R = 6371000.0` meters (Earth mean radius). Inputs in decimal degrees, converted to radians internally.

## State Machine

```
                     +----------+
   first valid   --> | MOVING   |--- speed < 3 km/h (store stop point) --> STOPPED
   moving fix        +----------+                                           |
        ^                 ^                                                 |
        |                 +--- speed >= 3 km/h (store resume point) --------+
        |
   +------------+
   | COLD_START |<--- power on / gps_filter_init()
   +------------+
```

- **COLD_START**: No previous fix. Accept first valid, non-stationary fix → transition to MOVING.
- **MOVING**: Accept all fixes passing the speed gate. Speed drops below threshold → store stop point, transition to STOPPED.
- **STOPPED**: Reject all fixes. Speed rises above threshold → store resume point, transition to MOVING.

## API

```c
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

void gps_filter_init(gps_filter_t* filter);
gps_filter_result_t gps_filter_process(gps_filter_t* filter, const gps_fix_t* fix);
gps_filter_state_t gps_filter_get_state(const gps_filter_t* filter);
```

## Constants

| Constant | Value | Unit | Rationale |
|---|---|---|---|
| `GPS_FILTER_STATIONARY_THRESHOLD_KMH` | 3.0 | km/h | Above GPS jitter, below walking speed |
| `GPS_FILTER_MAX_SPEED_KMH` | 250.0 | km/h | Above any production car speed |
| `EARTH_RADIUS_M` | 6371000.0 | meters | WGS84 mean radius |

## Acceptance Tests

| ID | Name | Given | Then |
|----|------|-------|------|
| T1 | reject_invalid_fix | Fix with `GPS_FIX_VALID` not set | `FILTER_REJECT_INVALID` |
| T2 | reject_no_position | Valid flag set but `GPS_HAS_LATLON` not set | `FILTER_REJECT_INVALID` |
| T3 | accept_first_moving_fix | COLD_START, speed 50 km/h | `FILTER_ACCEPT`. State → MOVING. |
| T4 | reject_stationary_cold_start | COLD_START, speed 1.5 km/h | `FILTER_REJECT_STATIONARY`. State stays COLD_START. |
| T5 | accept_moving_fix | MOVING, new fix 100m away, 1s later, 40 km/h | `FILTER_ACCEPT` |
| T6 | reject_outlier | MOVING, prev at (47.0, 8.0), new at (48.0, 8.0), 1s later | `FILTER_REJECT_OUTLIER` (implied ~400,000 km/h) |
| T7 | moving_to_stopped | MOVING, speed 1.0 km/h | `FILTER_ACCEPT` (stop point). State → STOPPED. |
| T8 | reject_while_stopped | STOPPED, speed 0.5 km/h | `FILTER_REJECT_STATIONARY` |
| T9 | stopped_to_moving | STOPPED, speed 15 km/h | `FILTER_ACCEPT` (resume point). State → MOVING. |
| T10 | haversine_known_distance | (47.3769, 8.5417) to (46.9480, 7.4474) | ~90,100m (±500m) |
| T11 | haversine_zero_distance | Same coords both points | 0.0m (±0.1) |
| T12 | haversine_antipodal | (0,0) to (0,180) | ~20,015,087m (±1000m) |
| T13 | speed_gate_skipped_first | COLD_START, no prev, speed 100 km/h | Speed gate skipped. `FILTER_ACCEPT`. |
| T14 | reject_zero_time_delta | Two fixes, identical timestamps, different positions | `FILTER_REJECT_NO_TIME_DELTA` |
| T15 | missing_speed_stationary | Fix with `GPS_HAS_SPEED` not set | Treated as stationary. |
| T16 | realistic_driving_sequence | Cold start → 3 stationary → accelerate → 5 moving → decelerate → 3 stationary → accelerate → 3 moving | Accepted: first moving, 5 moving, stop point, resume point, 3 moving. Rejected: cold start stationary, middle stationary. |

## Cross-References

- Input: `gps_fix_t` from `specs/nmea-parser.md`
- Output: accepted fixes go to `specs/data-storage.md`
- `haversine_distance_m()` in `src/lib/geo_utils.c` is shared
