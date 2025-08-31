/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ESS_SERVICE_H_
#define ESS_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct sensor_data;

/**
 * @brief Initialize the Environmental Sensing Service
 *
 * This function initializes the Bluetooth Environmental Sensing Service (ESS)
 * and sets up all supported characteristics: Temperature, Humidity, Pressure,
 * CO2 Concentration, and TVOC Concentration.
 *
 * @return 0 on success, negative error code on failure
 */
int ess_service_init(void);

/**
 * @brief Update ESS characteristics with sensor data
 *
 * Updates all Environmental Sensing Service characteristics with the provided
 * sensor data. Only sends notifications if values have changed and notifications
 * are enabled by the client.
 *
 * Data formats follow Bluetooth SIG specifications:
 * - Temperature: 0.01°C units (e.g., 2000 = 20.00°C)
 * - Humidity: 0.01% units (e.g., 5000 = 50.00%)
 * - Pressure: 0.1 hPa units (e.g., 10130 = 1013.0 hPa)
 * - CO2: ppm (direct value, custom characteristic)
 * - TVOC: ppb (direct value, custom characteristic)
 *
 * @param data Pointer to sensor data structure
 *
 * @return 0 on success, negative error code on failure
 */
int ess_service_update(const struct sensor_data *data);

/**
 * @brief Get current temperature value
 *
 * Returns the current temperature value in 0.01°C units.
 *
 * @return Temperature in 0.01°C (e.g., 2000 = 20.00°C)
 */
int ess_service_get_temperature(void);

/**
 * @brief Get current humidity value
 *
 * Returns the current humidity value in 0.01% units.
 *
 * @return Humidity in 0.01% (e.g., 5000 = 50.00%)
 */
int ess_service_get_humidity(void);

/**
 * @brief Get current pressure value
 *
 * Returns the current pressure value in Pa units.
 *
 * @return Pressure in Pa (e.g., 101325 = 1013.25 hPa)
 */
int ess_service_get_pressure(void);

/**
 * @brief Get current CO2 concentration
 *
 * Returns the current CO2 concentration in ppm.
 *
 * @return CO2 concentration in ppm
 */
int ess_service_get_co2(void);

/**
 * @brief Get current TVOC concentration
 *
 * Returns the current TVOC concentration in ppb.
 *
 * @return TVOC concentration in ppb
 */
int ess_service_get_tvoc(void);

#ifdef __cplusplus
}
#endif

#endif /* ESS_SERVICE_H_ */
