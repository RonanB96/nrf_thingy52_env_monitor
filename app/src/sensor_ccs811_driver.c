/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sensor_ccs811_driver.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <zephyr/settings/settings.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* Include CCS811 driver internals for register definitions and mode constants */
#include "../../../../modules/zephyr/drivers/sensor/ams/ccs811/ccs811.h"

LOG_MODULE_REGISTER(ccs811_driver, CONFIG_LOG_DEFAULT_LEVEL);

/* Static state */
static const struct device *ccs811_dev = NULL;
static bool ccs811_enabled = false;
static bool ccs811_conditioning_complete = false;
static int64_t ccs811_init_time = 0;

/* CCS811 timing constants */
#define CCS811_CONDITIONING_TIME_MS  (20 * 60 * 1000) /* 20 minutes */
#define CCS811_RESET_DELAY_MS        1                /* Reset assertion time */
#define CCS811_BOOT_DELAY_MS         20               /* Boot completion time */
#define CCS811_WAKE_DELAY_US         20               /* t_DWAKE specification */
#define CCS811_STATUS_CHECK_DELAY_MS 100              /* Status check delay */

/* Adaptive sampling state */
static bool first_sample_obtained;
static uint16_t cached_co2_ppm;
static uint16_t cached_tvoc_ppb;
static int64_t last_sample_time;

/* Power management: track whether sensor is in IDLE mode between reads */
static bool ccs811_in_idle_mode;

/* Baseline management for long-term accuracy */
static uint16_t stored_baseline;
static int64_t last_baseline_save;
static int64_t last_baseline_load;
#define BASELINE_SAVE_INTERVAL_MS (24 * 60 * 60 * 1000)     /* 24 hours */
#define BASELINE_LOAD_INTERVAL_MS (7 * 24 * 60 * 60 * 1000) /* 7 days */

#define CCS811_SETTINGS_KEY "ccs811/baseline"

static int ccs811_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (strcmp(key, "baseline") == 0) {
		if (len != sizeof(stored_baseline)) {
			return -EINVAL;
		}

		return read_cb(cb_arg, &stored_baseline, sizeof(stored_baseline));
	}

	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(ccs811, "ccs811", NULL, ccs811_settings_set, NULL, NULL);

/* Thread safety mutex for CCS811 operations */
static K_MUTEX_DEFINE(ccs811_mutex);

int ccs811_driver_init(const struct device *ccs811_device)
{
	if (!ccs811_device) {
		LOG_ERR("NULL CCS811 device pointer");
		return -EINVAL;
	}

	if (!device_is_ready(ccs811_device)) {
		LOG_ERR("CCS811 device not ready");
		return -ENODEV;
	}

	/* Thread-safe initialization */
	int ret = k_mutex_lock(&ccs811_mutex, K_MSEC(100));
	if (ret != 0) {
		LOG_ERR("Failed to acquire CCS811 mutex for init: %d", ret);
		return ret;
	}

	/* Check if already initialized */
	if (ccs811_dev != NULL) {
		LOG_WRN("CCS811 driver already initialized");
		k_mutex_unlock(&ccs811_mutex);
		return 0;
	}

	ccs811_dev = ccs811_device;
	ccs811_enabled = true;
	ccs811_conditioning_complete = false;
	ccs811_init_time = k_uptime_get();

	/* Reset adaptive sampling state */
	first_sample_obtained = false;
	ccs811_in_idle_mode = false;
	cached_co2_ppm = 0;
	cached_tvoc_ppb = 0;
	last_sample_time = 0;

	k_mutex_unlock(&ccs811_mutex);

	LOG_INF("CCS811 driver initialized - starting conditioning period");

	/* Start in 1-second mode for initial quick sampling */
	int mode_ret = ccs811_mode_update(ccs811_dev, CCS811_MEASUREMENT_1SEC);
	if (mode_ret != 0) {
		LOG_WRN("Failed to set initial 1-second mode: %d", mode_ret);
	} else {
		LOG_INF("CCS811 started in 1-second mode for initial sampling");
	}

	int baseline = ccs811_baseline_fetch(ccs811_dev);
	if (baseline > 0) {
		LOG_INF("CCS811 baseline fetched: 0x%04x", baseline);
	} else {
		LOG_WRN("Failed to fetch CCS811 baseline");
	}

	/* Restore saved baseline if available for improved accuracy */
	if (stored_baseline != 0) {
		mode_ret = ccs811_driver_restore_baseline();
		if (mode_ret != 0) {
			LOG_WRN("Failed to restore baseline: %d", mode_ret);
		}
	}

	return 0;
}

