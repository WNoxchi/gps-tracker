#ifndef HOST_BUILD

#include "hal/hal.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <stdlib.h>
#include <string.h>

#define GPS_UART       uart1
#define GPS_UART_TX_GP 4
#define GPS_UART_RX_GP 5

/* ---- UART ---- */

void hal_uart_init(uint32_t baud_rate) {
    uart_init(GPS_UART, baud_rate);
    gpio_set_function(GPS_UART_TX_GP, GPIO_FUNC_UART);
    gpio_set_function(GPS_UART_RX_GP, GPIO_FUNC_UART);
}

int hal_uart_read_line(char* buf, size_t buf_size, uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    size_t i = 0;
    while (i < buf_size - 1) {
        if (!uart_is_readable_within_us(GPS_UART, (uint64_t)(timeout_ms) * 1000)) {
            break;
        }
        char c = uart_getc(GPS_UART);
        if (c == '\n') {
            buf[i] = '\0';
            return (int)i;
        }
        buf[i++] = c;
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) break;
    }
    buf[i] = '\0';
    return (i > 0) ? (int)i : -1;
}

/* ---- GPIO ---- */

void hal_gpio_init_input(uint32_t pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
}

bool hal_gpio_read(uint32_t pin) {
    return gpio_get(pin);
}

static hal_gpio_irq_callback_t pico_irq_cb = NULL;
static uint32_t pico_irq_pin = 0;

static void pico_gpio_irq_handler(uint gpio, uint32_t events) {
    if (pico_irq_cb && gpio == pico_irq_pin) {
        pico_irq_cb(gpio, events);
    }
}

void hal_gpio_set_irq(uint32_t pin, uint32_t edge_mask, hal_gpio_irq_callback_t cb) {
    pico_irq_cb = cb;
    pico_irq_pin = pin;
    gpio_set_irq_enabled_with_callback(pin, edge_mask, true, pico_gpio_irq_handler);
}

/* ---- Filesystem (FatFS integration) ---- */

#include "ff.h"
#include "hw_config.h"

/* Global filesystem handle */
static FATFS g_fs = {0};
static bool g_fs_mounted = false;

int hal_fs_mount(void) {
    if (g_fs_mounted) {
        return 0;
    }

    FRESULT res = f_mount(&g_fs, "0:", 1);
    if (res == FR_OK) {
        g_fs_mounted = true;
        return 0;
    }
    return -1;
}

int hal_fs_unmount(void) {
    if (!g_fs_mounted) {
        return 0;
    }

    FRESULT res = f_unmount("0:");
    if (res == FR_OK) {
        g_fs_mounted = false;
        return 0;
    }
    return -1;
}

hal_file_t hal_fs_open(const char* path, const char* mode) {
    if (!path || !mode) {
        return NULL;
    }

    FIL *file = malloc(sizeof(FIL));
    if (!file) {
        return NULL;
    }

    BYTE f_mode = 0;
    if (strchr(mode, 'r')) {
        f_mode |= FA_READ;
    }
    if (strchr(mode, 'w')) {
        f_mode |= (FA_WRITE | FA_CREATE_ALWAYS);
    }
    if (strchr(mode, 'a')) {
        f_mode |= (FA_WRITE | FA_OPEN_ALWAYS);
    }

    FRESULT res = f_open(file, path, f_mode);
    if (res != FR_OK) {
        free(file);
        return NULL;
    }

    /* If append mode, seek to end */
    if (strchr(mode, 'a')) {
        f_lseek(file, f_size(file));
    }

    return (hal_file_t)file;
}

int hal_fs_write(hal_file_t file, const void* buf, size_t len) {
    if (!file || !buf || len == 0) {
        return -1;
    }

    UINT written = 0;
    FRESULT res = f_write((FIL *)file, buf, (UINT)len, &written);
    if (res != FR_OK) {
        return -1;
    }

    return (int)written;
}

int hal_fs_read(hal_file_t file, void* buf, size_t len) {
    if (!file || !buf || len == 0) {
        return -1;
    }

    UINT read = 0;
    FRESULT res = f_read((FIL *)file, buf, (UINT)len, &read);
    if (res != FR_OK) {
        return -1;
    }

    return (int)read;
}

int hal_fs_sync(hal_file_t file) {
    if (!file) {
        return -1;
    }

    FRESULT res = f_sync((FIL *)file);
    if (res != FR_OK) {
        return -1;
    }

    return 0;
}

int hal_fs_close(hal_file_t file) {
    if (!file) {
        return -1;
    }

    FRESULT res = f_close((FIL *)file);
    free(file);
    if (res != FR_OK) {
        return -1;
    }

    return 0;
}

int hal_fs_remove(const char* path) {
    if (!path) {
        return -1;
    }

    FRESULT res = f_unlink(path);
    if (res != FR_OK) {
        return -1;
    }

    return 0;
}

bool hal_fs_exists(const char* path) {
    if (!path) {
        return false;
    }

    FILINFO fno;
    FRESULT res = f_stat(path, &fno);
    return (res == FR_OK);
}

int hal_fs_seek_end(hal_file_t file) {
    if (!file) {
        return -1;
    }

    FIL *f = (FIL *)file;
    FSIZE_t size = f_size(f);
    FRESULT res = f_lseek(f, size);
    if (res != FR_OK) {
        return -1;
    }

    return (int)size;
}

int hal_fs_read_byte_at_end(hal_file_t file) {
    if (!file) {
        return -1;
    }

    FIL *f = (FIL *)file;
    FSIZE_t pos = f_tell(f);

    /* Position must be valid and not at start unless file is empty */
    if (pos < 0) {
        return -1;
    }

    /* Seek to end - 1 to read the last byte */
    FSIZE_t size = f_size(f);
    if (size == 0) {
        return -1;
    }

    FRESULT res = f_lseek(f, size - 1);
    if (res != FR_OK) {
        return -1;
    }

    BYTE byte;
    UINT read = 0;
    res = f_read(f, &byte, 1, &read);
    if (res != FR_OK || read != 1) {
        return -1;
    }

    return (int)byte;
}

int hal_fs_size(hal_file_t file) {
    if (!file) {
        return -1;
    }

    FIL *f = (FIL *)file;
    FSIZE_t size = f_size(f);
    if (size > 2147483647) { /* MAX_INT */
        return -1;
    }

    return (int)size;
}

/* ---- Time ---- */

#include "ff.h"

uint32_t hal_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void hal_sleep_ms(uint32_t ms) {
    sleep_ms(ms);
}

/* FatFS timestamp function required by the filesystem library */
DWORD get_fattime(void) {
    /* Return a fixed timestamp (2024-01-01 00:00:00) */
    /* FAT timestamp format: bits 31-25=year-1980, bits 24-21=month, bits 20-16=day,
     * bits 15-11=hour, bits 10-5=minute, bits 4-0=second/2 */
    /* Year 2024 - 1980 = 44 */
    DWORD year = 44;
    DWORD month = 1;
    DWORD day = 1;
    DWORD hour = 0;
    DWORD minute = 0;
    DWORD second = 0;

    return ((year << 25) | (month << 21) | (day << 16) |
            (hour << 11) | (minute << 5) | (second >> 1));
}

#endif /* !HOST_BUILD */
