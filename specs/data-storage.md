# Data Storage

Manage append-only CSV file writing to the SD card with power-loss tolerance.

## Overview

A C module (`data_storage.h` / `data_storage.c`) that manages writing GPS fixes to CSV files on a FAT32-formatted microSD card. Uses the FatFs library for file operations. Last stage of the on-device pipeline: `[GPS filter] → [data storage] → [SD card]`.

All file operations go through the HAL (see `specs/build-and-test.md`) so storage logic is testable on host with a mock filesystem.

## CSV Format

### Header Row

Every CSV file begins with exactly this header:
```
timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality
```

### Data Rows

One row per accepted GPS fix:

| Column | Format | Example | Source |
|--------|--------|---------|--------|
| timestamp | `YYYY-MM-DDTHH:MM:SSZ` (ISO 8601 UTC) | `2025-06-15T14:23:07Z` | RMC date + time |
| latitude | 6 decimal places | `47.285233` | GGA/RMC |
| longitude | 6 decimal places | `8.565265` | GGA/RMC |
| speed_kmh | 2 decimal places | `52.30` | RMC |
| altitude_m | 1 decimal place | `499.6` | GGA |
| course_deg | 1 decimal place | `77.5` | RMC |
| satellites | integer | `8` | GGA |
| hdop | 2 decimal places | `1.01` | GGA |
| fix_quality | integer | `1` | GGA |

- Missing fields (flag not set in `gps_fix_t`): empty (adjacent commas). Example: `2025-06-15T14:23:07Z,47.285233,8.565265,52.30,,77.5,8,1.01,1`
- Line ending: `\n` (LF only)
- No quoting, no escaping

### Row Size Estimate

~75-85 bytes per row. At 1Hz GPS, filter passing ~50% during driving: ~150 KB/hour. Over 10 years at 2hrs/day: ~2.1 GB. Well under FAT32's 4GB limit.

## File Naming and Rotation

- Primary file: `track.csv`
- After unclean shutdown: `track_1.csv`, `track_2.csv`, ..., `track_N.csv`
- Always write to the highest-numbered file (or `track.csv` if none exist)
- On new file creation: scan `track.csv` then `track_1.csv` through `track_999.csv`, find highest N, create `track_{N+1}.csv`
- If 999 files exist: halt with error (`STORAGE_ERR_TOO_MANY_FILES`)

## File Lifecycle

### 1. Startup Sequence

1. Mount FAT32 via `f_mount()`
2. Find most recent CSV file (highest numbered, or `track.csv`)
3. Check for `_dirty` marker file. If it exists → previous session did not shut down cleanly → start a new file
4. If no `_dirty` but last byte of CSV is NOT `\n` → also treat as unclean → start new file
5. If file ends with `\n` or is empty → append to it
6. If file has zero bytes → write CSV header first
7. If file has content and ends with `\n` → do NOT re-write header, append directly
8. Create `_dirty` marker file

### 2. Normal Operation (Append Loop)

1. Receive accepted `gps_fix_t` from filter
2. Format as CSV row via `snprintf`
3. `f_write()` to append
4. If `STORAGE_SYNC_INTERVAL_S` seconds elapsed since last sync → call `f_sync()`

### 3. Clean Shutdown

1. `f_sync()` — flush pending data
2. `f_close()` — close file handle
3. `f_unlink("_dirty")` — delete marker
4. `f_unmount()` — unmount filesystem

## API

```c
typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERR_MOUNT,
    STORAGE_ERR_OPEN,
    STORAGE_ERR_WRITE,
    STORAGE_ERR_SYNC,
    STORAGE_ERR_FULL,
    STORAGE_ERR_TOO_MANY_FILES
} storage_error_t;

typedef struct data_storage data_storage_t;

storage_error_t data_storage_init(data_storage_t* storage);
storage_error_t data_storage_write_fix(data_storage_t* storage, const gps_fix_t* fix);
storage_error_t data_storage_shutdown(data_storage_t* storage);
const char* data_storage_get_filename(const data_storage_t* storage);
```

## Constants

| Constant | Value | Rationale |
|---|---|---|
| `STORAGE_SYNC_INTERVAL_S` | 5 | Max 5s data loss on unclean shutdown, balances SD card wear |
| `STORAGE_MAX_FILE_NUMBER` | 999 | Practical scan limit |
| `STORAGE_DIRTY_FILENAME` | `"_dirty"` | Unclean shutdown marker |
| `STORAGE_BASE_FILENAME` | `"track"` | Base name for CSV files |
| `CSV_HEADER` | `"timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality\n"` | Fixed header |

## Acceptance Tests

| ID | Name | Given | Then |
|----|------|-------|------|
| T1 | fresh_start_creates_file | Empty filesystem, no `_dirty` | Creates `track.csv` with header. |
| T2 | append_to_clean_file | `track.csv` exists, ends with `\n`, no `_dirty` | Opens for appending. No new header. |
| T3 | dirty_flag_triggers_rotation | `track.csv` exists, `_dirty` exists | Creates `track_1.csv` with header. `_dirty` deleted. |
| T4 | incomplete_line_triggers_rotation | `track.csv` last byte != `\n`, no `_dirty` | Creates `track_1.csv`. |
| T5 | sequential_rotation | `track.csv` + `track_1.csv` exist, `_dirty` exists | Creates `track_2.csv`. |
| T6 | write_fix_csv_format | Write known fix | Line matches expected CSV format exactly. |
| T7 | write_fix_missing_altitude | Fix with `GPS_HAS_ALTITUDE` not set | Empty altitude field (adjacent commas). |
| T8 | sync_after_interval | Write fixes spanning 6s (mock time) | `f_sync` called at least once. |
| T9 | no_sync_before_interval | Write fixes spanning 3s | `f_sync` NOT called. |
| T10 | shutdown_sequence | Call `data_storage_shutdown` | `f_sync`, `f_close`, `f_unlink("_dirty")`, `f_unmount` in order. |
| T11 | write_after_shutdown | Write fix after shutdown | Returns `STORAGE_ERR_WRITE`. No crash. |
| T12 | max_files_error | 999 files exist, `_dirty` exists | Returns `STORAGE_ERR_TOO_MANY_FILES`. |
| T13 | csv_header_content | Fresh init | First line is exactly `CSV_HEADER`. |
| T14 | timestamp_format | Fix: day=15, month=6, year=2025, hour=14, min=23, sec=7 | Timestamp column: `2025-06-15T14:23:07Z` |
| T15 | coordinate_precision | Fix: lat=47.2852333333, lon=8.5652654321 | CSV: `47.285233` and `8.565265` (6 decimal places) |

## Cross-References

- Input: accepted `gps_fix_t` from `specs/gps-filtering.md`
- Shutdown triggered by `specs/power-management.md`
- File I/O through HAL from `specs/build-and-test.md`
