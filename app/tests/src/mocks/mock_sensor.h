/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_SENSOR_H
#define MOCK_SENSOR_H

#include <zephyr/ztest.h>
#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock sensor interface for testing
 * 
 * This mock implementation replaces the Zephyr sensor driver for unit testing,
 * allowing tests to simulate sensor readings and power states without hardware.
 */

/* Mock sensor reading data */
struct mock_sensor_reading {
    enum sensor_channel channel;
    struct sensor_value value;
    bool valid;
};

/* Mock sensor device state */
struct mock_sensor_device_state {
    bool device_ready;
    bool sample_fetch_success;
    bool power_on;
    struct mock_sensor_reading readings[8]; /* Support multiple channels */
    int reading_count;
    int fetch_call_count;
    int get_call_count;
};

/**
 * @brief Initialize mock sensor system
 */
void mock_sensor_init(void);

/**
 * @brief Reset mock sensor state
 */
void mock_sensor_reset(void);

/**
 * @brief Set sensor device ready state
 * 
 * @param dev Device pointer
 * @param ready Ready state to return
 */
void mock_sensor_set_device_ready(const struct device *dev, bool ready);

/**
 * @brief Set expected sensor reading value
 * 
 * @param dev Device pointer
 * @param channel Sensor channel
 * @param value Expected sensor value
 */
void mock_sensor_set_reading(const struct device *dev, enum sensor_channel channel,
                            const struct sensor_value *value);

/**
 * @brief Set sensor sample fetch result
 * 
 * @param dev Device pointer
 * @param success Whether sample fetch should succeed
 */
void mock_sensor_set_fetch_result(const struct device *dev, bool success);

/**
 * @brief Mock sensor sample fetch function
 * 
 * @param dev Device pointer
 * @return 0 on success, error code on failure
 */
int sensor_sample_fetch_mock(const struct device *dev);

/**
 * @brief Mock sensor channel get function
 * 
 * @param dev Device pointer
 * @param chan Sensor channel
 * @param val Sensor value pointer
 * @return 0 on success, error code on failure
 */
int sensor_channel_get_mock(const struct device *dev, enum sensor_channel chan,
                           struct sensor_value *val);

/**
 * @brief Get number of times sensor_sample_fetch was called
 * 
 * @param dev Device pointer
 * @return Number of fetch calls
 */
int mock_sensor_get_fetch_call_count(const struct device *dev);

/**
 * @brief Get number of times sensor_channel_get was called
 * 
 * @param dev Device pointer
 * @return Number of get calls
 */
int mock_sensor_get_get_call_count(const struct device *dev);

/**
 * @brief Simulate sensor power state
 * 
 * @param dev Device pointer
 * @param powered Whether sensor is powered
 */
void mock_sensor_set_power_state(const struct device *dev, bool powered);

/* Note: With CONFIG_SENSOR enabled, we use the real Zephyr sensor API 
 * but provide mock implementations. The inline utility functions are 
 * already defined in zephyr/drivers/sensor.h. */

#ifdef __cplusplus
}
#endif

#endif /* MOCK_SENSOR_H */