#ifndef HOST_BUILD

#include "hal/hal.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

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

/* ---- Filesystem (stub — requires FatFs integration) ---- */

int hal_fs_mount(void)   { return -1; }
int hal_fs_unmount(void) { return 0; }
hal_file_t hal_fs_open(const char* path, const char* mode) { (void)path; (void)mode; return NULL; }
int hal_fs_write(hal_file_t file, const void* buf, size_t len) { (void)file; (void)buf; (void)len; return -1; }
int hal_fs_read(hal_file_t file, void* buf, size_t len) { (void)file; (void)buf; (void)len; return -1; }
int hal_fs_sync(hal_file_t file) { (void)file; return -1; }
int hal_fs_close(hal_file_t file) { (void)file; return -1; }
int hal_fs_remove(const char* path) { (void)path; return -1; }
bool hal_fs_exists(const char* path) { (void)path; return false; }
int hal_fs_seek_end(hal_file_t file) { (void)file; return -1; }
int hal_fs_read_byte_at_end(hal_file_t file) { (void)file; return -1; }
int hal_fs_size(hal_file_t file) { (void)file; return -1; }

/* ---- Time ---- */

uint32_t hal_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void hal_sleep_ms(uint32_t ms) {
    sleep_ms(ms);
}

#endif /* !HOST_BUILD */
