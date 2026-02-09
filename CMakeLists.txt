cmake_minimum_required(VERSION 3.13)

option(BUILD_FOR_PICO "Build for Raspberry Pi Pico 2" OFF)
option(BUILD_TESTS "Build unit tests (host only)" ON)
option(HW_VALIDATION_TEST "Hardware validation test mode" OFF)

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
    # Add FatFS sources directly (skip rtc.c since we handle time separately)
    target_sources(gps_tracker_lib PRIVATE
        src/hal/hal_pico.c
        src/hal/hw_config.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/ff15/source/ffsystem.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/ff15/source/ffunicode.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/ff15/source/ff.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/sd_driver/sd_spi.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/sd_driver/demo_logging.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/sd_driver/spi.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/sd_driver/sd_card.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/sd_driver/crc.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/src/glue.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/src/f_util.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/src/ff_stdio.c
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/src/my_debug.c
    )

    # Add FatFS include directories
    target_include_directories(gps_tracker_lib PRIVATE
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/ff15/source
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/sd_driver
        external/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/include
    )

    target_link_libraries(gps_tracker_lib pico_stdlib hardware_uart hardware_spi hardware_gpio hardware_dma)

    if(HW_VALIDATION_TEST)
        target_compile_definitions(gps_tracker_lib PUBLIC HW_VALIDATION_TEST=1)
    endif()

    add_executable(gps_tracker src/main.c)
    target_link_libraries(gps_tracker gps_tracker_lib)

    if(HW_VALIDATION_TEST)
        pico_enable_stdio_usb(gps_tracker 1)
    endif()

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
