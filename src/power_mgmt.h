#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <stdbool.h>
#include <stdint.h>

#define POWER_MGMT_VBUS_GPIO       24
#define POWER_SHUTDOWN_TIMEOUT_MS  500
#ifdef HOST_BUILD
#define GPIO_IRQ_EDGE_FALL         0x04u
#endif

void power_mgmt_init(void);
bool power_mgmt_is_shutdown_requested(void);
bool power_mgmt_is_vbus_present(void);

#endif
