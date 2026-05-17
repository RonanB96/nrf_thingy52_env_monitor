/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>

#include "sensor_manager.h"
#include "battery_service.h"
#include "sensor_hts221_driver.h"
#include "sensor_lps22hb_driver.h"
#include "sensor_ccs811_driver.h"

LOG_MODULE_REGISTER(sensor_manager, CONFIG_LOG_DEFAULT_LEVEL);

/* Sensor device nodes from device tree */
#define HTS221_NODE  DT_NODELABEL(hts221)
#define LPS22HB_NODE DT_NODELABEL(lps22hb_press)
#define CCS811_NODE  DT_NODELABEL(ccs811)

/*
 * Power model:
 *   - env_work (HTS221 + LPS22HB + battery ADC): only runs while at least one
 *     GATT client is connected. No client means no consumer for the data, so
 *     we save energy by not sampling.
 *   - aq_work (CCS811 air quality): runs continuously regardless of connection
 *     state. The CCS811 must keep operating to preserve its conditioning state
 *     and 24h baseline persistence; powering it down would invalidate readings
 *     for hours after the next connect.
 *
 * Compensation freshness: while disconnected, env_work is not sampling so the
 * temperature/humidity values used by the CCS811 driver for environmental
 * compensation drift with the cached values. sensor_manager_on_connected()
 * runs an env read immediately before the on-connect AQ read, so every value
 * reported to a client is computed with compensation no older than one I2C
 * round-trip. Between samples while connected, env_work refreshes the
 * compensation inputs every CONFIG_SENSOR_ENV_INTERVAL_SEC.
 *
 * See docs/low_power.md for the full configured operating points and current
 * budget.
 */

/* Static sensor data */
static struct sensor_data current_data = {0};
static sensor_update_callback_t update_callback = NULL;
static bool initialized = false;
static bool armed = false;
static uint32_t connected_count;

/* Periodic work items */
static struct k_work_delayable env_work;
static struct k_work_delayable aq_work;

/* I2C device for direct register access - used by driver modules */
static const struct device *i2c_dev;

/* Thread safety mutex for sensor operations */
static K_MUTEX_DEFINE(sensor_manager_mutex);

static void env_work_handler(struct k_work *work)
{
	(void)work;

	sensor_manager_update_selective(SENSOR_ENV_BASIC);

	if (armed && connected_count > 0U) {
		k_work_reschedule(&env_work, K_SECONDS(CONFIG_SENSOR_ENV_INTERVAL_SEC));
	}
}

static void aq_work_handler(struct k_work *work)
{
	(void)work;

	sensor_manager_update_selective(SENSOR_AIR_QUALITY);

	if (ccs811_driver_baseline_save_due()) {
		int ret = ccs811_driver_save_baseline();
		if (ret != 0) {
			LOG_WRN("CCS811 baseline save failed: %d", ret);
		}
	}

	if (armed) {
		k_work_reschedule(&aq_work, K_SECONDS(CONFIG_SENSOR_ENV_INTERVAL_SEC *
						      CONFIG_SENSOR_AIR_QUALITY_DIVISOR));
	}
}

static int read_temperature_humidity(void)
{
	float temperature;
	float humidity;
	int ret;

	LOG_INF("Reading HTS221 temperature/humidity...");

	/* Use the dedicated HTS221 driver to read both values efficiently */
	ret = hts221_driver_read_both(&temperature, &humidity);
	if (ret == 0) {
		LOG_INF("HTS221 returned: T=%.2f°C H=%.2f%% (ret=%d)", (double)temperature,
			(double)humidity, ret);
		current_data.temperature = temperature;
		current_data.humidity = humidity;
		current_data.valid_mask |= SENSOR_TEMPERATURE;
		current_data.valid_mask |= SENSOR_HUMIDITY;
		LOG_DBG("Temperature: %.2f °C, Humidity: %.2f %%", (double)temperature,
			(double)humidity);
	} else {
		LOG_ERR("HTS221 read failed with ret=%d", ret);
		current_data.valid_mask &= ~SENSOR_TEMPERATURE;
		current_data.valid_mask &= ~SENSOR_HUMIDITY;
		LOG_WRN("Failed to read HTS221: %d", ret);
	}

	return ret;
}

