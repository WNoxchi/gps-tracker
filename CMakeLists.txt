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
