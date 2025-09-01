/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock GPIO interface for testing
 * 
 * This mock implementation replaces the Zephyr GPIO driver for unit testing,
 * allowing tests to simulate GPIO operations without hardware dependencies.
 * Critical for testing SX1509B GPIO expander interactions.
 */

/* Mock GPIO pin state */
struct mock_gpio_pin_state {
    bool configured;
    gpio_flags_t flags;
    bool value;
    gpio_callback_handler_t callback;
    struct gpio_callback *callback_data;
};

/* Mock GPIO device state */
struct mock_gpio_device_state {
    struct mock_gpio_pin_state pins[32]; /* Support up to 32 pins per device */
    bool device_ready;
};

/**
 * @brief Initialize mock GPIO system
 */
void mock_gpio_init(void);

/**
 * @brief Reset mock GPIO state
 */
void mock_gpio_reset(void);

/**
 * @brief Set GPIO device ready state
 * 
 * @param dev Device pointer
 * @param ready Ready state to return
 */
void mock_gpio_set_device_ready(const struct device *dev, bool ready);

/**
 * @brief Set expected GPIO pin value for reads
 * 
 * @param dev Device pointer
 * @param pin Pin number
 * @param value Expected pin value
 */
void mock_gpio_expect_pin_value(const struct device *dev, gpio_pin_t pin, int value);

/**
 * @brief Get GPIO pin value that was written
 * 
 * @param dev Device pointer
 * @param pin Pin number
 * @return Pin value that was set
 */
int mock_gpio_get_pin_value(const struct device *dev, gpio_pin_t pin);

/**
 * @brief Verify GPIO pin configuration
 * 
 * @param dev Device pointer
 * @param pin Pin number
 * @param expected_flags Expected configuration flags
 */
void mock_gpio_verify_pin_config(const struct device *dev, gpio_pin_t pin, 
                                 gpio_flags_t expected_flags);

/* Mock GPIO driver functions (when GPIO driver is disabled) */
#ifndef CONFIG_GPIO

static inline int gpio_pin_configure(const struct device *dev, gpio_pin_t pin,
                                    gpio_flags_t flags)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int gpio_pin_get(const struct device *dev, gpio_pin_t pin)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int gpio_pin_set(const struct device *dev, gpio_pin_t pin, int value)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int gpio_pin_toggle(const struct device *dev, gpio_pin_t pin)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int gpio_pin_interrupt_configure(const struct device *dev, gpio_pin_t pin,
                                              enum gpio_int_mode mode,
                                              enum gpio_int_trig trig)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int gpio_add_callback(const struct device *dev,
                                   struct gpio_callback *callback)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int gpio_remove_callback(const struct device *dev,
                                      struct gpio_callback *callback)
{
    /* Mock implementation will be called */
    return 0;
}

#endif /* !CONFIG_GPIO */

#ifdef __cplusplus
}
#endif

#endif /* MOCK_GPIO_H */