static int read_pressure(void)
{
	float pressure;
	int ret;

	/* Use the dedicated LPS22HB driver to read pressure */
	ret = lps22hb_driver_read_pressure(&pressure);
	if (ret == 0) {
		current_data.pressure = pressure;
		current_data.valid_mask |= SENSOR_PRESSURE;
		LOG_DBG("Pressure: %.3f kPa", (double)pressure);
	} else {
		current_data.valid_mask &= ~SENSOR_PRESSURE;
		LOG_WRN("Failed to read LPS22HB: %d", ret);
	}

	return ret;
}

static int read_air_quality(void)
{
	uint16_t co2_ppm;
	uint16_t tvoc_ppb;
	int ret;

	/* Use temperature and humidity for environmental compensation if available */
	float temp = (current_data.valid_mask & SENSOR_TEMPERATURE) != 0 ? current_data.temperature
									 : NAN;
	float humidity =
		(current_data.valid_mask & SENSOR_HUMIDITY) != 0 ? current_data.humidity : NAN;

	/* Use the dedicated CCS811 driver to read air quality data */
	ret = ccs811_driver_read_air_quality(&co2_ppm, &tvoc_ppb, temp, humidity);

	if (ret == 0) {
		/* Validate each reading independently, then set the flag only when both are valid
		 */
		bool eco2_ok = (co2_ppm != CCS811_INVALID_READING && co2_ppm != 0 &&
				co2_ppm >= CCS811_ECO2_MIN_PPM && co2_ppm <= CCS811_ECO2_MAX_PPM);
		bool tvoc_ok = (tvoc_ppb != CCS811_INVALID_READING &&
				tvoc_ppb >= CCS811_TVOC_MIN_PPB && tvoc_ppb <= CCS811_TVOC_MAX_PPB);

		if (eco2_ok) {
			current_data.eco2 = co2_ppm;
		} else {
			LOG_INF("Invalid eCO2 reading: %d (0x%04x)", co2_ppm, co2_ppm);
		}

		if (tvoc_ok) {
			current_data.tvoc = tvoc_ppb;
		} else {
			LOG_INF("Invalid TVOC reading: %d (0x%04x)", tvoc_ppb, tvoc_ppb);
		}

		if (eco2_ok && tvoc_ok) {
			current_data.valid_mask |= SENSOR_AIR_QUALITY;
			LOG_INF("CCS811 readings: eCO2=%d ppm, TVOC=%d ppb", current_data.eco2,
				current_data.tvoc);
		} else {
			current_data.valid_mask &= ~SENSOR_AIR_QUALITY;
		}
	} else if (ret == -EAGAIN) {
		/* Sensor still in conditioning or data not ready */
		current_data.valid_mask &= ~SENSOR_AIR_QUALITY;
		LOG_DBG("CCS811 data not ready yet");
	} else {
		/* Other error */
		current_data.valid_mask &= ~SENSOR_AIR_QUALITY;
		LOG_WRN("Failed to read CCS811: %d", ret);
	}

	return ret;
}

static int read_battery(void)
{
	int bat_level = battery_service_get_level();
	if (bat_level >= 0) {
		current_data.battery_level = (uint8_t)bat_level;
		current_data.valid_mask |= SENSOR_BATTERY;
	} else {
		current_data.valid_mask &= ~SENSOR_BATTERY;
		LOG_WRN("Battery read failed: %d", bat_level);
	}
	current_data.battery_charging = battery_service_is_charging();
	LOG_DBG("Battery: %d%% %s", current_data.battery_level,
		current_data.battery_charging ? "(Charging)" : "");
	return 0;
}

