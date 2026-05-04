/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/devicetree.h>
#include <stdbool.h>
#include <string.h>

#include "sensor_hts221_driver.h"

/* Include ST's official HTS221 register API */
#include "hts221.h"

LOG_MODULE_REGISTER(sensor_hts221_driver, CONFIG_LOG_DEFAULT_LEVEL);

/* Device tree node */
#define HTS221_NODE DT_NODELABEL(hts221)

static const uint32_t HTS221_ONE_SHOT_STABILIZATION_MS = 20U;
static const uint32_t HTS221_DRDY_TIMEOUT_MS = 100U; /* Data ready semaphore timeout */

/* Static variables */
static const struct device *hts221_dev;
static const struct device *i2c_dev;
static const stmdev_ctx_t *stm_ctx;
static bool hts221_enabled = false;

/* Semaphore for waiting on data ready interrupt */
static K_SEM_DEFINE(hts221_data_ready_sem, 0, 1);

/* Data ready trigger callback */
static void hts221_data_ready_handler(const struct device *dev,
				      const struct sensor_trigger *trigger)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trigger);

	/* Signal that data is ready */
	k_sem_give(&hts221_data_ready_sem);
}

int hts221_driver_init(const struct device *i2c_device)
{
	int ret;

	if (!i2c_device || !device_is_ready(i2c_device)) {
		LOG_ERR("Invalid or not ready I2C device");
		return -EINVAL;
	}

	/* Get HTS221 device from device tree */
	hts221_dev = DEVICE_DT_GET(HTS221_NODE);
	if (!hts221_dev || !device_is_ready(hts221_dev)) {
		LOG_ERR("HTS221 device not found or not ready");
		return -ENODEV;
	}

	i2c_dev = i2c_device;

	const struct hts221_config *cfg = hts221_dev->config;
	stm_ctx = (stmdev_ctx_t *)&cfg->ctx;

	/* Set up data ready trigger */
	struct sensor_trigger trigger = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	ret = sensor_trigger_set(hts221_dev, &trigger, hts221_data_ready_handler);
	if (ret != 0) {
		LOG_WRN("Failed to set HTS221 trigger: %d (continuing without interrupt)", ret);
	} else {
		LOG_INF("HTS221 data ready trigger configured");
	}

	LOG_INF("HTS221 driver initialized");
	return 0;
}

int hts221_driver_power_on(void)
{
	int32_t ret;

	if (!i2c_dev) {
		LOG_ERR("I2C device not initialized");
		return -ENODEV;
	}

	if (hts221_enabled) {
		return 0; /* Already powered on */
	}

	/* Power on sensor using ST register API */
	ret = hts221_power_on_set(stm_ctx, PROPERTY_ENABLE);
	if (ret != 0) {
		LOG_ERR("Failed to power on HTS221: %d", ret);
		return ret;
	}

	hts221_enabled = true;
	LOG_DBG("HTS221 powered on for one-shot measurement");
	return 0;
}

int hts221_driver_power_off(void)
{
	int32_t ret;

	if (!i2c_dev) {
		LOG_ERR("I2C device not initialized");
		return -ENODEV;
	}

	if (!hts221_enabled) {
		return 0; /* Already powered off */
	}

	/* Power down sensor using ST register API */
	ret = hts221_power_on_set(stm_ctx, PROPERTY_DISABLE);
	if (ret != 0) {
		LOG_ERR("Failed to power off HTS221: %d", ret);
		return ret;
	}

	hts221_enabled = false;
	LOG_DBG("HTS221 powered down");
	return 0;
}

bool hts221_driver_is_enabled(void)
{
	return hts221_enabled;
}

int hts221_driver_read_temperature(float *temperature)
{
	return hts221_driver_read_both(temperature, NULL);
}

int hts221_driver_read_humidity(float *humidity)
{
	return hts221_driver_read_both(NULL, humidity);
}

int hts221_driver_read_both(float *temperature, float *humidity)
{
	struct sensor_value temp_val;
	struct sensor_value hum_val;
	int ret;

	if (!hts221_dev || (!temperature && !humidity)) {
		return -EINVAL;
	}

	if (!device_is_ready(hts221_dev)) {
		LOG_WRN("HTS221 device not ready");
		return -ENODEV;
	}

	/* Power on sensor */
	ret = hts221_driver_power_on();
	if (ret != 0) {
		LOG_ERR("Failed to power on HTS221: %d", ret);
		return ret;
	}

	/* Wait for sensor stabilization */
	k_msleep((int32_t)HTS221_ONE_SHOT_STABILIZATION_MS);

	/* Trigger one-shot measurement using ST register API */
	ret = hts221_one_shoot_trigger_set(stm_ctx, PROPERTY_ENABLE);
	if (ret != 0) {
		LOG_ERR("Failed to trigger HTS221 one-shot: %d", ret);
		goto power_down;
	}

	/* Wait for data ready interrupt (with timeout) */
	ret = k_sem_take(&hts221_data_ready_sem, K_MSEC(HTS221_DRDY_TIMEOUT_MS));
	if (ret != 0) {
		LOG_ERR("HTS221 measurement timeout waiting for interrupt");
		ret = -ETIMEDOUT;
		goto power_down;
	}

	LOG_DBG("HTS221 measurement complete (interrupt received)");

	/* Now fetch the fresh sample using Zephyr driver */
	ret = sensor_sample_fetch(hts221_dev);
	if (ret != 0) {
		LOG_ERR("Failed to fetch HTS221 sample: %d", ret);
		goto power_down;
	}

	/* Read temperature if requested */
	if (temperature) {
		ret = sensor_channel_get(hts221_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_val);
		if (ret == 0) {
			*temperature = sensor_value_to_float(&temp_val);
			LOG_DBG("Temperature: %.2f °C", (double)*temperature);
		} else {
			LOG_WRN("Failed to read temperature: %d", ret);
			goto power_down;
		}
	}

	/* Read humidity if requested */
	if (humidity) {
		ret = sensor_channel_get(hts221_dev, SENSOR_CHAN_HUMIDITY, &hum_val);
		if (ret == 0) {
			*humidity = sensor_value_to_float(&hum_val);
			LOG_DBG("Humidity: %.2f %%", (double)*humidity);
		} else {
			LOG_WRN("Failed to read humidity: %d", ret);
			goto power_down;
		}
	}

power_down:
	/* Always power down after reading */
	{
		int power_ret = hts221_driver_power_off();
		if (power_ret != 0) {
			LOG_WRN("Failed to power off HTS221: %d", power_ret);
		}
	}

	return ret;
}
