/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Shared FFF fakes for the application sensor_manager API. Used by unit
 * tests for components that consume sensor data (ess_service,
 * ble_advertiser).
 */

#ifndef APP_TESTS_UNIT_COMMON_MOCKS_SENSOR_MANAGER_H_
#define APP_TESTS_UNIT_COMMON_MOCKS_SENSOR_MANAGER_H_

#include <zephyr/fff.h>

#include "sensor_manager.h"

#define SENSOR_MANAGER_FFF_FAKES_LIST(FAKE)                                                        \
	FAKE(sensor_manager_init)                                                                  \
	FAKE(sensor_manager_get_data)                                                              \
	FAKE(sensor_manager_update)                                                                \
	FAKE(sensor_manager_update_selective)                                                      \
	FAKE(sensor_manager_register_callback)                                                     \
	FAKE(sensor_manager_arm)                                                                   \
	FAKE(sensor_manager_on_connected)                                                          \
	FAKE(sensor_manager_on_disconnected)                                                       \
	FAKE(sensor_manager_is_ccs811_ready)                                                       \
	FAKE(sensor_manager_get_temperature)                                                       \
	FAKE(sensor_manager_get_humidity)                                                          \
	FAKE(sensor_manager_get_pressure)                                                          \
	FAKE(sensor_manager_get_eco2)                                                              \
	FAKE(sensor_manager_get_tvoc)                                                              \
	FAKE(sensor_manager_get_battery_level)                                                     \
	FAKE(sensor_manager_update_air_quality_for_ble)

DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_init);
DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_get_data, struct sensor_data *);
DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_update);
DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_update_selective, enum sensor_select);
DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_register_callback, sensor_update_callback_t);
DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_arm);
DECLARE_FAKE_VALUE_FUNC(int, sensor_manager_on_connected);
DECLARE_FAKE_VOID_FUNC(sensor_manager_on_disconnected);
DECLARE_FAKE_VALUE_FUNC(bool, sensor_manager_is_ccs811_ready);
DECLARE_FAKE_VALUE_FUNC(float, sensor_manager_get_temperature);
DECLARE_FAKE_VALUE_FUNC(float, sensor_manager_get_humidity);
DECLARE_FAKE_VALUE_FUNC(float, sensor_manager_get_pressure);
DECLARE_FAKE_VALUE_FUNC(uint16_t, sensor_manager_get_eco2);
DECLARE_FAKE_VALUE_FUNC(uint16_t, sensor_manager_get_tvoc);
DECLARE_FAKE_VALUE_FUNC(uint8_t, sensor_manager_get_battery_level);
DECLARE_FAKE_VOID_FUNC(sensor_manager_update_air_quality_for_ble);

#endif /* APP_TESTS_UNIT_COMMON_MOCKS_SENSOR_MANAGER_H_ */
