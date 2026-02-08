add_library(unity STATIC
    ${CMAKE_SOURCE_DIR}/external/Unity/src/unity.c
)
target_include_directories(unity PUBLIC ${CMAKE_SOURCE_DIR}/external/Unity/src)

# Test 1: geo_utils (3 tests, no setUp/tearDown)
add_executable(test_geo_utils_exe test_geo_utils.c)
target_link_libraries(test_geo_utils_exe gps_tracker_lib unity m)
target_compile_options(test_geo_utils_exe PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_geo_utils COMMAND test_geo_utils_exe)

# Test 2: nmea_parser (18 tests, has setUp/tearDown)
add_executable(test_nmea_parser_exe test_nmea_parser.c)
target_link_libraries(test_nmea_parser_exe gps_tracker_lib unity m)
target_compile_options(test_nmea_parser_exe PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_nmea_parser COMMAND test_nmea_parser_exe)

# Test 3: gps_filter (13 tests, has setUp/tearDown)
add_executable(test_gps_filter_exe test_gps_filter.c)
target_link_libraries(test_gps_filter_exe gps_tracker_lib unity m)
target_compile_options(test_gps_filter_exe PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_gps_filter COMMAND test_gps_filter_exe)

# Test 4: data_storage (15 tests, has setUp/tearDown)
add_executable(test_data_storage_exe test_data_storage.c)
target_link_libraries(test_data_storage_exe gps_tracker_lib unity m)
target_compile_options(test_data_storage_exe PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_data_storage COMMAND test_data_storage_exe)

# Test 5: power_mgmt (8 tests, has setUp/tearDown)
add_executable(test_power_mgmt_exe test_power_mgmt.c)
target_link_libraries(test_power_mgmt_exe gps_tracker_lib unity m)
target_compile_options(test_power_mgmt_exe PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_power_mgmt COMMAND test_power_mgmt_exe)
