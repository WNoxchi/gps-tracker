# Power Management

Detect USB power loss via GPIO24 and trigger a clean SD card shutdown.

## Overview

The Pico 2 is powered via micro-USB from the car's USB adapter. When the car turns off, VBUS drops. GPIO24 reads high when VBUS is present, low when lost (via on-board voltage divider R10=5.6K, R1=10K, divides 5V to ~3.2V). A supercapacitor on VSYS provides holdover energy for clean shutdown.

Module: `power_mgmt.h` / `power_mgmt.c`

## Hardware

### GPIO24 VBUS Detection

- Built into Pico 2 PCB — no external components needed
- USB connected: GPIO24 reads HIGH (~3.2V)
- USB disconnected: GPIO24 reads LOW (pulled to ground via R1)
- Note: Pico 2 non-W variant only. Pico 2 W uses WL_GPIO2 instead.

### Supercapacitor

- 1F, 5.5V between VSYS and GND
- Holds VSYS above RT6150 minimum (~2.1V) for ~30 seconds at 40mA: `t = C × dV / I = 1.0 × 1.2 / 0.040 = 30s`
- Shutdown needs ~120ms worst case. Margin: >100x.

## ISR Design

Configure GPIO24 as input (no pull). Register falling-edge interrupt. ISR is minimal:

```c
static volatile bool g_power_lost = false;

static void power_loss_isr(uint gpio, uint32_t events) {
    (void)gpio;
    (void)events;
    g_power_lost = true;
}
```

No file I/O, no printf, no complex logic in the ISR. The main loop polls `g_power_lost`.

## Main Loop Integration

```c
while (true) {
    // 1. Check power — FIRST thing each iteration
    if (power_mgmt_is_shutdown_requested()) {
        data_storage_shutdown(&storage);
        while (true) { tight_loop_contents(); }
    }

    // 2. Read NMEA from UART
    // 3. Parse
    // 4. Filter
    // 5. Store
}
```

## Startup Sequence

On power-on, initialization order:

1. Initialize clocks, GPIO, UART1 on GP4/GP5 (9600 baud for GPS)
2. Initialize power management (GPIO24, register ISR)
3. Initialize storage (mount SD, detect unclean shutdown, open file)
4. Initialize NMEA parser
5. Initialize GPS filter (COLD_START state)
6. Enter main loop

No explicit "wait for GPS fix" step. The filter's COLD_START state rejects invalid/stationary fixes until a valid moving fix arrives.

## API

```c
void power_mgmt_init(void);
bool power_mgmt_is_shutdown_requested(void);
bool power_mgmt_is_vbus_present(void);
```

## Constants

| Constant | Value | Rationale |
|---|---|---|
| `POWER_MGMT_VBUS_GPIO` | 24 | Pico 2 hardware |
| `POWER_SHUTDOWN_TIMEOUT_MS` | 500 | Safety bound; supercap provides ~30s |

## Timing Analysis

1. ISR fires, sets flag: < 1 microsecond
2. Main loop polls: worst case ~50ms (end of current iteration at 1Hz with ~50ms loop body)
3. `f_sync()`: 10-50ms typical
4. `f_close()`: < 5ms
5. `f_unlink("_dirty")`: < 10ms
6. `f_unmount()`: < 5ms
7. **Total worst case: ~120ms. Supercap provides ~30s. Margin: >100x.**

## Acceptance Tests

| ID | Name | Given | Then |
|----|------|-------|------|
| T1 | initial_no_shutdown | `power_mgmt_init()`, no ISR fired | `is_shutdown_requested()` returns false |
| T2 | isr_sets_flag | Mock ISR callback invoked | `is_shutdown_requested()` returns true |
| T3 | vbus_present | Mock GPIO24 HIGH | `is_vbus_present()` returns true |
| T4 | vbus_absent | Mock GPIO24 LOW | `is_vbus_present()` returns false |
| T5 | shutdown_closes_storage | Init storage, write fixes, fire ISR, run loop | `data_storage_shutdown` called. File flushed and closed. |
| T6 | shutdown_idempotent | Call shutdown twice | No crash, no double-close. |
| T7 | gpio_configured_input | After init | Mock verifies GPIO24 set as input, no pull. |
| T8 | falling_edge_registered | After init | Mock verifies falling edge IRQ on GPIO24. |

## Cross-References

- Calls `data_storage_shutdown()` from `specs/data-storage.md`
- GPIO24 access through HAL from `specs/build-and-test.md`
