# Build System, Testing, and Project Structure

Dual-target CMake build with a HAL abstraction enabling hardware-free testing via Unity.

## Overview

Two build targets from the same source tree:
- **Host** (Mac/Linux): native `gcc`/`clang`, mocked hardware, runs all unit tests. This is the Ralph Loop development target.
- **Pico** (RP2350): cross-compile with `arm-none-eabi-gcc` via Pico SDK, produces `.uf2` for flashing.

Test framework: [Unity](https://github.com/ThrowTheSwitch/Unity) (ThrowTheSwitch) — single C file.

## Directory Structure

```
gps-tracker/
  CLAUDE.md
  IMPLEMENTATION_PLAN.md
  PROMPT_build.md
  PROMPT_plan.md
  CMakeLists.txt
  pico_sdk_import.cmake
  specs/
    nmea-parser.md
    gps-filtering.md
    data-storage.md
    power-management.md
    build-and-test.md
  src/
    main.c                  # Pico entry point only
    nmea_parser.h / .c
    gps_filter.h / .c
    data_storage.h / .c
    power_mgmt.h / .c
    hal/
      hal.h                 # HAL interface
      hal_pico.c            # Pico SDK implementation
      hal_mock.c            # Host mock implementation
    lib/
      geo_utils.h / .c      # Haversine, coordinate math
  tests/
    CMakeLists.txt
    test_nmea_parser.c
    test_gps_filter.c
    test_data_storage.c
    test_power_mgmt.c
    test_geo_utils.c
    test_main.c             # Unity test runner
  external/
    Unity/                  # Git submodule or vendored
      src/
        unity.c
        unity.h
        unity_internals.h
```

## CMake Configuration

### Top-Level CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.13)

option(BUILD_FOR_PICO "Build for Raspberry Pi Pico 2" OFF)
option(BUILD_TESTS "Build unit tests (host only)" ON)

if(BUILD_FOR_PICO)
    set(PICO_BOARD pico2)
    include(pico_sdk_import.cmake)
endif()

project(gps_tracker C CXX ASM)
set(CMAKE_C_STANDARD 11)

if(BUILD_FOR_PICO)
    pico_sdk_init()
endif()

add_library(gps_tracker_lib STATIC
    src/nmea_parser.c
    src/gps_filter.c
    src/data_storage.c
    src/power_mgmt.c
    src/lib/geo_utils.c
)
target_include_directories(gps_tracker_lib PUBLIC src src/lib)

if(BUILD_FOR_PICO)
    target_sources(gps_tracker_lib PRIVATE src/hal/hal_pico.c)
    target_link_libraries(gps_tracker_lib pico_stdlib hardware_uart hardware_spi hardware_gpio)
    add_executable(gps_tracker src/main.c)
    target_link_libraries(gps_tracker gps_tracker_lib)
    pico_add_extra_outputs(gps_tracker)
else()
    target_sources(gps_tracker_lib PRIVATE src/hal/hal_mock.c)
    target_compile_definitions(gps_tracker_lib PUBLIC HOST_BUILD=1)
    target_compile_options(gps_tracker_lib PRIVATE -Wall -Wextra -Werror)
endif()

if(BUILD_TESTS AND NOT BUILD_FOR_PICO)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### Build Commands

Host (testing):
```bash
mkdir -p build/host && cd build/host
cmake ../.. -DBUILD_FOR_PICO=OFF -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

Pico (cross-compile):
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir -p build/pico && cd build/pico
cmake ../.. -DBUILD_FOR_PICO=ON -DBUILD_TESTS=OFF
cmake --build .
# Output: gps_tracker.uf2
```

## Compiler Flags

- Host: `-Wall -Wextra -Werror -std=c11 -g -O0` — `-Werror` is critical so agents can't ignore warnings
- Pico: `-Wall -Wextra -std=c11 -Os` — no `-Werror` because Pico SDK generates some warnings outside our control

## Hardware Abstraction Layer (HAL)

### Purpose

Every hardware interaction goes through `hal.h`. Host tests use mock implementations. Pico builds use real SDK calls. Logic modules (parser, filter) have zero Pico SDK dependencies.

### Interface (`src/hal/hal.h`)

```c
#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// UART
void hal_uart_init(uint32_t baud_rate);
int hal_uart_read_line(char* buf, size_t buf_size, uint32_t timeout_ms);

// GPIO
void hal_gpio_init_input(uint pin);
bool hal_gpio_read(uint pin);
typedef void (*hal_gpio_irq_callback_t)(uint pin, uint32_t events);
void hal_gpio_set_irq(uint pin, uint32_t edge_mask, hal_gpio_irq_callback_t cb);

// Filesystem
typedef void* hal_file_t;
int hal_fs_mount(void);
int hal_fs_unmount(void);
hal_file_t hal_fs_open(const char* path, const char* mode);
int hal_fs_write(hal_file_t file, const void* buf, size_t len);
int hal_fs_read(hal_file_t file, void* buf, size_t len);
int hal_fs_sync(hal_file_t file);
int hal_fs_close(hal_file_t file);
int hal_fs_remove(const char* path);
bool hal_fs_exists(const char* path);
int hal_fs_seek_end(hal_file_t file);
int hal_fs_read_byte_at_end(hal_file_t file);
int hal_fs_size(hal_file_t file);

// Time
uint32_t hal_time_ms(void);
void hal_sleep_ms(uint32_t ms);

#endif
```

### Pin Assignments (Pico)

**GPS (u-blox NEO-8M) — UART1:**

| Function | GPIO | Pico Pin | Notes |
|----------|------|----------|-------|
| UART1 TX → GPS RXD | GP4 | Pin 6 | Pico sends to GPS |
| UART1 RX ← GPS TXD | GP5 | Pin 7 | GPS sends to Pico |
| GPS PPS | GP2 | Pin 4 | Pulse-per-second (GPIO input, future use) |
| GPS VCC | — | Pin 36 | 3V3(OUT) |
| GPS GND | — | Pin 38 | GND |

**SD Card — SPI0:**

| Function | GPIO | Pico Pin | Notes |
|----------|------|----------|-------|
| SPI0 RX (SD MISO) | GP16 | Pin 21 | SD sends data to Pico |
| SPI0 CSn (SD CS) | GP17 | Pin 22 | Chip select |
| SPI0 SCK (SD CLK) | GP18 | Pin 24 | Clock |
| SPI0 TX (SD MOSI) | GP19 | Pin 25 | Pico sends data to SD |
| SD VCC | — | Pin 40 | VBUS (5V, module has regulator) |
| SD GND | — | Pin 38 | GND |

**Internal:**

| Function | GPIO | Pico Pin | Notes |
|----------|------|----------|-------|
| VBUS Detect | GP24 | (internal) | On-board voltage divider, no external wiring |

### Mock Implementation (`src/hal/hal_mock.c`)

Mock control API (test-only, not in `hal.h`):

```c
void hal_mock_uart_set_data(const char* nmea_data);
void hal_mock_gpio_set(uint pin, bool value);
void hal_mock_gpio_trigger_irq(uint pin, uint32_t events);
void hal_mock_time_set_ms(uint32_t ms);
void hal_mock_time_advance_ms(uint32_t ms);
void hal_mock_fs_set_root(const char* path);
void hal_mock_reset(void);
```

- UART: reads from configurable buffer set by test
- GPIO: returns values set by test, IRQ callbacks manually triggered
- Filesystem: wraps standard C `fopen`/`fwrite`/`fclose` on a temp directory
- Time: returns mock clock, tests advance manually

## What Gets Mocked vs Real

| Component | Host (Mock) | Pico (Real) |
|-----------|-------------|-------------|
| UART GPS | Canned NMEA from buffer | `uart_read_blocking()` on UART1 (GP4/GP5) |
| SPI SD card | C stdio on temp dir | FatFs + SPI |
| GPIO24 | `hal_mock_gpio_set()` | `gpio_get(24)` |
| GPIO24 IRQ | `hal_mock_gpio_trigger_irq()` | `gpio_set_irq_enabled_with_callback()` |
| Clock | `hal_mock_time_set_ms()` | `to_ms_since_boot()` |
| NMEA parser | Real code | Real code |
| GPS filter | Real code | Real code |
| Data storage | Real code, mock FS | Real code, real FatFs |

## Unity Test Framework

### Test File Pattern

```c
#include "unity.h"
#include "module_under_test.h"

void setUp(void) { hal_mock_reset(); }
void tearDown(void) { }

void test_example(void) {
    TEST_ASSERT_EQUAL_INT(expected, actual);
    TEST_ASSERT_FLOAT_WITHIN(0.000001, expected, actual);
}
```

### Test Runner (`tests/test_main.c`)

```c
#include "unity.h"
extern void test_example(void);

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    return UNITY_END();
}
```

## External Dependencies

| Dependency | Purpose | Integration |
|---|---|---|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) | RP2350 HAL | `pico_sdk_import.cmake` (Pico only) |
| [no-OS-FatFS-SD-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico) | FAT32 on SD via SPI | Git submodule (Pico only) |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | Unit tests | Vendored in `external/Unity/` (host only) |

## Codebase Conventions

- C11, no C++ except where Pico SDK requires it
- One `.h` + `.c` per module, functions prefixed with module name
- No dynamic allocation after startup (only `nmea_parser_create()`)
- No `printf` in production code
- Constants in SCREAMING_SNAKE_CASE in relevant headers

## Acceptance Tests

| ID | Name | Given | Then |
|----|------|-------|------|
| T1 | host_build_succeeds | Clean checkout, host cmake + make | Zero errors, zero warnings |
| T2 | host_tests_pass | Host build complete | `ctest` exits 0, all tests pass |
| T3 | pico_build_succeeds | Pico SDK installed | `gps_tracker.uf2` exists, > 0 bytes |
| T4 | no_pico_sdk_in_host | Host build | No Pico SDK headers referenced. `HOST_BUILD=1` defined. |
| T5 | mock_compiles_host_only | Host build | `hal_mock.c` compiled, `hal_pico.c` NOT |
| T6 | pico_compiles_pico_only | Pico build | `hal_pico.c` compiled, `hal_mock.c` NOT |
| T7 | werror_on_host | Unused variable in source | Host build fails |
