# NMEA 0183 Parser

Parse NMEA 0183 GGA and RMC sentences into a structured `gps_fix_t` type.

## Overview

A C module (`nmea_parser.h` / `nmea_parser.c`) that accepts raw NMEA sentence strings one at a time and produces structured `gps_fix_t` records.

Pure logic — no hardware dependencies. Takes `const char*` input, returns structured data. This module is the first stage of the pipeline: `[UART bytes] → [line assembly] → [NMEA parser] → [gps_fix_t]`.

## Input Format

### Supported Sentence Types

Only two sentence types are parsed. All others (GSV, GSA, VTG, GLL, TXT, etc.) are silently ignored.

**GPGGA — Global Positioning System Fix Data**
```
$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*56
```

| Field | Index | Description |
|-------|-------|-------------|
| Time | 1 | HHMMSS.ss UTC |
| Lat | 2 | DDmm.mmmm |
| N/S | 3 | N or S |
| Lon | 4 | DDDmm.mmmm |
| E/W | 5 | E or W |
| Fix Quality | 6 | 0=no fix, 1=GPS, 2=DGPS |
| Satellites | 7 | Number of satellites in use |
| HDOP | 8 | Horizontal dilution of precision |
| Altitude | 9 | Altitude above MSL in meters |
| Alt Unit | 10 | Always M |

**GPRMC — Recommended Minimum Navigation Information**
```
$GPRMC,092725.00,A,4717.11399,N,00833.91590,E,0.004,77.52,091202,,,A*57
```

| Field | Index | Description |
|-------|-------|-------------|
| Time | 1 | HHMMSS.ss UTC |
| Status | 2 | A=active/valid, V=void |
| Lat | 3 | DDmm.mmmm |
| N/S | 4 | N or S |
| Lon | 5 | DDDmm.mmmm |
| E/W | 6 | E or W |
| Speed | 7 | Speed over ground in knots |
| Course | 8 | Track angle in degrees true |
| Date | 9 | DDMMYY |

### Talker IDs

The NEO-8M uses prefixes: GP (GPS), GN (multi-GNSS), GL (GLONASS), GA (Galileo), GB (BeiDou). The parser must accept any two-character talker ID. Sentence type is determined by characters at positions 3-5 after the `$` (i.e., GGA or RMC).

## Processing Rules

### Line Assembly (caller responsibility)

The parser receives complete null-terminated strings, one sentence per call. Line assembly from a UART byte stream is NOT part of this module. Maximum sentence length: 82 characters (NMEA spec).

### Checksum Validation

- Every sentence ends with `*HH` where HH is a two-hex-digit checksum.
- Checksum = XOR of all ASCII bytes between `$` and `*` (exclusive of both).
- Invalid checksum → silently discard (return error).
- Missing `*HH` suffix or `$` prefix → silently discard.

### Coordinate Conversion

NMEA encodes latitude as `DDmm.mmmm` and longitude as `DDDmm.mmmm`.

Convert to decimal degrees: `DD + (mm.mmmm / 60.0)`

Hemisphere sign: N and E are positive, S and W are negative. Store as `double`, output to CSV at 6 decimal places (~0.11m precision).

Examples:
- `4717.11399,N` → `47.285233`
- `00833.91590,E` → `8.565265`
- `3402.5432,S` → `-34.042387`
- `11832.1099,W` → `-118.535165`

### Speed Conversion

RMC speed in knots → km/h: `speed_kmh = speed_knots * 1.852`

Example: `5.400` knots → `10.00` km/h

### Fix Correlation

GGA and RMC with the same UTC time belong to the same fix epoch.

- When a GGA arrives: populate fix quality, satellite count, altitude, HDOP, lat/lon, time.
- When a RMC arrives with the same time: populate date, speed, course, validity status.
- When a new time is seen, the previous epoch is "complete". Emit the previous fix and start a new epoch.
- A fix is valid only when: GGA fix_quality >= 1 AND RMC status == 'A'.
- If only one of GGA/RMC was received for an epoch, emit with available data and sentinel values for missing fields.
- The NEO-8M typically emits GGA before RMC each epoch, but the parser must not depend on this ordering.

### No-Fix Handling

GGA fix_quality == 0 or RMC status == 'V' → fix is marked `valid = false`. Invalid fixes are still emitted but flagged.

