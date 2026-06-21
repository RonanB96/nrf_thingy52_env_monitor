/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include "mock_bluetooth.h"

LOG_MODULE_REGISTER(mock_bluetooth, CONFIG_LOG_DEFAULT_LEVEL);

DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(int, bt_bas_set_battery_level, uint8_t);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_present, enum bt_bas_bls_battery_present);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_wired_external_power_source,
		      enum bt_bas_bls_wired_power_source);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_wireless_external_power_source,
		      enum bt_bas_bls_wireless_power_source);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_charge_state, enum bt_bas_bls_battery_charge_state);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_charge_level, enum bt_bas_bls_battery_charge_level);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_charge_type, enum bt_bas_bls_battery_charge_type);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_charging_fault_reason, enum bt_bas_bls_charging_fault_reason);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_identifier, uint16_t);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_service_required, enum bt_bas_bls_service_required);
DEFINE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_fault, enum bt_bas_bls_battery_fault);
