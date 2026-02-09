#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include "nmea_parser.h"
#include "hal/hal.h"

#define STORAGE_SYNC_INTERVAL_S   5
#define STORAGE_MAX_FILE_NUMBER   999
#define STORAGE_DIRTY_FILENAME    "_dirty"
#define STORAGE_BASE_FILENAME     "track"
#define CSV_HEADER                "timestamp,latitude,longitude,speed_kmh,altitude_m,course_deg,satellites,hdop,fix_quality\n"

typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERR_MOUNT,
    STORAGE_ERR_OPEN,
    STORAGE_ERR_WRITE,
    STORAGE_ERR_SYNC,
    STORAGE_ERR_FULL,
    STORAGE_ERR_TOO_MANY_FILES
} storage_error_t;

typedef struct {
    hal_file_t file;
    char filename[32];
    uint32_t last_sync_ms;
    bool is_open;
} data_storage_t;

storage_error_t data_storage_init(data_storage_t* storage);
storage_error_t data_storage_write_fix(data_storage_t* storage, const gps_fix_t* fix);
storage_error_t data_storage_shutdown(data_storage_t* storage);
const char*     data_storage_get_filename(const data_storage_t* storage);

#endif
