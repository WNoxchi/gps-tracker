#include "data_storage.h"
#include <string.h>
#include <stdio.h>

static void make_filename(char* buf, size_t buf_size, int number) {
    if (number == 0) {
        snprintf(buf, buf_size, "%s.csv", STORAGE_BASE_FILENAME);
    } else {
        snprintf(buf, buf_size, "%s_%d.csv", STORAGE_BASE_FILENAME, number);
    }
}

static int find_highest_file_number(void) {
    /* Check track.csv first */
    int highest = -1;
    if (hal_fs_exists("track.csv")) {
        highest = 0;
    }
    for (int i = 1; i <= STORAGE_MAX_FILE_NUMBER; i++) {
        char name[32];
        make_filename(name, sizeof(name), i);
        if (hal_fs_exists(name)) {
            highest = i;
        }
    }
    return highest;
}

static bool file_ends_with_newline(const char* filename) {
    hal_file_t f = hal_fs_open(filename, "rb");
    if (!f) return false;
    int size = hal_fs_size(f);
    if (size <= 0) {
        hal_fs_close(f);
        return (size == 0); /* empty file is OK */
    }
    int byte = hal_fs_read_byte_at_end(f);
    hal_fs_close(f);
    return byte == '\n';
}

static bool file_is_empty(const char* filename) {
    hal_file_t f = hal_fs_open(filename, "rb");
    if (!f) return true;
    int size = hal_fs_size(f);
    hal_fs_close(f);
    return size == 0;
}

storage_error_t data_storage_init(data_storage_t* storage) {
    if (!storage) return STORAGE_ERR_MOUNT;
    memset(storage, 0, sizeof(data_storage_t));

    if (hal_fs_mount() != 0) return STORAGE_ERR_MOUNT;

    int highest = find_highest_file_number();
    bool dirty = hal_fs_exists(STORAGE_DIRTY_FILENAME);
    bool need_new_file = false;
    bool need_header = false;

    if (highest < 0) {
        /* No files exist — create track.csv */
        highest = 0;
        need_new_file = true;
        need_header = true;
    } else if (dirty) {
        /* Unclean shutdown — rotate */
        need_new_file = true;
        need_header = true;
        hal_fs_remove(STORAGE_DIRTY_FILENAME);
    } else {
        /* Check if last file is clean */
        char name[32];
        make_filename(name, sizeof(name), highest);
        if (file_is_empty(name)) {
            need_header = true;
        } else if (!file_ends_with_newline(name)) {
            /* Incomplete write — rotate */
            need_new_file = true;
            need_header = true;
        }
    }

    if (need_new_file) {
        /* Rotate to next file number */
        highest = find_highest_file_number() + 1;
    }

    if (highest > STORAGE_MAX_FILE_NUMBER) {
        return STORAGE_ERR_TOO_MANY_FILES;
    }

    make_filename(storage->filename, sizeof(storage->filename), highest);

    /* Open for append */
    storage->file = hal_fs_open(storage->filename, "ab");
    if (!storage->file) return STORAGE_ERR_OPEN;
    storage->is_open = true;

    if (need_header) {
        if (hal_fs_write(storage->file, CSV_HEADER, strlen(CSV_HEADER)) != 0) {
            return STORAGE_ERR_WRITE;
        }
    }

    /* Create dirty marker */
    hal_file_t dirty_f = hal_fs_open(STORAGE_DIRTY_FILENAME, "wb");
    if (dirty_f) hal_fs_close(dirty_f);

    storage->last_sync_ms = hal_time_ms();
    return STORAGE_OK;
}

storage_error_t data_storage_write_fix(data_storage_t* storage, const gps_fix_t* fix) {
    if (!storage || !storage->is_open) return STORAGE_ERR_WRITE;

    char line[256];
    int pos = 0;

    /* Timestamp */
    if ((fix->flags & GPS_HAS_DATE) && (fix->flags & GPS_HAS_TIME)) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos,
                        "%04u-%02u-%02uT%02u:%02u:%02uZ",
                        fix->year, fix->month, fix->day,
                        fix->hour, fix->minute, fix->second);
    }
    line[pos++] = ',';

    /* Latitude */
    if (fix->flags & GPS_HAS_LATLON) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%.6f", fix->latitude);
    }
    line[pos++] = ',';

    /* Longitude */
    if (fix->flags & GPS_HAS_LATLON) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%.6f", fix->longitude);
    }
    line[pos++] = ',';

    /* Speed */
    if (fix->flags & GPS_HAS_SPEED) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%.2f", (double)fix->speed_kmh);
    }
    line[pos++] = ',';

    /* Altitude */
    if (fix->flags & GPS_HAS_ALTITUDE) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%.1f", (double)fix->altitude_m);
    }
    line[pos++] = ',';

    /* Course */
    if (fix->flags & GPS_HAS_COURSE) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%.1f", (double)fix->course_deg);
    }
    line[pos++] = ',';

    /* Satellites */
    if (fix->flags & GPS_HAS_LATLON) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%u", fix->satellites);
    }
    line[pos++] = ',';

    /* HDOP */
    if (fix->flags & GPS_HAS_HDOP) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%.2f", (double)fix->hdop);
    }
    line[pos++] = ',';

    /* Fix quality */
    pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%u", fix->fix_quality);
    line[pos++] = '\n';
    line[pos] = '\0';

    if (hal_fs_write(storage->file, line, (size_t)pos) != 0) {
        return STORAGE_ERR_WRITE;
    }

    /* Sync if interval elapsed */
    uint32_t now = hal_time_ms();
    if (now - storage->last_sync_ms >= STORAGE_SYNC_INTERVAL_S * 1000u) {
        if (hal_fs_sync(storage->file) != 0) {
            return STORAGE_ERR_SYNC;
        }
        storage->last_sync_ms = now;
    }

    return STORAGE_OK;
}

storage_error_t data_storage_shutdown(data_storage_t* storage) {
    if (!storage || !storage->is_open) return STORAGE_ERR_WRITE;

    hal_fs_sync(storage->file);
    hal_fs_close(storage->file);
    storage->file = NULL;
    storage->is_open = false;

    hal_fs_remove(STORAGE_DIRTY_FILENAME);
    hal_fs_unmount();

    return STORAGE_OK;
}

const char* data_storage_get_filename(const data_storage_t* storage) {
    if (!storage) return NULL;
    return storage->filename;
}
