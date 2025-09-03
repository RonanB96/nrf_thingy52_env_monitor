/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "mock_sensor.h"

LOG_MODULE_REGISTER(mock_sensor, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Mock sensor driver for testing
 * 
 * This file provides mock implementations for Zephyr sensor API
 * used by the environmental sensors (HTS221, LPS22HB, CCS811).
 */

/* Mock sensor state */
#define MAX_SENSOR_DEVICES 8

static struct {
    struct mock_sensor_device_state devices[MAX_SENSOR_DEVICES];
    int device_count;
} mock_sensor_state;

void mock_sensor_init(void)
{
    LOG_DBG("Mock sensor system initialized");
    memset(&mock_sensor_state, 0, sizeof(mock_sensor_state));
    
    /* Set all devices ready by default */
    for (int i = 0; i < MAX_SENSOR_DEVICES; i++) {
        mock_sensor_state.devices[i].device_ready = true;
        mock_sensor_state.devices[i].sample_fetch_success = true;
        mock_sensor_state.devices[i].power_on = true;
    }
}

void mock_sensor_reset(void)
{
    LOG_DBG("Mock sensor state reset");
    
    /* Reset device states but keep them ready */
    for (int i = 0; i < MAX_SENSOR_DEVICES; i++) {
        mock_sensor_state.devices[i].device_ready = true;
        mock_sensor_state.devices[i].sample_fetch_success = true;
        mock_sensor_state.devices[i].power_on = true;
        mock_sensor_state.devices[i].reading_count = 0;
        mock_sensor_state.devices[i].fetch_call_count = 0;
        mock_sensor_state.devices[i].get_call_count = 0;
        memset(mock_sensor_state.devices[i].readings, 0,
               sizeof(mock_sensor_state.devices[i].readings));
    }
}

static int get_device_index(const struct device *dev)
{
    /* Simple hash based on device pointer */
    return ((uintptr_t)dev / sizeof(struct device)) % MAX_SENSOR_DEVICES;
}

void mock_sensor_set_device_ready(const struct device *dev, bool ready)
{
    int device_index = get_device_index(dev);
    mock_sensor_state.devices[device_index].device_ready = ready;
    
    LOG_DBG("Set mock sensor device %p ready state to %s", dev, ready ? "true" : "false");
}

void mock_sensor_set_reading(const struct device *dev, enum sensor_channel channel,
                           const struct sensor_value *value)
{
    int device_index = get_device_index(dev);
    struct mock_sensor_device_state *device = &mock_sensor_state.devices[device_index];
    
    /* Find existing reading for this channel or add new one */
    int reading_index = -1;
    for (int i = 0; i < device->reading_count; i++) {
        if (device->readings[i].channel == channel) {
            reading_index = i;
            break;
        }
    }
    
    if (reading_index == -1) {
        /* Add new reading */
        zassert_true(device->reading_count < ARRAY_SIZE(device->readings),
                     "Too many sensor readings for device");
        reading_index = device->reading_count++;
    }
    
    device->readings[reading_index].channel = channel;
    device->readings[reading_index].value = *value;
    device->readings[reading_index].valid = true;
    
    float fval = sensor_value_to_float(value);
    LOG_DBG("Set sensor reading: dev=%p, channel=%d, value=%.2f", dev, channel, (double)fval);
}

void mock_sensor_set_fetch_result(const struct device *dev, bool success)
{
    int device_index = get_device_index(dev);
    mock_sensor_state.devices[device_index].sample_fetch_success = success;
    
    LOG_DBG("Set sensor fetch result: dev=%p, success=%s", dev, success ? "true" : "false");
}

int mock_sensor_get_fetch_call_count(const struct device *dev)
{
    int device_index = get_device_index(dev);
    return mock_sensor_state.devices[device_index].fetch_call_count;
}

int mock_sensor_get_get_call_count(const struct device *dev)
{
    int device_index = get_device_index(dev);
    return mock_sensor_state.devices[device_index].get_call_count;
}

void mock_sensor_set_power_state(const struct device *dev, bool powered)
{
    int device_index = get_device_index(dev);
    mock_sensor_state.devices[device_index].power_on = powered;
    
    LOG_DBG("Set sensor power state: dev=%p, powered=%s", dev, powered ? "true" : "false");
}

/* Mock implementations for when SENSOR driver is disabled */
#ifndef CONFIG_SENSOR

int sensor_sample_fetch_mock(const struct device *dev)
{
    int device_index = get_device_index(dev);
    struct mock_sensor_device_state *device = &mock_sensor_state.devices[device_index];
    
    LOG_DBG("Mock sensor sample fetch: dev=%p", dev);
    
    device->fetch_call_count++;
    
    if (!device->device_ready) {
        LOG_WRN("Sensor device not ready");
        return -ENODEV;
    }
    
    if (!device->power_on) {
        LOG_WRN("Sensor device not powered");
        return -EIO;
    }
    
    if (!device->sample_fetch_success) {
        LOG_WRN("Sensor fetch configured to fail");
        return -EIO;
    }
    
    return 0;
}

int sensor_sample_fetch_chan_mock(const struct device *dev, enum sensor_channel type)
{
    LOG_DBG("Mock sensor sample fetch chan: dev=%p, chan=%d", dev, type);
    
    /* For now, just call the main fetch function */
    return sensor_sample_fetch_mock(dev);
}

int sensor_channel_get_mock(const struct device *dev, enum sensor_channel chan,
                           struct sensor_value *val)
{
    int device_index = get_device_index(dev);
    struct mock_sensor_device_state *device = &mock_sensor_state.devices[device_index];
    
    LOG_DBG("Mock sensor channel get: dev=%p, chan=%d", dev, chan);
    
    device->get_call_count++;
    
    if (!device->device_ready) {
        return -ENODEV;
    }
    
    if (!device->power_on) {
        return -EIO;
    }
    
    /* Find reading for this channel */
    for (int i = 0; i < device->reading_count; i++) {
        if (device->readings[i].channel == chan && device->readings[i].valid) {
            *val = device->readings[i].value;
            float fval = sensor_value_to_float(val);
            LOG_DBG("Returning sensor value: %.2f", (double)fval);
            return 0;
        }
    }
    
    /* No reading found for this channel */
    LOG_WRN("No reading available for channel %d", chan);
    return -ENODATA;
}

int sensor_attr_set_mock(const struct device *dev, enum sensor_channel chan,
                        enum sensor_attribute attr, const struct sensor_value *val)
{
    int device_index = get_device_index(dev);
    struct mock_sensor_device_state *device = &mock_sensor_state.devices[device_index];
    
    LOG_DBG("Mock sensor attr set: dev=%p, chan=%d, attr=%d", dev, chan, attr);
    
    if (!device->device_ready) {
        return -ENODEV;
    }
    
    /* Just acknowledge the set operation */
    return 0;
}

int sensor_attr_get_mock(const struct device *dev, enum sensor_channel chan,
                        enum sensor_attribute attr, struct sensor_value *val)
{
    int device_index = get_device_index(dev);
    struct mock_sensor_device_state *device = &mock_sensor_state.devices[device_index];
    
    LOG_DBG("Mock sensor attr get: dev=%p, chan=%d, attr=%d", dev, chan, attr);
    
    if (!device->device_ready) {
        return -ENODEV;
    }
    
    /* Return some default value */
    val->val1 = 0;
    val->val2 = 0;
    
    return 0;
}

#endif /* !CONFIG_SENSOR */