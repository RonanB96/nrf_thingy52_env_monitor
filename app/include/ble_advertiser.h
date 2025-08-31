/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_ADVERTISER_H
#define BLE_ADVERTISER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Environmental sensor advertising data
 */
struct ble_sensor_data {
	uint16_t temperature;    /* Temperature in 0.01°C */
	uint16_t humidity;       /* Humidity in 0.01% */
	uint16_t pressure;       /* Pressure in 0.1 hPa */
	uint16_t eco2;          /* CO2 in ppm */
	uint16_t tvoc;          /* TVOC in ppb */
	uint8_t battery_level;   /* Battery in % */
	bool battery_charging;   /* Battery charging status */
} __packed;

/**
 * @brief Initialize BLE advertiser
 *
 * @return 0 on success, negative error code on failure
 */
int ble_advertiser_init(void);

/**
 * @brief Start advertising with sensor data
 *
 * @param data Sensor data to advertise
 * @return 0 on success, negative error code on failure
 */
int ble_advertiser_start(const struct ble_sensor_data *data);

/**
 * @brief Stop advertising
 *
 * @return 0 on success, negative error code on failure
 */
int ble_advertiser_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_ADVERTISER_H */
