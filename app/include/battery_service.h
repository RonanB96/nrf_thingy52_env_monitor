/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BATTERY_SERVICE_H_
#define BATTERY_SERVICE_H_

#include <zephyr/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*battery_charging_cb_t)(bool charging);

/**
 * @brief Initialize the battery service
 *
 * This function initializes the BLE Battery Service and starts
 * periodic battery level measurements using the Thingy52 voltage divider.
 *
 * @return 0 on success, negative error code on failure
 */
int battery_service_init(void);

/**
 * @brief Get the current battery level
 *
 * @return Battery level in percentage (0-100), or negative error code on failure
 */
int battery_service_get_level(void);

/**
 * @brief Sample battery level now
 *
 * Takes an immediate battery measurement and updates the cached level.
 * This should be called periodically by the main sensor sampling loop.
 *
 * @return 0 on success, negative error code on failure
 */
int battery_service_sample(void);

/**
 * @brief Check if battery is charging
 *
 * @return true if battery is charging, false if not charging or charging finished
 */
bool battery_service_is_charging(void);

/**
 * @brief Register callback for charging status changes
 *
 * @param callback Function to call when charging status changes
 */
void battery_service_register_charging_callback(battery_charging_cb_t callback);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SERVICE_H_ */
