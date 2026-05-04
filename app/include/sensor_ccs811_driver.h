/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_CCS811_DRIVER_H_
#define SENSOR_CCS811_DRIVER_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <stdint.h>
#include <stdbool.h>

/* CCS811 measurement range and sentinel constants */
#define CCS811_INVALID_READING 65021U
#define CCS811_ECO2_MIN_PPM    400U
#define CCS811_ECO2_MAX_PPM    8192U
#define CCS811_TVOC_MIN_PPB    0U
#define CCS811_TVOC_MAX_PPB    1187U

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize CCS811 driver and start conditioning period
 *
 * @param ccs811_dev CCS811 device from device tree
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_init(const struct device *ccs811_dev);

/**
 * @brief Check if CCS811 conditioning period is complete
 *
 * The CCS811 requires a conditioning period after power-on before
 * reliable readings can be obtained.
 *
 * @return true if conditioning is complete, false otherwise
 */
bool ccs811_driver_is_ready(void);

/**
 * @brief Get time remaining in conditioning period
 *
 * @return milliseconds remaining in conditioning, 0 if complete
 */
int64_t ccs811_driver_conditioning_time_remaining(void);

/**
 * @brief Save CCS811 baseline for long-term accuracy
 *
 * Should be called periodically (every 24 hours) to maintain
 * long-term accuracy across power cycles.
 *
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_save_baseline(void);

/**
 * @brief Restore previously saved CCS811 baseline
 *
 * Should be called after sensor initialization to restore
 * long-term accuracy from previous session.
 *
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_restore_baseline(void);

/**
 * @brief Check if baseline save is due
 *
 * Checks if 24 hours have passed since last baseline save.
 *
 * @return true if baseline should be saved, false otherwise
 */
bool ccs811_driver_baseline_save_due(void);

/**
 * @brief Perform comprehensive CCS811 diagnostics
 *
 * Checks SX1509B GPIO expander state, pin configurations,
 * and CCS811 sensor status for debugging purposes.
 *
 * @return 0 on success, negative error code if issues found
 */
int ccs811_driver_run_diagnostics(void);

/**
 * @brief Manual reset sequence for CCS811
 *
 * Performs a manual reset sequence following Nordic's approach
 * when automatic initialization fails.
 *
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_manual_reset(void);

/**
 * @brief Check if CCS811 sensor is currently powered on
 *
 * @return true if sensor is powered on, false otherwise
 */
bool ccs811_driver_is_enabled(void);

/**
 * @brief Read air quality data from CCS811 sensor
 *
 * Handles environmental data updates, conditioning checks, and baseline management.
 * Implements adaptive sampling: starts in 1-second mode, switches to 60-second mode
 * after first successful sample for power optimization.
 *
 * @param co2_ppm Pointer to store CO2 value in ppm (can be NULL)
 * @param tvoc_ppb Pointer to store TVOC value in ppb (can be NULL)
 * @param temp_celsius Temperature for environmental compensation (can be NaN if not available)
 * @param humidity_percent Humidity for environmental compensation (can be NaN if not available)
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_read_air_quality(uint16_t *co2_ppm, uint16_t *tvoc_ppb, float temp_celsius,
				   float humidity_percent);

/**
 * @brief Read air quality data for BLE with cached fallback
 *
 * Optimized for BLE requests: tries to get fresh data but falls back to cached
 * readings if sensor is not ready. This ensures BLE responses are always
 * available when connected, using the most recent data available.
 *
 * @param co2_ppm Pointer to store CO2 value in ppm (can be NULL)
 * @param tvoc_ppb Pointer to store TVOC value in ppb (can be NULL)
 * @param temp_celsius Temperature for environmental compensation (can be NaN if not available)
 * @param humidity_percent Humidity for environmental compensation (can be NaN if not available)
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_read_for_ble(uint16_t *co2_ppm, uint16_t *tvoc_ppb, float temp_celsius,
			       float humidity_percent);

/**
 * @brief Get current CCS811 measurement mode
 *
 * @return Current measurement mode (enum ccs811_measurement_mode), or negative error code on
 * failure
 */
int ccs811_driver_get_mode(void);

/**
 * @brief Change CCS811 measurement mode
 *
 * Allows dynamic switching between different measurement intervals:
 * - CCS811_MODE_IDLE: No measurements
 * - CCS811_MODE_IAQ_1SEC: Measurement every 1 second
 * - CCS811_MODE_IAQ_10SEC: Measurement every 10 seconds
 * - CCS811_MODE_IAQ_60SEC: Measurement every 60 seconds
 * - CCS811_MODE_IAQ_250MSEC: Measurement every 250 milliseconds
 *
 * @param mode New measurement mode (use enum ccs811_measurement_mode or
 * CCS811_MODE_* constants)
 * @return 0 on success, negative error code on failure
 */
int ccs811_driver_set_mode(enum ccs811_measurement_mode mode);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_CCS811_DRIVER_H_ */
