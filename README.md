# GPS Tracker — Raspberry Pi Pico 2

A GPS tracker that reads coordinates from a u-blox NEO-8M GPS module via UART and saves them as CSV to a FAT32-formatted SD card over SPI. Designed for in-car use with automatic clean shutdown on USB power loss.

## Hardware

- Raspberry Pi Pico 2 (RP2350)
- u-blox NEO-8M GPS module (UART1: TX→GP4, RX→GP5)
- MicroSD card breakout (SPI0: MISO→GP16, CS→GP17, CLK→GP18, MOSI→GP19)
- 1F 5.5V supercapacitor between VSYS and GND (for clean shutdown on power loss)

## Prerequisites

- **CMake** >= 3.13
- **macOS**: Xcode command line tools (`xcode-select --install`)
- **Linux**: `build-essential cmake`

## Setup

### 1. Clone the repository

```bash
git clone <repo-url> gps-tracker
cd gps-tracker
```

### 2. Install the ARM cross-compiler

The Pico 2 uses an ARM Cortex-M33. You need the `arm-none-eabi-gcc` toolchain with Newlib.

**macOS (ARM64):**

The Homebrew formula (`brew install arm-none-eabi-gcc`) is missing Newlib and won't work. Download the full toolchain tarball from ARM instead:

```bash
curl -L -o /tmp/arm-toolchain.tar.xz \
  "https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
mkdir -p external/arm-toolchain
tar xf /tmp/arm-toolchain.tar.xz -C external/arm-toolchain --strip-components=1
```

**Linux (x86_64):**

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

### 3. Clone the Pico SDK

```bash
git clone https://github.com/raspberrypi/pico-sdk.git external/pico-sdk --branch master
cd external/pico-sdk && git submodule update --init && cd ../..
```

### 4. Clone the FatFS SD card library

```bash
git clone https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico.git external/no-OS-FatFS-SD-SPI-RPi-Pico
```

## Building

### Pico firmware (cross-compile)

```bash
mkdir -p build/pico && cd build/pico

# Set these to wherever you installed/cloned them:
export PICO_SDK_PATH=$(cd ../../external/pico-sdk && pwd)

# macOS only (skip on Linux if arm-none-eabi-gcc is on PATH):
export PICO_TOOLCHAIN_PATH=$(cd ../../external/arm-toolchain && pwd)

cmake ../.. -DBUILD_FOR_PICO=ON -DBUILD_TESTS=OFF
cmake --build .
```

Output: `build/pico/gps_tracker.uf2` (~147KB)

### Host build (testing)

```bash
mkdir -p build/host && cd build/host
cmake ../.. -DBUILD_FOR_PICO=OFF -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

## Flashing the Pico 2

1. Hold the **BOOTSEL** button on the Pico 2
2. Plug it into USB while holding BOOTSEL
3. Release the button — it mounts as a USB drive (`RPI-RP2`)
4. Copy the firmware:
   ```bash
   cp build/pico/gps_tracker.uf2 /Volumes/RPI-RP2/
   ```
5. The Pico reboots automatically and starts the GPS tracker

## How it works

1. **Power on** — mounts SD card, detects previous unclean shutdown, opens/creates CSV file
2. **Main loop** — reads NMEA sentences from GPS, parses GGA+RMC, filters out stationary/outlier points, appends accepted fixes to CSV
3. **Power loss** — GPIO24 detects USB voltage drop, flushes and closes the file, deletes the dirty marker

### CSV output format

Files are written to the SD card root as `track.csv` (or `track_1.csv`, `track_2.csv`, etc. after unclean shutdowns):

```
timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality
2025-06-15T14:23:07Z,47.285233,8.565265,52.30,499.6,77.5,8,1.01,1
```

## Project structure

```
src/
  main.c              # Pico entry point and main loop
  nmea_parser.c/.h    # NMEA GGA/RMC sentence parser
  gps_filter.c/.h     # Stationary/outlier rejection filter
  data_storage.c/.h   # CSV file writing with power-loss tolerance
  power_mgmt.c/.h     # USB power detection and clean shutdown
  hal/
    hal.h             # Hardware abstraction layer interface
    hal_pico.c        # Real hardware (Pico SDK + FatFS)
    hal_mock.c        # Mock hardware (for host testing)
    hw_config.c       # SD card SPI pin configuration
  lib/
    geo_utils.c/.h    # Haversine distance calculation
tests/                # Unity-based unit tests (host only)
specs/                # Detailed module specifications
external/
  Unity/              # Vendored test framework
  pico-sdk/           # Cloned at build time (gitignored)
  arm-toolchain/      # Extracted at build time (gitignored)
  no-OS-FatFS-SD-SPI-RPi-Pico/  # Cloned at build time (gitignored)
```
