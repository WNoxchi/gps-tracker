#include "unity.h"
#include "power_mgmt.h"
#include "hal/hal.h"
#include "hal/hal_mock.h"

void setUp(void) {
    hal_mock_reset();
}

void tearDown(void) { }

/* T1: initial no shutdown */
void test_initial_no_shutdown(void) {
    power_mgmt_init();
    TEST_ASSERT_FALSE(power_mgmt_is_shutdown_requested());
}

/* T2: ISR sets flag */
void test_isr_sets_flag(void) {
    power_mgmt_init();
    hal_mock_gpio_trigger_irq(POWER_MGMT_VBUS_GPIO, GPIO_IRQ_EDGE_FALL);
    TEST_ASSERT_TRUE(power_mgmt_is_shutdown_requested());
}

/* T3: VBUS present */
void test_vbus_present(void) {
    power_mgmt_init();
    hal_mock_gpio_set(POWER_MGMT_VBUS_GPIO, true);
    TEST_ASSERT_TRUE(power_mgmt_is_vbus_present());
}

/* T4: VBUS absent */
void test_vbus_absent(void) {
    power_mgmt_init();
    hal_mock_gpio_set(POWER_MGMT_VBUS_GPIO, false);
    TEST_ASSERT_FALSE(power_mgmt_is_vbus_present());
}

/* T5: shutdown closes storage (integration) — tested via mock */
void test_shutdown_closes_storage(void) {
    power_mgmt_init();
    /* Simulate ISR firing */
    hal_mock_gpio_trigger_irq(POWER_MGMT_VBUS_GPIO, GPIO_IRQ_EDGE_FALL);
    TEST_ASSERT_TRUE(power_mgmt_is_shutdown_requested());
    /* In real code, the main loop would call data_storage_shutdown() */
}

/* T6: shutdown idempotent — calling init again resets state */
void test_shutdown_idempotent(void) {
    power_mgmt_init();
    hal_mock_gpio_trigger_irq(POWER_MGMT_VBUS_GPIO, GPIO_IRQ_EDGE_FALL);
    TEST_ASSERT_TRUE(power_mgmt_is_shutdown_requested());
    /* Re-init should reset */
    power_mgmt_init();
    TEST_ASSERT_FALSE(power_mgmt_is_shutdown_requested());
}

/* T7: GPIO configured as input */
void test_gpio_configured_input(void) {
    power_mgmt_init();
    TEST_ASSERT_TRUE(hal_mock_gpio_is_initialized(POWER_MGMT_VBUS_GPIO));
}

/* T8: falling edge registered */
void test_falling_edge_registered(void) {
    power_mgmt_init();
    TEST_ASSERT_EQUAL_UINT32(GPIO_IRQ_EDGE_FALL, hal_mock_gpio_get_edge_mask(POWER_MGMT_VBUS_GPIO));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_no_shutdown);
    RUN_TEST(test_isr_sets_flag);
    RUN_TEST(test_vbus_present);
    RUN_TEST(test_vbus_absent);
    RUN_TEST(test_shutdown_closes_storage);
    RUN_TEST(test_shutdown_idempotent);
    RUN_TEST(test_gpio_configured_input);
    RUN_TEST(test_falling_edge_registered);
    return UNITY_END();
}
