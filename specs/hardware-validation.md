# Hardware Validation Test

Temporary build mode for validating the GPS-to-SD-card pipeline on real hardware **without a supercapacitor**.

## Problem

The supercapacitor (1F, 5.5V) that protects the SD card filesystem during power loss is not yet installed. Pulling USB power mid-write could corrupt the FAT32 filesystem. A safe, time-bounded test mode is needed to validate the hardware pipeline end-to-end.

## Approach

A compile-time flag `HW_VALIDATION_TEST` enables three behaviors:

1. **30-second write window** — after boot, GPS data is written to the SD card for 30 seconds, then a clean shutdown is performed (sync, close, delete `_dirty`, unmount). The device halts in a safe state. The user can unplug at any time after 30 seconds.

2. **Stationary filter bypass** — the GPS filter's 3.0 km/h stationary threshold (`GPS_FILTER_STATIONARY_THRESHOLD_KMH`) rejects all fixes from a non-moving device. In validation mode, the `gps_filter_process()` call is skipped entirely so that stationary GPS fixes are written to the CSV. The validity gate (requiring `GPS_FIX_VALID` and `GPS_HAS_LATLON`) is still checked in `main.c` before writing — the filter module itself is not modified.

3. **USB serial echo** — each CSV line written to the SD card is also printed over USB serial, allowing real-time monitoring from the laptop. This is enabled by adding `pico_enable_stdio_usb(gps_tracker 1)` in CMake when `HW_VALIDATION_TEST` is set.

## Test Procedure

1. Build with `HW_VALIDATION_TEST`:
   ```bash
   cd build/pico
   cmake ../.. -DBUILD_FOR_PICO=ON -DBUILD_TESTS=OFF -DHW_VALIDATION_TEST=ON
   cmake --build .
   ```
2. Flash `gps_tracker.uf2` to the Pico 2 (hold BOOTSEL, plug in USB, drag `.uf2` to the drive).
3. Connect GPS module (UART1 on GP4/GP5) and SD card module (SPI0 on GP16-GP19).
4. Open a serial terminal on the laptop to monitor output:
   ```bash
   cat /dev/tty.usbmodem* | tee captured.csv
   ```
5. Power on the device (plug in USB). The device will:
   - Initialize storage (mount SD, open CSV file)
   - Read GPS sentences from UART
   - Parse NMEA, skip filtering, write every valid fix to the CSV
   - Echo each CSV line over USB serial
   - After 30 seconds, perform a clean shutdown and halt
6. Wait at least 30 seconds. The serial output will stop.
7. Unplug the device. Remove the SD card and verify `track.csv` on the laptop.

## Changes Required

### `CMakeLists.txt`

- Add `option(HW_VALIDATION_TEST "Hardware validation test mode" OFF)`
- When `HW_VALIDATION_TEST` is `ON` and `BUILD_FOR_PICO` is `ON`:
  - `target_compile_definitions(gps_tracker_lib PUBLIC HW_VALIDATION_TEST=1)`
  - `pico_enable_stdio_usb(gps_tracker 1)`

### `src/main.c`

All changes are guarded by `#ifdef HW_VALIDATION_TEST`.

**Timer shutdown** — record `hal_time_ms()` after init. In the main loop, check elapsed time before the power-loss check:

```c
#ifdef HW_VALIDATION_TEST
#define HW_TEST_DURATION_MS 30000
uint32_t start_ms = hal_time_ms();
#endif

while (1) {
    #ifdef HW_VALIDATION_TEST
    if (hal_time_ms() - start_ms > HW_TEST_DURATION_MS) {
        data_storage_shutdown(&storage);
        while (1) { /* halted — safe to unplug */ }
    }
    #endif

    if (power_mgmt_is_shutdown_requested()) { ... }

    // ... read NMEA, parse ...

    #ifdef HW_VALIDATION_TEST
    // Skip gps_filter_process(), but still check validity
    if (!(fix.flags & GPS_FIX_VALID) || !(fix.flags & GPS_HAS_LATLON)) continue;
    #else
    if (gps_filter_process(&filter, &fix) != FILTER_ACCEPT) continue;
    #endif

    data_storage_write_fix(&storage, &fix);

    #ifdef HW_VALIDATION_TEST
    // Echo CSV line over USB serial
    printf("FIX: %.6f,%.6f\n", fix.latitude, fix.longitude);
    #endif
}
```

The `printf` here is a minimal echo for monitoring — the full CSV formatting is done by `data_storage_write_fix()`. The serial output is just for live feedback, not a faithful CSV reproduction.

### Files NOT modified

- `src/gps_filter.c` — no changes. The filter is bypassed at the call site in `main.c`.
- `src/data_storage.c` — no changes. Writes to SD as normal.
- `src/hal/hal_pico.c` — no changes. All HAL functions used as-is.

## Expected Output

### SD card (`track.csv`)

```
timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality
2026-02-08T15:00:01Z,47.285233,8.565265,0.30,499.6,,8,1.01,1
2026-02-08T15:00:02Z,47.285234,8.565266,0.10,499.5,,8,1.02,1
...
```

Stationary fixes will show GPS jitter (small position variations, speed < 3 km/h). This is expected and confirms the pipeline is working.

### USB serial

```
FIX: 47.285233,8.565265
FIX: 47.285234,8.565266
...
```

## Success Criteria

1. `track.csv` exists on the SD card and is readable.
2. The CSV header is present and correct.
3. Multiple data rows are present with valid GPS coordinates.
4. The file is not corrupted (clean shutdown completed before unplug).
5. USB serial output matches the fixes written to the SD card.

## Lifecycle

This spec and the `test/hw-validation` branch are **temporary**. Once the supercapacitor is installed and the real power-management shutdown path is validated, this branch can be deleted.
