/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_MANAGER_H_
#define SENSOR_MANAGER_H_

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sensor selection flags for selective updates
 */
enum sensor_select {
	SENSOR_NONE = 0x00,
	SENSOR_TEMPERATURE = 0x01,
	SENSOR_HUMIDITY = 0x02,
	SENSOR_PRESSURE = 0x04,
	SENSOR_AIR_QUALITY = 0x08, /* CO2 + TVOC */
	SENSOR_BATTERY = 0x10,
	/* Named combinations — use these instead of inline OR expressions */
	SENSOR_TEMP_HUMIDITY = SENSOR_TEMPERATURE | SENSOR_HUMIDITY,
	SENSOR_ENV_BASIC = SENSOR_TEMPERATURE | SENSOR_HUMIDITY | SENSOR_PRESSURE | SENSOR_BATTERY,
	SENSOR_ENV_FULL = SENSOR_TEMPERATURE | SENSOR_HUMIDITY | SENSOR_PRESSURE | SENSOR_BATTERY |
			  SENSOR_AIR_QUALITY,
	SENSOR_ALL = 0xFF
};

/**
 * @brief Environmental sensor data structure
 */
struct sensor_data {
	/* Temperature in degrees Celsius */
	float temperature;

	/* Humidity in percentage */
	float humidity;

	/* Pressure in hPa */
	float pressure;

	/* CO2 equivalent in ppm */
	uint16_t eco2;

	/* Total Volatile Organic Compounds in ppb */
	uint16_t tvoc;

	/* Battery level in percentage */
	uint8_t battery_level;

	/* Battery charging status */
	bool battery_charging;

	/* Timestamp of last update */
	int64_t timestamp;

	/* Bitmask of valid sensor readings */
	enum sensor_select valid_mask;
};

#define SENSOR_DATA_IS_VALID(data, flag) (((data)->valid_mask & (flag)) != 0U)

/**
 * @brief Sensor update callback function type
 *
 * @param data Pointer to updated sensor data
 */
typedef void (*sensor_update_callback_t)(const struct sensor_data *data);

/**
 * @brief Initialize the sensor manager
 *
 * @return 0 on success, negative error code on failure
 */
int sensor_manager_init(void);

/**
 * @brief Get current sensor data
 *
 * @param data Pointer to store sensor data
 * @return 0 on success, negative error code on failure
 */
int sensor_manager_get_data(struct sensor_data *data);

/**
 * @brief Force a sensor reading update
 *
 * @return 0 on success, negative error code on failure
 */
int sensor_manager_update(void);

/**
 * @brief Force selective sensor reading update
 *
 * @param sensors Bitmask of sensors to update (enum sensor_select)
 * @return 0 on success, negative error code on failure
 */
int sensor_manager_update_selective(enum sensor_select sensors);

/**
 * @brief Register callback for sensor updates.
 *
 * Must be called after sensor_manager_init() and before sensor_manager_arm().
 * Currently a single global callback is supported (overwrites previous).
 *
 * @param callback Function to call when sensors are updated.
 * @return 0 on success, negative error code on failure.
 */
int sensor_manager_register_callback(sensor_update_callback_t callback);

/**
 * @brief Arm the periodic sampling work items.
 *
 * Pre-conditions: sensor_manager_init() succeeded AND a callback has been
 * registered. Returns -EINVAL if either is not true. Once armed:
 *   - The air-quality (CCS811) work item runs continuously to preserve the
 *     sensor's conditioning state and 24h baseline persistence.
 *   - The environmental work item (HTS221, LPS22HB, battery ADC) is dormant
 *     until sensor_manager_on_connected() is called.
 *
 * @return 0 on success, negative errno on failure.
 */
int sensor_manager_arm(void);

/**
 * @brief Notify the sensor manager that a GATT client has connected.
 *
 * Triggers an immediate environmental sample so the first notify after
 * subscribe carries fresh data, then schedules the env work item to run at
 * CONFIG_SENSOR_ENV_INTERVAL_SEC for the duration of the connection.
 *
 * Tracks an internal connection count: env sampling continues until the last
 * client disconnects.
 *
 * @return 0 on success, negative errno on failure.
 */
int sensor_manager_on_connected(void);

/**
 * @brief Notify the sensor manager that a GATT client has disconnected.
 *
 * Decrements the connection count; cancels the env work item when the last
 * client is gone. The CCS811 (air quality) work item is unaffected.
 */
void sensor_manager_on_disconnected(void);

/**
 * @brief Check if CCS811 conditioning period is complete.
 * @return true if conditioning is complete, false otherwise.
 */
bool sensor_manager_is_ccs811_ready(void);

/**
 * @brief Individual sensor accessors (return last cached value or 0 if invalid).
 */
float sensor_manager_get_temperature(void);
float sensor_manager_get_humidity(void);
float sensor_manager_get_pressure(void);
uint16_t sensor_manager_get_eco2(void);
uint16_t sensor_manager_get_tvoc(void);
uint8_t sensor_manager_get_battery_level(void);

/**
 * @brief Update air quality readings optimized for BLE requests
 *
 * This function attempts to get fresh air quality data but falls back
 * to cached readings if the sensor is not ready. It's designed for
 * BLE service calls to ensure responsive data availability.
 */
void sensor_manager_update_air_quality_for_ble(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H_ */