int sensor_manager_init(void)
{
	int ret;

	if (initialized) {
		return 0;
	}

	LOG_INF("Initializing sensor manager");

	/* Get I2C device for direct sensor power management */
	i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device not ready - power management disabled");
		i2c_dev = NULL;
		return -ENODEV;
	} else {
		LOG_INF("I2C device ready - initializing sensor drivers");
	}

	/* Initialize battery service first */
	ret = battery_service_init();
	if (ret != 0) {
		LOG_WRN("Battery service init failed: %d", ret);
		/* Continue without battery monitoring */
	}

	/* Get sensor devices for driver initialization */
	const struct device *hts221_dev = DEVICE_DT_GET(HTS221_NODE);
	const struct device *lps22hb_dev = DEVICE_DT_GET(LPS22HB_NODE);
	const struct device *ccs811_dev = DEVICE_DT_GET(CCS811_NODE);

	/* Initialize HTS221 driver */
	if (device_is_ready(hts221_dev)) {
		ret = hts221_driver_init(i2c_dev);
		if (ret != 0) {
			LOG_ERR("Failed to initialize HTS221 driver: %d", ret);
		} else {
			LOG_INF("HTS221 driver initialized successfully");
		}
	} else {
		LOG_WRN("HTS221 device not ready");
	}

	/* Initialize LPS22HB driver */
	if (device_is_ready(lps22hb_dev)) {
		const struct gpio_dt_spec lps22hb_int_pin =
			GPIO_DT_SPEC_GET(DT_NODELABEL(lps22hb_int), gpios);
		ret = lps22hb_driver_init(i2c_dev, &lps22hb_int_pin);
		if (ret != 0) {
			LOG_ERR("Failed to initialize LPS22HB driver: %d", ret);
		} else {
			LOG_INF("LPS22HB driver initialized successfully");
		}
	} else {
		LOG_WRN("LPS22HB device not ready");
	}

	/* Initialize CCS811 driver */
	if (device_is_ready(ccs811_dev)) {
		ret = ccs811_driver_init(ccs811_dev);
		if (ret != 0) {
			LOG_ERR("Failed to initialize CCS811 driver: %d", ret);
		} else {
			LOG_INF("CCS811 driver initialized successfully");
		}
	} else {
		LOG_WRN("CCS811 device not ready");
	}

	/* LPS22HB interrupt and semaphore are now handled within the driver */

	/* Initialize work items - neither is scheduled yet; sensor_manager_arm()
	 * starts aq_work and sensor_manager_on_connected() starts env_work.
	 */
	k_work_init_delayable(&env_work, env_work_handler);
	k_work_init_delayable(&aq_work, aq_work_handler);

	initialized = true;
	LOG_INF("Sensor manager initialized");

	sensor_manager_update();

	return 0;
}

int sensor_manager_get_data(struct sensor_data *data)
{
	if (!initialized || !data) {
		return -EINVAL;
	}

	/* Thread-safe data copy */
	int ret = k_mutex_lock(&sensor_manager_mutex, K_MSEC(100));
	if (ret != 0) {
		LOG_ERR("Failed to acquire sensor manager mutex: %d", ret);
		return ret;
	}

	/* Create local copy to avoid volatile qualifier issues */
	struct sensor_data local_data = *(struct sensor_data *)&current_data;
	memcpy(data, &local_data, sizeof(struct sensor_data));

	k_mutex_unlock(&sensor_manager_mutex);
	return 0;
}

