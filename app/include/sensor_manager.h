/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <zephyr/kernel.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sensor selection flags for selective updates
 */
enum sensor_select {
	SENSOR_NONE        = 0x00,
	SENSOR_TEMPERATURE = 0x01,
	SENSOR_HUMIDITY    = 0x02,
	SENSOR_PRESSURE    = 0x04,
	SENSOR_AIR_QUALITY = 0x08,  /* CO2 + TVOC */
	SENSOR_BATTERY     = 0x10,
	SENSOR_ALL         = 0xFF
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

	/* Data validity flags */
	bool temperature_valid;
	bool humidity_valid;
	bool pressure_valid;
	bool eco2_valid;
	bool tvoc_valid;
	bool battery_valid;
};

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
 * @brief Register callback for sensor updates
 *
 * @param callback Function to call when sensors are updated
 * @return 0 on success, negative error code on failure
 */
int sensor_manager_register_callback(sensor_update_callback_t callback);

/**
 * @brief Start periodic sensor readings
 *
 * @param interval_ms Interval between readings in milliseconds
 * @return 0 on success, negative error code on failure
 */
int sensor_manager_start_periodic(uint32_t interval_ms);

/**
 * @brief Stop periodic sensor readings
 */
void sensor_manager_stop_periodic(void);

/**
 * @brief Check if CCS811 conditioning period is complete
 * @return true if conditioning is complete, false otherwise
 */
bool sensor_manager_is_ccs811_ready(void);

/**
 * @brief Get individual sensor readings (for BLE Mesh compatibility)
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

#endif /* SENSOR_MANAGER_H */
