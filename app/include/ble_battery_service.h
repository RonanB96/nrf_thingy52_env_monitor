/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_BATTERY_SERVICE_H_
#define BLE_BATTERY_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BLE Battery Service
 *
 * This function initializes the Bluetooth Battery Service (BAS) and sets up
 * initial battery characteristics. It integrates with the hardware battery
 * service to get initial values.
 *
 * @return 0 on success, negative error code on failure
 */
int ble_battery_service_init(void);

/**
 * @brief Update BLE battery service from hardware
 *
 * Reads current battery level and charging status from the hardware
 * battery service and updates the BLE Battery Service characteristics.
 * Only updates if values have changed.
 *
 * @return 0 on success, negative error code on failure
 */
int ble_battery_service_update(void);

/**
 * @brief Manually update BLE battery service
 *
 * Updates the BLE Battery Service characteristics with provided values.
 * Use this when you have battery data from sensors or other sources.
 *
 * @param level Battery level in percentage (0-100)
 * @param charging True if battery is charging, false otherwise
 *
 * @return 0 on success, negative error code on failure
 */
int ble_battery_service_update_manual(uint8_t level, bool charging);

/**
 * @brief Get current cached battery level
 *
 * Returns the last known battery level from the BLE service cache.
 *
 * @return Battery level in percentage (0-100), or 0 if not initialized
 */
uint8_t ble_battery_service_get_level(void);

/**
 * @brief Get current cached charging status
 *
 * Returns the last known charging status from the BLE service cache.
 *
 * @return true if charging, false if not charging
 */
bool ble_battery_service_is_charging(void);

/**
 * @brief Set battery fault status
 *
 * Updates the Battery Level Status characteristic with fault information.
 *
 * @param fault_present True if battery fault is present, false otherwise
 *
 * @return 0 on success, negative error code on failure
 */
int ble_battery_service_set_fault(bool fault_present);

/**
 * @brief Set service required status
 *
 * Updates the Battery Level Status characteristic to indicate if battery
 * service is required.
 *
 * @param required True if service is required, false otherwise
 *
 * @return 0 on success, negative error code on failure
 */
int ble_battery_service_set_service_required(bool required);

#ifdef __cplusplus
}
#endif

#endif /* BLE_BATTERY_SERVICE_H_ */