bool ccs811_driver_is_ready(void)
{
	if (ccs811_conditioning_complete) {
		return true;
	}

	if (ccs811_init_time == 0) {
		/* Not initialized yet */
		return false;
	}

	int64_t elapsed = k_uptime_get() - ccs811_init_time;
	if (elapsed >= (int64_t)CCS811_CONDITIONING_TIME_MS) {
		ccs811_conditioning_complete = true;
		LOG_INF("CCS811 conditioning complete after %lld ms", elapsed);
		return true;
	}

	LOG_DBG("CCS811 still conditioning: %lld/%d ms", elapsed, CCS811_CONDITIONING_TIME_MS);
	return false;
}

int64_t ccs811_driver_conditioning_time_remaining(void)
{
	if (ccs811_conditioning_complete || ccs811_init_time == 0) {
		return 0;
	}

	int64_t elapsed = k_uptime_get() - ccs811_init_time;
	int64_t remaining = (int64_t)CCS811_CONDITIONING_TIME_MS - elapsed;

	return remaining > 0 ? remaining : 0;
}

int ccs811_driver_save_baseline(void)
{
	if (!ccs811_dev || !ccs811_conditioning_complete) {
		return -ENODEV;
	}

	int baseline = ccs811_baseline_fetch(ccs811_dev);
	if (baseline >= 0) {
		stored_baseline = (uint16_t)baseline;
		last_baseline_save = k_uptime_get();
		LOG_INF("CCS811 baseline saved: 0x%04x", stored_baseline);

		int save_ret = settings_save_one(CCS811_SETTINGS_KEY, &stored_baseline,
						 sizeof(stored_baseline));
		if (save_ret != 0) {
			LOG_WRN("CCS811 baseline flash save failed: %d", save_ret);
		} else {
			LOG_INF("CCS811 baseline persisted to flash: 0x%04x", stored_baseline);
		}

		return 0;
	} else {
		LOG_WRN("Failed to fetch CCS811 baseline: %d", baseline);
		return baseline;
	}
}

int ccs811_driver_restore_baseline(void)
{
	if (!ccs811_dev || stored_baseline == 0) {
		return -EINVAL;
	}

	int ret = ccs811_baseline_update(ccs811_dev, stored_baseline);
	if (ret == 0) {
		last_baseline_load = k_uptime_get();
		LOG_INF("CCS811 baseline restored: 0x%04x", stored_baseline);
		return 0;
	} else {
		LOG_WRN("Failed to restore CCS811 baseline: %d", ret);
		return ret;
	}
}

bool ccs811_driver_baseline_save_due(void)
{
	if (!ccs811_conditioning_complete) {
		return false;
	}

	int64_t uptime = k_uptime_get();
	return (uptime - last_baseline_save) >= (int64_t)BASELINE_SAVE_INTERVAL_MS;
}

