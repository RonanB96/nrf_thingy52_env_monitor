/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mock_battery_service.h"

DEFINE_FAKE_VALUE_FUNC(int, battery_service_init);
DEFINE_FAKE_VALUE_FUNC(int, battery_service_get_level);
DEFINE_FAKE_VALUE_FUNC(bool, battery_service_is_charging);
DEFINE_FAKE_VALUE_FUNC(int, battery_service_sample);
DEFINE_FAKE_VOID_FUNC(battery_service_register_charging_callback, battery_charging_cb_t);
