/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sensor_manager_mock.h"

DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_init);
DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_get_data, struct sensor_data *);
DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_update);
DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_update_selective, enum sensor_select);
DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_register_callback, sensor_update_callback_t);
DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_arm);
DEFINE_FAKE_VALUE_FUNC(int, sensor_manager_on_connected);
DEFINE_FAKE_VOID_FUNC(sensor_manager_on_disconnected);
DEFINE_FAKE_VALUE_FUNC(bool, sensor_manager_is_ccs811_ready);
DEFINE_FAKE_VALUE_FUNC(float, sensor_manager_get_temperature);
DEFINE_FAKE_VALUE_FUNC(float, sensor_manager_get_humidity);
DEFINE_FAKE_VALUE_FUNC(float, sensor_manager_get_pressure);
DEFINE_FAKE_VALUE_FUNC(uint16_t, sensor_manager_get_eco2);
DEFINE_FAKE_VALUE_FUNC(uint16_t, sensor_manager_get_tvoc);
DEFINE_FAKE_VALUE_FUNC(uint8_t, sensor_manager_get_battery_level);
DEFINE_FAKE_VOID_FUNC(sensor_manager_update_air_quality_for_ble);