int ccs811_driver_manual_reset(void)
{
	if (!ccs811_dev) {
		return -ENODEV;
	}

	const struct ccs811_config *config = ccs811_dev->config;

	/* Try a manual reset sequence following Nordic's approach */
	if (config->reset_gpio.port && config->wake_gpio.port) {
		LOG_INF("Performing manual CCS811 reset sequence");

		/* Reset sequence: reset low, wake low (awake), then reset high */
		gpio_pin_set_dt(&config->reset_gpio, 0); /* Assert reset (active low) */
		gpio_pin_set_dt(&config->wake_gpio, 0);  /* Keep awake (active low wake) */
		k_msleep(CCS811_RESET_DELAY_MS);

		gpio_pin_set_dt(&config->reset_gpio, 1); /* Release reset */
		k_msleep(CCS811_BOOT_DELAY_MS);          /* Wait for sensor to boot */

		LOG_INF("Manual reset sequence completed");

		/* Check status after reset */
		k_msleep(CCS811_STATUS_CHECK_DELAY_MS);
		const struct ccs811_result_type *post_reset_result = ccs811_result(ccs811_dev);
		if (post_reset_result) {
			LOG_INF("CCS811 after manual reset: status=0x%02x, FW_MODE=%d, "
				"APP_VALID=%d",
				post_reset_result->status, (post_reset_result->status >> 7) & 1,
				(post_reset_result->status >> 4) & 1);
		}

		/* Put device back to sleep to save power */
		gpio_pin_set_dt(&config->wake_gpio, 1); /* Set wake high (sleep mode) */
		k_busy_wait(CCS811_WAKE_DELAY_US);      /* t_DWAKE = 20 us */

		return 0;
	} else {
		LOG_WRN("Reset/Wake GPIOs not configured - cannot perform manual reset");
		return -ENOTSUP;
	}
}

bool ccs811_driver_is_enabled(void)
{
	return ccs811_enabled;
}

