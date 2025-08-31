/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef UPTIME_SERVICE_H_
#define UPTIME_SERVICE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom Uptime Service UUID
 *
 * Custom 16-bit UUID for the uptime service.
 * In a production environment, you should use a 128-bit UUID
 * to avoid conflicts with other services.
 */
#define BT_UUID_UPTIME_SERVICE_VAL 0x1900

/**
 * @brief Uptime Characteristic UUID
 *
 * Custom 16-bit UUID for the uptime characteristic.
 */
#define BT_UUID_UPTIME_CHAR_VAL 0x1901

/**
 * @brief Initialize the Uptime Service
 *
 * This function initializes the uptime GATT service.
 *
 * @return 0 on success, negative error code on failure
 */
int uptime_service_init(void);

/**
 * @brief Update the uptime value
 *
 * This function updates the current uptime value in the service.
 * Should be called when a BLE client connects to provide fresh data.
 *
 * @return 0 on success, negative error code on failure
 */
int uptime_service_update(void);

/**
 * @brief Get current uptime in seconds
 *
 * @return Current uptime in seconds since boot
 */
uint64_t uptime_service_get_uptime_seconds(void);

#ifdef __cplusplus
}
#endif

#endif /* UPTIME_SERVICE_H_ */
