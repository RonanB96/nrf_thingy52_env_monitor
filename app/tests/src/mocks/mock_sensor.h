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

/* Mock sensor driver functions (when SENSOR driver is disabled) */
#ifndef CONFIG_SENSOR

static inline int sensor_sample_fetch(const struct device *dev)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int sensor_sample_fetch_chan(const struct device *dev,
                                          enum sensor_channel type)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int sensor_channel_get(const struct device *dev,
                                    enum sensor_channel chan,
                                    struct sensor_value *val)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int sensor_attr_set(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 const struct sensor_value *val)
{
    /* Mock implementation will be called */
    return 0;
}

static inline int sensor_attr_get(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 struct sensor_value *val)
{
    /* Mock implementation will be called */
    return 0;
}

/* Sensor value utility functions */
static inline float sensor_value_to_float(const struct sensor_value *val)
{
    return (float)val->val1 + (float)val->val2 / 1000000.0f;
}

static inline double sensor_value_to_double(const struct sensor_value *val)
{
    return (double)val->val1 + (double)val->val2 / 1000000.0;
}

static inline int sensor_value_from_float(struct sensor_value *val, float fval)
{
    val->val1 = (int32_t)fval;
    val->val2 = (int32_t)((fval - (float)val->val1) * 1000000.0f);
    return 0;
}

static inline int sensor_value_from_double(struct sensor_value *val, double dval)
{
    val->val1 = (int32_t)dval;
    val->val2 = (int32_t)((dval - (double)val->val1) * 1000000.0);
    return 0;
}

#endif /* !CONFIG_SENSOR */

#ifdef __cplusplus
}
#endif

#endif /* MOCK_SENSOR_H */