int ccs811_driver_read_air_quality(uint16_t *co2_ppm, uint16_t *tvoc_ppb, float temp_celsius,
				   float humidity_percent)
{
	struct sensor_value temp_val, hum_val, co2_val, tvoc_val;
	int ret = 0;
	bool mutex_locked = false;
	bool woke_sensor = false;

	/* Input validation */
	if (!ccs811_dev) {
		LOG_ERR("CCS811 driver not initialized");
		ret = -ENODEV;
		goto exit;
	}

	if (!co2_ppm && !tvoc_ppb) {
		LOG_ERR("Both output parameters are NULL");
		ret = -EINVAL;
		goto exit;
	}

	/* Thread-safe operation */
	ret = k_mutex_lock(&ccs811_mutex, K_MSEC(500));
	if (ret != 0) {
		LOG_ERR("Failed to acquire CCS811 mutex: %d", ret);
		goto exit;
	}
	mutex_locked = true;

	if (!device_is_ready(ccs811_dev)) {
		LOG_WRN("CCS811 device not ready");
		ret = -ENODEV;
		goto exit;
	}

	/* Check if conditioning period is complete */
	if (!ccs811_driver_is_ready()) {
		LOG_WRN("CCS811 still in conditioning period (%lld ms remaining)",
			ccs811_driver_conditioning_time_remaining());
		ret = -EAGAIN;
		goto exit;
	}

	/* Wake sensor from idle if in power-save mode */
	if (ccs811_in_idle_mode) {
		LOG_INF("CCS811 waking from IDLE for on-demand measurement");
		int wake_ret = ccs811_mode_update(ccs811_dev, CCS811_MEASUREMENT_1SEC);
		if (wake_ret == 0) {
			woke_sensor = true;
			/* Wait 1.5s for the first 1-second measurement to complete */
			k_mutex_unlock(&ccs811_mutex);
			k_msleep(1500);
			ret = k_mutex_lock(&ccs811_mutex, K_MSEC(500));
			if (ret != 0) {
				LOG_ERR("Failed to re-acquire CCS811 mutex after wake: %d", ret);
				woke_sensor = false;
				mutex_locked = false;
				goto exit;
			}
		} else {
			LOG_WRN("CCS811 wake from IDLE failed: %d", wake_ret);
		}
	}

	/* Update environmental data if provided and valid */
	if (!isnan(temp_celsius) && !isnan(humidity_percent) && temp_celsius >= -40.0f &&
	    temp_celsius <= 85.0f && humidity_percent >= 0.0f && humidity_percent <= 100.0f) {

		int32_t temp_whole = (int32_t)temp_celsius;
		int32_t hum_whole = (int32_t)humidity_percent;

		temp_val.val1 = temp_whole;
		temp_val.val2 = (int32_t)((temp_celsius - (float)temp_whole) * 1000000.0f);
		hum_val.val1 = hum_whole;
		hum_val.val2 = (int32_t)((humidity_percent - (float)hum_whole) * 1000000.0f);

		int env_ret = ccs811_envdata_update(ccs811_dev, &temp_val, &hum_val);
		if (env_ret != 0) {
			LOG_WRN("Failed to update CCS811 environmental data: %d", env_ret);
		} else {
			LOG_DBG("Updated CCS811 env data: %.1f°C, %.1f%%RH", (double)temp_celsius,
				(double)humidity_percent);
		}
	} else if (!isnan(temp_celsius) || !isnan(humidity_percent)) {
		LOG_WRN("Invalid environmental data: T=%.1f°C, H=%.1f%%RH (range check failed)",
			(double)temp_celsius, (double)humidity_percent);
	}

	/* Try direct sample fetch without waiting for interrupt */
	ret = sensor_sample_fetch(ccs811_dev);
	if (ret != 0) {
		if (ret == -EAGAIN) {
			LOG_DBG("CCS811 data not ready yet");
		} else {
			LOG_ERR("Failed to fetch CCS811 sample: %d", ret);
		}
		goto exit;
	}

	/* Check result status */
	const struct ccs811_result_type *result = ccs811_result(ccs811_dev);
	if (!result) {
		LOG_ERR("Failed to get CCS811 result");
		ret = -EIO;
		goto exit;
	}

	if (!(result->status & CCS811_STATUS_DATA_READY)) {
		LOG_WRN("CCS811 data not ready, status=0x%02x", result->status);
		ret = -EAGAIN;
		goto exit;
	}

	if (result->status & CCS811_STATUS_ERROR) {
		LOG_WRN("CCS811 error bit set, status=0x%02x", result->status);
		ret = -EIO;
		goto exit;
	}

	/* Read CO2 if requested */
	if (co2_ppm) {
		ret = sensor_channel_get(ccs811_dev, SENSOR_CHAN_CO2, &co2_val);
		if (ret == 0) {
			/* Validate CO2 reading is within reasonable bounds */
			if (co2_val.val1 >= 400 && co2_val.val1 <= 8192) {
				*co2_ppm = (uint16_t)co2_val.val1;
				cached_co2_ppm = *co2_ppm; /* Cache the reading */
				LOG_DBG("CO2: %d ppm", *co2_ppm);
			} else {
				LOG_WRN("CO2 reading out of bounds: %d ppm", co2_val.val1);
				ret = -ERANGE;
				goto exit;
			}
		} else {
			LOG_WRN("Failed to read CO2: %d", ret);
			goto exit;
		}
	}

	/* Read TVOC if requested */
	if (tvoc_ppb) {
		ret = sensor_channel_get(ccs811_dev, SENSOR_CHAN_VOC, &tvoc_val);
		if (ret == 0) {
			/* Validate TVOC reading is within reasonable bounds */
			if (tvoc_val.val1 >= 0 && tvoc_val.val1 <= 1187) {
				*tvoc_ppb = (uint16_t)tvoc_val.val1;
				cached_tvoc_ppb = *tvoc_ppb; /* Cache the reading */
				LOG_DBG("TVOC: %d ppb", *tvoc_ppb);
			} else {
				LOG_WRN("TVOC reading out of bounds: %d ppb", tvoc_val.val1);
				ret = -ERANGE;
				goto exit;
			}
		} else {
			LOG_WRN("Failed to read TVOC: %d", ret);
			goto exit;
		}
	}

	/* Update timing and adaptive sampling logic */
	last_sample_time = k_uptime_get();

	/* After first conditioning read: enter IDLE power-save mode between reads */
	if (!first_sample_obtained) {
		first_sample_obtained = true;
		int idle_ret = ccs811_mode_update(ccs811_dev, CCS811_MEASUREMENT_IDLE);
		if (idle_ret == 0) {
			ccs811_in_idle_mode = true;
			woke_sensor = false; /* Already returning to idle below */
			LOG_INF("CCS811 entered IDLE mode after first read (power save)");
		} else {
			LOG_WRN("CCS811 failed to enter IDLE mode: %d", idle_ret);
		}
	}

	/* Check if baseline save is due */
	if (ccs811_driver_baseline_save_due()) {
		LOG_INF("CCS811 baseline save is due");
	}

	/* Success path */
	ret = 0;

exit:
	/* Return sensor to IDLE if we woke it for this read */
	if (mutex_locked && woke_sensor) {
		ccs811_mode_update(ccs811_dev, CCS811_MEASUREMENT_IDLE);
		ccs811_in_idle_mode = true;
		LOG_INF("CCS811 returned to IDLE mode");
	}

	/* Single exit point - release mutex if we acquired it */
	if (mutex_locked) {
		k_mutex_unlock(&ccs811_mutex);
	}
	return ret;
}

