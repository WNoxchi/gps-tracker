# Raspberry Pi Pico 2 Hardware Flashing Guide

To load the GPS tracker onto a Raspberry Pi Pico 2, you'll need to cross-compile for the Pico hardware and then flash the `.uf2` file. Here's the process:

## Prerequisites

### 1. Pico SDK

You'll need the Raspberry Pi Pico SDK installed. If you don't have it:

```bash
# Clone the SDK (pick a location you want to keep it)
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk
git submodule update --init
```

### 2. ARM Toolchain

For cross-compilation:

**On Ubuntu/Debian:**
```bash
sudo apt-get install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```

**On macOS:**
```bash
brew install cmake arm-none-eabi-gcc
```

## Build for Pico 2

Once you have the SDK installed, build with:

```bash
export PICO_SDK_PATH=~/pico-sdk
mkdir -p build/pico && cd build/pico
cmake ../.. -DBUILD_FOR_PICO=ON -DBUILD_TESTS=OFF
cmake --build .
```

This produces `build/pico/gps_tracker.uf2`.

## Flash to Pico 2

### 1. Enter Bootloader Mode

- Hold the **BOOTSEL** button on the Pico 2
- While holding, plug the USB cable into the computer
- Release the button
- The Pico should appear as a USB mass storage device (check `lsblk` or `df`)

### 2. Copy the .uf2 File

```bash
# Find the mount point (usually /media/your-user/RPI-RP2)
mount | grep RPI-RP2

# Copy the UF2 file
cp build/pico/gps_tracker.uf2 /media/your-user/RPI-RP2/

# The Pico will auto-flash and reboot
```

## Hardware Connections

Before powering on, ensure your GPS module and SD card are connected per the pinout below:

### GPS (u-blox NEO-8M) - UART1

| Function | GPIO | Pico Pin | Connection |
|----------|------|----------|-----------|
| UART1 TX | GP4 | Pin 6 | GPS RXD |
| UART1 RX | GP5 | Pin 7 | GPS TXD |
| VCC | — | Pin 36 | 3V3(OUT) → GPS VCC |
| GND | — | Pin 38 | GND → GPS GND |

### SD Card - SPI0

| Function | GPIO | Pico Pin | Connection |
|----------|------|----------|-----------|
| SPI0 RX (MISO) | GP16 | Pin 21 | SD MISO |
| SPI0 CSn (CS) | GP17 | Pin 22 | SD CS |
| SPI0 SCK (CLK) | GP18 | Pin 24 | SD CLK |
| SPI0 TX (MOSI) | GP19 | Pin 25 | SD MOSI |
| VCC | — | Pin 40 | VBUS (5V) → SD VCC |
| GND | — | Pin 38 | GND → SD GND |

### Power Detection

| Function | GPIO |
|----------|------|
| VBUS Detect | GP24 (built-in voltage divider, no external wiring needed) |

This GPIO monitors USB power for clean shutdown on power loss. It reads HIGH when USB is connected, LOW when disconnected.

## Validation

Once running on hardware:

1. The device will initialize:
   - Power management (GPIO24 VBUS detection)
   - Storage (mount SD card, detect unclean shutdown)
   - NMEA parser
   - GPS filter (COLD_START state)

2. It waits for valid GPS fixes:
   - Must be moving (speed > 3 km/h)
   - Must have valid signal (fix quality >= 1)
   - Must pass outlier checks (no impossible jumps)

3. Accepted fixes are written to `track.csv` on the SD card

4. Each CSV row contains:
   ```
   timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality
   2025-06-15T14:23:07Z,47.285233,8.565265,52.30,499.6,77.5,8,1.01,1
   ```

5. To verify:
   - Remove the SD card from the Pico
   - Insert it into a computer with an SD card reader
   - Open `track.csv` to view the collected GPS data

## Troubleshooting

**Pico doesn't appear as mass storage after BOOTSEL:**
- Make sure you're holding BOOTSEL while plugging in USB
- Try a different USB cable (some cables are charge-only)
- Check `lsusb` to see if it appears as a USB device

**Build fails with "PICO_SDK_PATH not set":**
- Ensure you've exported the environment variable:
  ```bash
  export PICO_SDK_PATH=~/pico-sdk
  ```
- Or add it to your `~/.bashrc` or `~/.zshrc` for persistence

**GPS module not getting data:**
- Verify UART1 connections (GP4/GP5)
- Check baud rate is 9600 (hardcoded in HAL)
- Ensure GPS module has a clear view of sky (needs satellite signals)

**SD card not being written to:**
- Verify SD card is FAT32 formatted
- Check SPI0 connections (GP16-19)
- Ensure SD card is inserted before power-on

**Power loss detection not working:**
- GPIO24 is built-in; no external connections needed
- Unplug USB to simulate power loss
- Supercapacitor should hold power for ~30 seconds (time to flush and close files)

## Next Steps

- Monitor serial output (USB UART) if needed for debugging
- Check the CSV file after test drives to validate GPS data collection
- Verify clean shutdown by checking for `_dirty` file on SD card (should not exist after clean shutdown)
