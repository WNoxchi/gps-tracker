#ifdef HOST_BUILD

#include "hal/hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Mock state ---- */

#define HAL_MOCK_UART_BUF_SIZE 4096
#define HAL_MOCK_MAX_GPIO 32
#define HAL_MOCK_MAX_PATH 256

static char mock_uart_buf[HAL_MOCK_UART_BUF_SIZE];
static size_t mock_uart_pos;
static size_t mock_uart_len;

static bool mock_gpio_values[HAL_MOCK_MAX_GPIO];
static hal_gpio_irq_callback_t mock_gpio_callbacks[HAL_MOCK_MAX_GPIO];
static uint32_t mock_gpio_edge_masks[HAL_MOCK_MAX_GPIO];
static bool mock_gpio_initialized[HAL_MOCK_MAX_GPIO];

static uint32_t mock_time_ms_val;

static char mock_fs_root[HAL_MOCK_MAX_PATH];
static bool mock_fs_mounted;

/* ---- Mock control API ---- */

void hal_mock_reset(void) {
    memset(mock_uart_buf, 0, sizeof(mock_uart_buf));
    mock_uart_pos = 0;
    mock_uart_len = 0;
    memset(mock_gpio_values, 0, sizeof(mock_gpio_values));
    memset(mock_gpio_callbacks, 0, sizeof(mock_gpio_callbacks));
    memset(mock_gpio_edge_masks, 0, sizeof(mock_gpio_edge_masks));
    memset(mock_gpio_initialized, 0, sizeof(mock_gpio_initialized));
    mock_time_ms_val = 0;
    memset(mock_fs_root, 0, sizeof(mock_fs_root));
    mock_fs_mounted = false;
}

void hal_mock_uart_set_data(const char* nmea_data) {
    size_t len = strlen(nmea_data);
    if (len >= HAL_MOCK_UART_BUF_SIZE) len = HAL_MOCK_UART_BUF_SIZE - 1;
    memcpy(mock_uart_buf, nmea_data, len);
    mock_uart_buf[len] = '\0';
    mock_uart_len = len;
    mock_uart_pos = 0;
}

void hal_mock_gpio_set(uint32_t pin, bool value) {
    if (pin < HAL_MOCK_MAX_GPIO) {
        mock_gpio_values[pin] = value;
    }
}

void hal_mock_gpio_trigger_irq(uint32_t pin, uint32_t events) {
    if (pin < HAL_MOCK_MAX_GPIO && mock_gpio_callbacks[pin]) {
        mock_gpio_callbacks[pin](pin, events);
    }
}

bool hal_mock_gpio_is_initialized(uint32_t pin) {
    if (pin < HAL_MOCK_MAX_GPIO) return mock_gpio_initialized[pin];
    return false;
}

uint32_t hal_mock_gpio_get_edge_mask(uint32_t pin) {
    if (pin < HAL_MOCK_MAX_GPIO) return mock_gpio_edge_masks[pin];
    return 0;
}

void hal_mock_time_set_ms(uint32_t ms) {
    mock_time_ms_val = ms;
}

void hal_mock_time_advance_ms(uint32_t ms) {
    mock_time_ms_val += ms;
}

/* ---- HAL Time implementation ---- */

uint32_t hal_time_ms(void) {
    return mock_time_ms_val;
}

void hal_sleep_ms(uint32_t ms) {
    (void)ms;
}

void hal_mock_fs_set_root(const char* path) {
    size_t len = strlen(path);
    if (len >= HAL_MOCK_MAX_PATH) len = HAL_MOCK_MAX_PATH - 1;
    memcpy(mock_fs_root, path, len);
    mock_fs_root[len] = '\0';
}

/* ---- HAL UART implementation ---- */

void hal_uart_init(uint32_t baud_rate) {
    (void)baud_rate;
}

int hal_uart_read_line(char* buf, size_t buf_size, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (mock_uart_pos >= mock_uart_len) return -1;

    size_t i = 0;
    while (mock_uart_pos < mock_uart_len && i < buf_size - 1) {
        char c = mock_uart_buf[mock_uart_pos++];
        if (c == '\n') {
            buf[i] = '\0';
            return (int)i;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (i > 0) ? (int)i : -1;
}

/* ---- HAL GPIO implementation ---- */

void hal_gpio_init_input(uint32_t pin) {
    if (pin < HAL_MOCK_MAX_GPIO) {
        mock_gpio_initialized[pin] = true;
    }
}

bool hal_gpio_read(uint32_t pin) {
    if (pin < HAL_MOCK_MAX_GPIO) return mock_gpio_values[pin];
    return false;
}

void hal_gpio_set_irq(uint32_t pin, uint32_t edge_mask, hal_gpio_irq_callback_t cb) {
    if (pin < HAL_MOCK_MAX_GPIO) {
        mock_gpio_callbacks[pin] = cb;
        mock_gpio_edge_masks[pin] = edge_mask;
    }
}

/* ---- HAL Filesystem implementation (wraps stdio on temp dir) ---- */

static void build_path(char* out, size_t out_size, const char* name) {
    snprintf(out, out_size, "%s/%s", mock_fs_root, name);
}

int hal_fs_mount(void) {
    if (mock_fs_root[0] == '\0') return -1;
    mock_fs_mounted = true;
    return 0;
}

int hal_fs_unmount(void) {
    mock_fs_mounted = false;
    return 0;
}

hal_file_t hal_fs_open(const char* path, const char* mode) {
    if (!mock_fs_mounted) return NULL;
    char full[HAL_MOCK_MAX_PATH * 2];
    build_path(full, sizeof(full), path);
    FILE* f = fopen(full, mode);
    return (hal_file_t)f;
}

int hal_fs_write(hal_file_t file, const void* buf, size_t len) {
    if (!file) return -1;
    size_t written = fwrite(buf, 1, len, (FILE*)file);
    return (written == len) ? 0 : -1;
}

int hal_fs_read(hal_file_t file, void* buf, size_t len) {
    if (!file) return -1;
    size_t rd = fread(buf, 1, len, (FILE*)file);
    return (int)rd;
}

int hal_fs_sync(hal_file_t file) {
    if (!file) return -1;
    return fflush((FILE*)file);
}

int hal_fs_close(hal_file_t file) {
    if (!file) return -1;
    return fclose((FILE*)file);
}

int hal_fs_remove(const char* path) {
    char full[HAL_MOCK_MAX_PATH * 2];
    build_path(full, sizeof(full), path);
    return remove(full);
}

bool hal_fs_exists(const char* path) {
    char full[HAL_MOCK_MAX_PATH * 2];
    build_path(full, sizeof(full), path);
    FILE* f = fopen(full, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

int hal_fs_seek_end(hal_file_t file) {
    if (!file) return -1;
    return fseek((FILE*)file, 0, SEEK_END);
}

int hal_fs_read_byte_at_end(hal_file_t file) {
    if (!file) return -1;
    FILE* f = (FILE*)file;
    long pos = ftell(f);
    if (pos < 0) return -1;
    if (fseek(f, -1, SEEK_END) != 0) return -1;
    int byte = fgetc(f);
    if (fseek(f, pos, SEEK_SET) != 0) return -1;
    return byte;
}

int hal_fs_size(hal_file_t file) {
    if (!file) return -1;
    FILE* f = (FILE*)file;
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, cur, SEEK_SET);
    return (int)size;
}

#endif /* HOST_BUILD */
