#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* UART */
void hal_uart_init(uint32_t baud_rate);
int hal_uart_read_line(char* buf, size_t buf_size, uint32_t timeout_ms);

/* GPIO */
void hal_gpio_init_input(uint32_t pin);
bool hal_gpio_read(uint32_t pin);
typedef void (*hal_gpio_irq_callback_t)(uint32_t pin, uint32_t events);
void hal_gpio_set_irq(uint32_t pin, uint32_t edge_mask, hal_gpio_irq_callback_t cb);

/* Filesystem */
typedef void* hal_file_t;
int hal_fs_mount(void);
int hal_fs_unmount(void);
hal_file_t hal_fs_open(const char* path, const char* mode);
int hal_fs_write(hal_file_t file, const void* buf, size_t len);
int hal_fs_read(hal_file_t file, void* buf, size_t len);
int hal_fs_sync(hal_file_t file);
int hal_fs_close(hal_file_t file);
int hal_fs_remove(const char* path);
bool hal_fs_exists(const char* path);
int hal_fs_seek_end(hal_file_t file);
int hal_fs_read_byte_at_end(hal_file_t file);
int hal_fs_size(hal_file_t file);

/* Time */
uint32_t hal_time_ms(void);
void hal_sleep_ms(uint32_t ms);

#endif