int sensor_manager_update(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	/* Thread-safe update */
	int ret = k_mutex_lock(&sensor_manager_mutex, K_MSEC(500));
	if (ret != 0) {
		LOG_ERR("Failed to acquire sensor manager mutex for update: %d", ret);
		return ret;
	}

	/* Update timestamp */
	current_data.timestamp = k_uptime_get();

	/* Read all sensors */
	read_temperature_humidity();
	read_pressure();
	read_air_quality();
	read_battery();

	/* Copy data and callback pointer under lock, call after release to avoid deadlock */
	struct sensor_data local_data = current_data;
	sensor_update_callback_t local_cb = update_callback;
	k_mutex_unlock(&sensor_manager_mutex);

	LOG_INF("Sensor update: T=%.1f°C H=%.1f%% P=%.3fkPa CO2=%dppm TVOC=%dppb Bat=%d%%",
		(double)((local_data.valid_mask & SENSOR_TEMPERATURE) != 0 ? local_data.temperature
									   : 0.0f),
		(double)((local_data.valid_mask & SENSOR_HUMIDITY) != 0 ? local_data.humidity
									: 0.0f),
		(double)((local_data.valid_mask & SENSOR_PRESSURE) != 0 ? local_data.pressure
									: 0.0f),
		(local_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? local_data.eco2 : 0,
		(local_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? local_data.tvoc : 0,
		(local_data.valid_mask & SENSOR_BATTERY) != 0 ? local_data.battery_level : 0);

	if (local_cb) {
		local_cb(&local_data);
	}

	return 0;
}

int sensor_manager_update_selective(enum sensor_select sensors)
{
	if (!initialized) {
		return -EINVAL;
	}

	/* Thread-safe update */
	int ret = k_mutex_lock(&sensor_manager_mutex, K_MSEC(500));
	if (ret != 0) {
		LOG_ERR("Failed to acquire sensor manager mutex for selective update: %d", ret);
		return ret;
	}

	/* Update timestamp */
	current_data.timestamp = k_uptime_get();

	/* Read selected sensors */
	if (sensors & (SENSOR_TEMPERATURE | SENSOR_HUMIDITY)) {
		read_temperature_humidity();
	}

	if (sensors & SENSOR_PRESSURE) {
		read_pressure();
	}

	if (sensors & SENSOR_AIR_QUALITY) {
		read_air_quality();
	}

	if (sensors & SENSOR_BATTERY) {
		read_battery();
	}

	/* Copy data and callback pointer under lock, call after release to avoid deadlock */
	struct sensor_data local_data = current_data;
	sensor_update_callback_t local_cb = update_callback;
	k_mutex_unlock(&sensor_manager_mutex);

	LOG_DBG("Selective sensor update (0x%02x): T=%.1f°C H=%.1f%% P=%.3fkPa CO2=%dppm "
		"TVOC=%dppb Bat=%d%%",
		sensors,
		(double)((local_data.valid_mask & SENSOR_TEMPERATURE) != 0 ? local_data.temperature
									   : 0.0f),
		(double)((local_data.valid_mask & SENSOR_HUMIDITY) != 0 ? local_data.humidity
									: 0.0f),
		(double)((local_data.valid_mask & SENSOR_PRESSURE) != 0 ? local_data.pressure
									: 0.0f),
		(local_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? local_data.eco2 : 0,
		(local_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? local_data.tvoc : 0,
		(local_data.valid_mask & SENSOR_BATTERY) != 0 ? local_data.battery_level : 0);

	if (local_cb) {
		local_cb(&local_data);
	}

	return 0;
}

int sensor_manager_register_callback(sensor_update_callback_t callback)
{
	int ret = k_mutex_lock(&sensor_manager_mutex, K_MSEC(100));
	if (ret != 0) {
		LOG_ERR("Failed to lock for callback registration: %d", ret);
		return ret;
	}

	update_callback = callback;
	k_mutex_unlock(&sensor_manager_mutex);
	return 0;
}

int sensor_manager_arm(void)
{
	if (!initialized) {
		LOG_ERR("sensor_manager_arm: not initialized");
		return -EINVAL;
	}

	if (update_callback == NULL) {
		LOG_ERR("sensor_manager_arm: no callback registered");
		return -EINVAL;
	}

	armed = true;

	/* Start the always-on air-quality work loop. */
	k_work_reschedule(&aq_work, K_SECONDS(CONFIG_SENSOR_ENV_INTERVAL_SEC *
					      CONFIG_SENSOR_AIR_QUALITY_DIVISOR));

	LOG_INF("Sensor manager armed (env on-connect, AQ continuous every %u s)",
		(unsigned int)(CONFIG_SENSOR_ENV_INTERVAL_SEC * CONFIG_SENSOR_AIR_QUALITY_DIVISOR));
	return 0;
}

int sensor_manager_on_connected(void)
{
	if (!armed) {
		LOG_WRN("on_connected before arm() - ignored");
		return -EINVAL;
	}

	connected_count++;
	LOG_INF("GATT client connected (count=%u): starting env sampling",
		(unsigned int)connected_count);

	/* Take a fresh env sample first so current_data.temperature and
	 * .humidity are up to date. read_air_quality() pulls those values for
	 * CCS811 environmental compensation, so doing env before AQ refreshes
	 * the compensation inputs the on-connect AQ read uses.
	 *
	 * Both reads are also what the first ESS notify after subscribe will
	 * carry, so the client sees data no older than the I2C round-trip time.
	 */
	sensor_manager_update_selective(SENSOR_ENV_BASIC);
	sensor_manager_update_selective(SENSOR_AIR_QUALITY);

	/* Reschedule env work; reschedule is idempotent if it was already running
	 * for an earlier connection.
	 */
	k_work_reschedule(&env_work, K_SECONDS(CONFIG_SENSOR_ENV_INTERVAL_SEC));
	return 0;
}

void sensor_manager_on_disconnected(void)
{
	if (connected_count == 0U) {
		LOG_WRN("on_disconnected with no tracked connections");
		return;
	}

	connected_count--;
	LOG_INF("GATT client disconnected (count=%u)", (unsigned int)connected_count);

	if (connected_count == 0U) {
		k_work_cancel_delayable(&env_work);
		LOG_INF("No clients - env sampling stopped (AQ continues)");
	}
}

bool sensor_manager_is_ccs811_ready(void)
{
	return ccs811_driver_is_ready();
}

/* Individual sensor getters for BLE Mesh compatibility */
float sensor_manager_get_temperature(void)
{
	return (current_data.valid_mask & SENSOR_TEMPERATURE) != 0 ? current_data.temperature
								   : 0.0f;
}

float sensor_manager_get_humidity(void)
{
	return (current_data.valid_mask & SENSOR_HUMIDITY) != 0 ? current_data.humidity : 0.0f;
}

float sensor_manager_get_pressure(void)
{
	return (current_data.valid_mask & SENSOR_PRESSURE) != 0 ? current_data.pressure : 0.0f;
}

uint16_t sensor_manager_get_eco2(void)
{
	return (current_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? current_data.eco2 : 0;
}

uint16_t sensor_manager_get_tvoc(void)
{
	return (current_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? current_data.tvoc : 0;
}

void sensor_manager_update_air_quality_for_ble(void)
{
	if (!initialized) {
		return;
	}

	uint16_t co2_ppm;
	uint16_t tvoc_ppb;
	int ret;

	/* Use temperature and humidity for environmental compensation if available */
	float temp = (current_data.valid_mask & SENSOR_TEMPERATURE) != 0 ? current_data.temperature
									 : NAN;
	float humidity =
		(current_data.valid_mask & SENSOR_HUMIDITY) != 0 ? current_data.humidity : NAN;

	/* Use the BLE-optimized read function with cached fallback */
	ret = ccs811_driver_read_for_ble(&co2_ppm, &tvoc_ppb, temp, humidity);

	if (ret == 0) {
		/* Validate both readings; set the flag only when both are valid (keep cache on
		 * failure so BLE advertiser continues using last known good values) */
		bool eco2_ok = (co2_ppm != CCS811_INVALID_READING && co2_ppm != 0 &&
				co2_ppm >= CCS811_ECO2_MIN_PPM && co2_ppm <= CCS811_ECO2_MAX_PPM);
		bool tvoc_ok = (tvoc_ppb != CCS811_INVALID_READING &&
				tvoc_ppb >= CCS811_TVOC_MIN_PPB && tvoc_ppb <= CCS811_TVOC_MAX_PPB);

		if (eco2_ok && tvoc_ok) {
			current_data.eco2 = co2_ppm;
			current_data.tvoc = tvoc_ppb;
			current_data.valid_mask |= SENSOR_AIR_QUALITY;
		}

		LOG_DBG("BLE air quality update: eCO2=%d ppm, TVOC=%d ppb",
			(current_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? current_data.eco2 : 0,
			(current_data.valid_mask & SENSOR_AIR_QUALITY) != 0 ? current_data.tvoc
									    : 0);
	} else {
		LOG_DBG("BLE air quality update failed: %d (using existing cache)", ret);
	}
}

uint8_t sensor_manager_get_battery_level(void)
{
	return (current_data.valid_mask & SENSOR_BATTERY) != 0 ? current_data.battery_level : 0;
}
