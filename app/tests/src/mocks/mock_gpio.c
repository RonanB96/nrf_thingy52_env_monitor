/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "mock_gpio.h"

LOG_MODULE_REGISTER(mock_gpio, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Mock GPIO driver for testing
 *
 * This file provides mock implementations for GPIO operations
 * used by the sensor drivers and power management, particularly
 * the SX1509B GPIO expander operations.
 */

/* Mock GPIO state */
#define MAX_GPIO_DEVICES 4

static struct {
	struct mock_gpio_device_state devices[MAX_GPIO_DEVICES];
	int device_count;
} mock_gpio_state;

void mock_gpio_init(void)
{
	LOG_DBG("Mock GPIO system initialized");
	memset(&mock_gpio_state, 0, sizeof(mock_gpio_state));

	/* Set all devices ready by default */
	for (int i = 0; i < MAX_GPIO_DEVICES; i++) {
		mock_gpio_state.devices[i].device_ready = true;
	}
}

void mock_gpio_reset(void)
{
	LOG_DBG("Mock GPIO state reset");

	/* Reset pin states but keep devices ready */
	for (int i = 0; i < MAX_GPIO_DEVICES; i++) {
		memset(mock_gpio_state.devices[i].pins, 0, sizeof(mock_gpio_state.devices[i].pins));
		mock_gpio_state.devices[i].device_ready = true;
	}
}

static int get_device_index(const struct device *dev)
{
	/* Simple hash based on device pointer */
	return ((uintptr_t)dev / sizeof(struct device)) % MAX_GPIO_DEVICES;
}

void mock_gpio_set_device_ready(const struct device *dev, bool ready)
{
	int device_index = get_device_index(dev);
	mock_gpio_state.devices[device_index].device_ready = ready;

	LOG_DBG("Set mock GPIO device %p ready state to %s", dev, ready ? "true" : "false");
}

void mock_gpio_expect_pin_value(const struct device *dev, gpio_pin_t pin, int value)
{
	int device_index = get_device_index(dev);

	zassert_true(pin < ARRAY_SIZE(mock_gpio_state.devices[device_index].pins),
		     "GPIO pin %d out of range", pin);

	mock_gpio_state.devices[device_index].pins[pin].value = (value != 0);

	LOG_DBG("Set expected GPIO pin %d value to %d", pin, value);
}

int mock_gpio_get_pin_value(const struct device *dev, gpio_pin_t pin)
{
	int device_index = get_device_index(dev);

	zassert_true(pin < ARRAY_SIZE(mock_gpio_state.devices[device_index].pins),
		     "GPIO pin %d out of range", pin);

	bool value = mock_gpio_state.devices[device_index].pins[pin].value;
	LOG_DBG("Get GPIO pin %d value: %d", pin, value);

	return value ? 1 : 0;
}

void mock_gpio_verify_pin_config(const struct device *dev, gpio_pin_t pin,
				 gpio_flags_t expected_flags)
{
	int device_index = get_device_index(dev);

	zassert_true(pin < ARRAY_SIZE(mock_gpio_state.devices[device_index].pins),
		     "GPIO pin %d out of range", pin);

	struct mock_gpio_pin_state *pin_state = &mock_gpio_state.devices[device_index].pins[pin];

	zassert_true(pin_state->configured, "GPIO pin %d was not configured", pin);
	zassert_equal(pin_state->flags, expected_flags,
		      "GPIO pin %d configuration mismatch. Expected: 0x%x, Actual: 0x%x", pin,
		      expected_flags, pin_state->flags);

	LOG_DBG("Verified GPIO pin %d configuration: 0x%x", pin, expected_flags);
}

/* Mock implementations for when GPIO driver is disabled */
#ifndef CONFIG_GPIO

int gpio_pin_configure_mock(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	int device_index = get_device_index(dev);

	LOG_DBG("Mock GPIO configure: dev=%p, pin=%d, flags=0x%x", dev, pin, flags);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		LOG_WRN("GPIO device not ready");
		return -ENODEV;
	}

	if (pin >= ARRAY_SIZE(mock_gpio_state.devices[device_index].pins)) {
		LOG_ERR("GPIO pin %d out of range", pin);
		return -EINVAL;
	}

	struct mock_gpio_pin_state *pin_state = &mock_gpio_state.devices[device_index].pins[pin];

	pin_state->configured = true;
	pin_state->flags = flags;

	return 0;
}

int gpio_pin_get_mock(const struct device *dev, gpio_pin_t pin)
{
	int device_index = get_device_index(dev);

	LOG_DBG("Mock GPIO get: dev=%p, pin=%d", dev, pin);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		return -ENODEV;
	}

	if (pin >= ARRAY_SIZE(mock_gpio_state.devices[device_index].pins)) {
		return -EINVAL;
	}

	return mock_gpio_state.devices[device_index].pins[pin].value ? 1 : 0;
}

int gpio_pin_set_mock(const struct device *dev, gpio_pin_t pin, int value)
{
	int device_index = get_device_index(dev);

	LOG_DBG("Mock GPIO set: dev=%p, pin=%d, value=%d", dev, pin, value);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		return -ENODEV;
	}

	if (pin >= ARRAY_SIZE(mock_gpio_state.devices[device_index].pins)) {
		return -EINVAL;
	}

	mock_gpio_state.devices[device_index].pins[pin].value = (value != 0);

	return 0;
}

int gpio_pin_toggle_mock(const struct device *dev, gpio_pin_t pin)
{
	int device_index = get_device_index(dev);

	LOG_DBG("Mock GPIO toggle: dev=%p, pin=%d", dev, pin);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		return -ENODEV;
	}

	if (pin >= ARRAY_SIZE(mock_gpio_state.devices[device_index].pins)) {
		return -EINVAL;
	}

	bool current_value = mock_gpio_state.devices[device_index].pins[pin].value;
	mock_gpio_state.devices[device_index].pins[pin].value = !current_value;

	return 0;
}

int gpio_pin_interrupt_configure_mock(const struct device *dev, gpio_pin_t pin,
				      enum gpio_int_mode mode, enum gpio_int_trig trig)
{
	int device_index = get_device_index(dev);

	LOG_DBG("Mock GPIO interrupt configure: dev=%p, pin=%d, mode=%d, trig=%d", dev, pin, mode,
		trig);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		return -ENODEV;
	}

	if (pin >= ARRAY_SIZE(mock_gpio_state.devices[device_index].pins)) {
		return -EINVAL;
	}

	/* Just mark as configured for now */
	mock_gpio_state.devices[device_index].pins[pin].configured = true;

	return 0;
}

int gpio_add_callback_mock(const struct device *dev, struct gpio_callback *callback)
{
	LOG_DBG("Mock GPIO add callback: dev=%p, callback=%p", dev, callback);

	/* Store callback for potential triggering in tests */
	int device_index = get_device_index(dev);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		return -ENODEV;
	}

	/* Simple implementation - just store the callback */
	/* Real implementation would need more sophisticated callback management */

	return 0;
}

int gpio_remove_callback_mock(const struct device *dev, struct gpio_callback *callback)
{
	LOG_DBG("Mock GPIO remove callback: dev=%p, callback=%p", dev, callback);

	int device_index = get_device_index(dev);

	if (!mock_gpio_state.devices[device_index].device_ready) {
		return -ENODEV;
	}

	return 0;
}

#endif /* !CONFIG_GPIO */