int ccs811_driver_read_for_ble(uint16_t *co2_ppm, uint16_t *tvoc_ppb, float temp_celsius,
			       float humidity_percent)
{
	/* For BLE requests, try to get fresh data but fallback to cached if not ready */
	int ret = ccs811_driver_read_air_quality(co2_ppm, tvoc_ppb, temp_celsius, humidity_percent);

	if (ret == 0) {
		/* Fresh data obtained successfully */
		return 0;
	}

	/* If fresh data not available, use cached data if available and not too old */
	if ((ret == -EAGAIN || ret == -ETIME) && first_sample_obtained) {
		int64_t age_ms = k_uptime_get() - last_sample_time;
		const int64_t MAX_CACHE_AGE_MS = 5LL * 60LL * 1000LL; /* 5 minutes max age */

		if (age_ms <= MAX_CACHE_AGE_MS) {
			LOG_DBG("Using cached CCS811 data for BLE (fresh data not ready)");

			bool has_cached_data = false;
			if (co2_ppm && cached_co2_ppm > 0) {
				*co2_ppm = cached_co2_ppm;
				has_cached_data = true;
			}

			if (tvoc_ppb && cached_tvoc_ppb > 0) {
				*tvoc_ppb = cached_tvoc_ppb;
				has_cached_data = true;
			}

			/* Return success if we have at least one cached value */
			if (has_cached_data) {
				LOG_INF("Using cached CCS811 data (age: %lld ms): CO2=%d ppm, "
					"TVOC=%d ppb",
					age_ms, cached_co2_ppm, cached_tvoc_ppb);
				return 0;
			}
		} else {
			LOG_WRN("Cached CCS811 data too old (%lld ms), max age %lld ms", age_ms,
				MAX_CACHE_AGE_MS);
		}
	}

	/* No fresh or valid cached data available */
	LOG_WRN("No fresh or cached CCS811 data available (ret=%d)", ret);
	return ret;
}

int ccs811_driver_get_mode(void)
{
	if (!ccs811_dev) {
		LOG_ERR("CCS811 driver not initialized");
		return -ENODEV;
	}

	if (!device_is_ready(ccs811_dev)) {
		LOG_ERR("CCS811 device not ready");
		return -ENODEV;
	}

	/* Get the current mode from the driver data */
	struct ccs811_data *drv_data = (struct ccs811_data *)ccs811_dev->data;
	if (!drv_data) {
		LOG_ERR("CCS811 driver data is NULL");
		return -EFAULT;
	}

	uint8_t current_mode = drv_data->mode & CCS811_MODE_MSK;

	LOG_DBG("CCS811 current mode: 0x%02x", current_mode);
	return current_mode;
}
