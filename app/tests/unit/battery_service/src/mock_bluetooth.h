/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_BLUETOOTH_H_
#define MOCK_BLUETOOTH_H_

#include <zephyr/fff.h>
#include <zephyr/bluetooth/services/bas.h>

#define BT_BAS_FFF_FAKES_LIST(FAKE)                                                                \
	FAKE(bt_bas_set_battery_level)                                                             \
	FAKE(bt_bas_bls_set_battery_present)                                                       \
	FAKE(bt_bas_bls_set_wired_external_power_source)                                           \
	FAKE(bt_bas_bls_set_wireless_external_power_source)                                        \
	FAKE(bt_bas_bls_set_battery_charge_state)                                                  \
	FAKE(bt_bas_bls_set_battery_charge_level)                                                  \
	FAKE(bt_bas_bls_set_battery_charge_type)                                                   \
	FAKE(bt_bas_bls_set_charging_fault_reason)                                                 \
	FAKE(bt_bas_bls_set_identifier)                                                            \
	FAKE(bt_bas_bls_set_service_required)                                                      \
	FAKE(bt_bas_bls_set_battery_fault)

DECLARE_FAKE_VALUE_FUNC(int, bt_bas_set_battery_level, uint8_t);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_present, enum bt_bas_bls_battery_present);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_wired_external_power_source,
		       enum bt_bas_bls_wired_power_source);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_wireless_external_power_source,
		       enum bt_bas_bls_wireless_power_source);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_charge_state, enum bt_bas_bls_battery_charge_state);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_charge_level, enum bt_bas_bls_battery_charge_level);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_charge_type, enum bt_bas_bls_battery_charge_type);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_charging_fault_reason, enum bt_bas_bls_charging_fault_reason);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_identifier, uint16_t);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_service_required, enum bt_bas_bls_service_required);
DECLARE_FAKE_VOID_FUNC(bt_bas_bls_set_battery_fault, enum bt_bas_bls_battery_fault);

#endif /* MOCK_BLUETOOTH_H_ */
