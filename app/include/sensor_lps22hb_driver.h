/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_LPS22HB_DRIVER_H_
#define SENSOR_LPS22HB_DRIVER_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LPS22HB driver with I2C and interrupt GPIO
 *
 * @param i2c_dev I2C device for direct register access
 * @param int_spec GPIO spec for data ready interrupt pin
 * @return 0 on success, negative error code on failure
 */
int lps22hb_driver_init(const struct device *i2c_dev, const struct gpio_dt_spec *int_spec);

/**
 * @brief Power on LPS22HB sensor and enable data ready interrupt
 *
 * Configures the sensor for one-shot mode with data ready interrupt enabled.
 * Power consumption: ~15µA during measurement, ~1µA when off.
 *
 * @return 0 on success, negative error code on failure
 */
int lps22hb_driver_power_on(void);

/**
 * @brief Trigger one-shot pressure measurement
 *
 * Triggers a single pressure and temperature measurement.
 * Data ready interrupt will fire when measurement is complete.
 *
 * @return 0 on success, negative error code on failure
 */
int lps22hb_driver_trigger_oneshot(void);

/**
 * @brief Wait for data ready with timeout
 *
 * Waits for the data ready interrupt using a semaphore.
 *
 * @param timeout Maximum time to wait as k_timeout_t (e.g. K_MSEC(100))
 * @return 0 on success, -ETIMEDOUT on timeout, other negative error codes
 */
int lps22hb_driver_wait_data_ready(k_timeout_t timeout);

/**
 * @brief Power off LPS22HB sensor to save power
 *
 * Puts the sensor into power-down mode and disables interrupts.
 *
 * @return 0 on success, negative error code on failure
 */
int lps22hb_driver_power_off(void);

/**
 * @brief Check if LPS22HB sensor is currently powered on
 *
 * @return true if sensor is powered on, false otherwise
 */
bool lps22hb_driver_is_enabled(void);

/**
 * @brief Read pressure from LPS22HB sensor
 *
 * Handles power management and interrupt handling automatically.
 *
 * @param pressure Pointer to store pressure value in kPa
 * @return 0 on success, negative error code on failure
 */
int lps22hb_driver_read_pressure(float *pressure);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_LPS22HB_DRIVER_H_ */