### Robustness

The parser must not crash, assert, or produce undefined behavior on: truncated sentences, empty strings, non-NMEA text, missing fields, null pointer input, or unexpected characters. All invalid input is silently discarded. No `printf`, no `assert`, no side effects.

## Output Data Structure

```c
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

    double latitude;        // Decimal degrees, negative = South
    double longitude;       // Decimal degrees, negative = West
    float altitude_m;       // Meters above MSL
    float speed_kmh;        // Converted from knots
    float course_deg;       // Degrees true
    uint8_t fix_quality;    // 0=none, 1=GPS, 2=DGPS
    uint8_t satellites;
    float hdop;
} gps_fix_t;
```

## API

```c
typedef struct nmea_parser nmea_parser_t;

typedef enum {
    NMEA_RESULT_NONE = 0,
    NMEA_RESULT_FIX_READY = 1,
    NMEA_RESULT_ERROR = -1
} nmea_result_t;

nmea_parser_t* nmea_parser_create(void);
void nmea_parser_destroy(nmea_parser_t* parser);

// Feed one complete NMEA sentence. Returns FIX_READY when a complete epoch is available.
nmea_result_t nmea_parser_feed(nmea_parser_t* parser, const char* sentence);

// Retrieve the most recently completed fix. Returns false if none available.
bool nmea_parser_get_fix(nmea_parser_t* parser, gps_fix_t* out_fix);
```

## Constants

| Constant | Value | Rationale |
|---|---|---|
| `NMEA_MAX_SENTENCE_LEN` | 82 | NMEA 0183 standard |
| `KNOTS_TO_KMH` | 1.852 | Standard conversion |

## Acceptance Tests

| ID | Name | Given | Then |
|----|------|-------|------|
| T1 | valid_gga_rmc_pair | GGA + RMC with same time, valid fix | `FIX_READY` on second call. `GPS_FIX_VALID` set. All fields populated. |
| T2 | coordinate_north_east | GGA with `4717.11399,N,00833.91590,E` | lat == 47.285233 (±0.000001), lon == 8.565265 (±0.000001) |
| T3 | coordinate_south_west | GGA with `3402.5432,S,11832.1099,W` | lat == -34.042387 (±0.000001), lon == -118.535165 (±0.000001) |
| T4 | speed_conversion | RMC with speed `5.400` | speed_kmh == 10.00 (±0.01) |
| T5 | no_fix_gga | GGA with fix_quality=0 | `GPS_FIX_VALID` NOT set |
| T6 | no_fix_rmc | RMC with status='V' | `GPS_FIX_VALID` NOT set |
| T7 | bad_checksum | Sentence with wrong `*HH` | Returns `NMEA_RESULT_ERROR` |
| T8 | missing_checksum | Sentence without `*` | Returns `NMEA_RESULT_ERROR` |
| T9 | truncated_sentence | `$GPGGA,09272` | Returns `NMEA_RESULT_ERROR`. No crash. |
| T10 | empty_string | `""` | Returns `NMEA_RESULT_ERROR`. No crash. |
| T11 | null_pointer | `NULL` | Returns `NMEA_RESULT_ERROR`. No crash. |
| T12 | garbage_input | `"hello world\n"` | Returns `NMEA_RESULT_ERROR`. No crash. |
| T13 | ignored_sentences | Valid GSV, GSA, VTG with correct checksums | Returns `NMEA_RESULT_NONE` for each. |
| T14 | missing_optional_fields | RMC with empty course, GGA with empty altitude | Fix emitted. `GPS_HAS_COURSE` and `GPS_HAS_ALTITUDE` NOT set. |
| T15 | mixed_talker_ids | `$GPGGA,...` + `$GNRMC,...` same time | Correlated into single fix. |
| T16 | rmc_date_parsing | RMC date `091202` | day=9, month=12, year=2002. `GPS_HAS_DATE` set. |
| T17 | sequential_fixes | Three complete GGA+RMC pairs, different times | Three fixes produced with correct independent data. |
| T18 | empty_position_fields | GGA with all empty position fields (cold start) | `GPS_HAS_LATLON` NOT set. |

## Cross-References

- `gps_fix_t` is the input to `specs/gps-filtering.md`
- No hardware dependencies. UART line assembly is the caller's responsibility (see `specs/build-and-test.md` HAL).
