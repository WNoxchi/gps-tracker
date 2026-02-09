#include "power_mgmt.h"
#include "hal/hal.h"
#ifndef HOST_BUILD
#include "hardware/gpio.h"
#endif

static volatile bool g_power_lost = false;

static void power_loss_isr(uint32_t gpio, uint32_t events) {
    (void)gpio;
    (void)events;
    g_power_lost = true;
}

void power_mgmt_init(void) {
    g_power_lost = false;
    hal_gpio_init_input(POWER_MGMT_VBUS_GPIO);
    hal_gpio_set_irq(POWER_MGMT_VBUS_GPIO, GPIO_IRQ_EDGE_FALL, power_loss_isr);
}

bool power_mgmt_is_shutdown_requested(void) {
    return g_power_lost;
}

bool power_mgmt_is_vbus_present(void) {
    return hal_gpio_read(POWER_MGMT_VBUS_GPIO);
}
