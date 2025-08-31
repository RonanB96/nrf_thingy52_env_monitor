/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_HTS221_DRIVER_H_
#define SENSOR_HTS221_DRIVER_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize HTS221 driver with I2C device
 *
 * @param i2c_dev I2C device for direct register access
 * @return 0 on success, negative error code on failure
 */
int hts221_driver_init(const struct device *i2c_dev);

/**
 * @brief Power on HTS221 sensor for one-shot measurement
 *
 * Configures the sensor for one-shot mode and powers it on.
 * Power consumption: ~10µA during measurement, ~0.5µA when off.
 *
 * @return 0 on success, negative error code on failure
 */
int hts221_driver_power_on(void);

/**
 * @brief Power off HTS221 sensor to save power
 *
 * Puts the sensor into power-down mode for ultra-low power consumption.
 *
 * @return 0 on success, negative error code on failure
 */
int hts221_driver_power_off(void);

/**
 * @brief Check if HTS221 sensor is currently powered on
 *
 * @return true if sensor is powered on, false otherwise
 */
bool hts221_driver_is_enabled(void);

/**
 * @brief Read temperature from HTS221 sensor
 *
 * Handles power management automatically - powers on, reads, powers off.
 *
 * @param temperature Pointer to store temperature value in Celsius
 * @return 0 on success, negative error code on failure
 */
int hts221_driver_read_temperature(float *temperature);

/**
 * @brief Read humidity from HTS221 sensor
 *
 * Handles power management automatically - powers on, reads, powers off.
 *
 * @param humidity Pointer to store humidity value in percent
 * @return 0 on success, negative error code on failure
 */
int hts221_driver_read_humidity(float *humidity);

/**
 * @brief Read both temperature and humidity from HTS221 sensor
 *
 * More efficient than reading separately as it only powers on once.
 *
 * @param temperature Pointer to store temperature value in Celsius (can be NULL)
 * @param humidity Pointer to store humidity value in percent (can be NULL)
 * @return 0 on success, negative error code on failure
 */
int hts221_driver_read_both(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HTS221_DRIVER_H_ */
