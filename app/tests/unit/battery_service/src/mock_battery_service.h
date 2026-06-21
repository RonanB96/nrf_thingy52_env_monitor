/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_BATTERY_SERVICE_H_
#define MOCK_BATTERY_SERVICE_H_

#include <zephyr/fff.h>
#include "battery_service.h"

#define BATTERY_SERVICE_FFF_FAKES_LIST(FAKE)                                                       \
	FAKE(battery_service_init)                                                                 \
	FAKE(battery_service_get_level)                                                            \
	FAKE(battery_service_is_charging)                                                          \
	FAKE(battery_service_sample)                                                               \
	FAKE(battery_service_register_charging_callback)

DECLARE_FAKE_VALUE_FUNC(int, battery_service_init);
DECLARE_FAKE_VALUE_FUNC(int, battery_service_get_level);
DECLARE_FAKE_VALUE_FUNC(bool, battery_service_is_charging);
DECLARE_FAKE_VALUE_FUNC(int, battery_service_sample);
DECLARE_FAKE_VOID_FUNC(battery_service_register_charging_callback, battery_charging_cb_t);

#endif /* MOCK_BATTERY_SERVICE_H_ */
