#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#ifdef HOST_BUILD

#include <stdint.h>
#include <stdbool.h>

void hal_mock_reset(void);
void hal_mock_uart_set_data(const char* nmea_data);
void hal_mock_gpio_set(uint32_t pin, bool value);
void hal_mock_gpio_trigger_irq(uint32_t pin, uint32_t events);
bool hal_mock_gpio_is_initialized(uint32_t pin);
uint32_t hal_mock_gpio_get_edge_mask(uint32_t pin);
void hal_mock_time_set_ms(uint32_t ms);
void hal_mock_time_advance_ms(uint32_t ms);
void hal_mock_fs_set_root(const char* path);

#endif /* HOST_BUILD */

#endif /* HAL_MOCK_H */
