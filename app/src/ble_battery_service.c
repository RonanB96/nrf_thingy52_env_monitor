/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_battery_service.h"
#include "battery_service.h" /* Hardware battery monitoring */

LOG_MODULE_REGISTER(ble_battery_service, CONFIG_LOG_DEFAULT_LEVEL);

static const uint8_t BATTERY_GOOD_THRESHOLD = 75U;
static const uint8_t BATTERY_LOW_THRESHOLD = 25U;
static const uint8_t BATTERY_CRITICAL_THRESHOLD = 10U;
static const uint8_t BLE_BAS_INIT_RETRY_MAX = 3U;
static const uint32_t BLE_BAS_RETRY_DELAY_MS = 50U;
static const uint32_t BLE_STACK_READY_DELAY_MS = 100U;
static const uint8_t BATTERY_LEVEL_MAX = 100U;
static struct k_work_delayable battery_poll_work;
static uint32_t connected_count;

/* BLE Battery service state */
static struct {
	uint8_t level; /* Battery level percentage (0-100) */
	bool charging; /* Charging status */
	enum bt_bas_bls_battery_present present;
	enum bt_bas_bls_wired_power_source wired_power;
	enum bt_bas_bls_battery_charge_state charge_state;
	enum bt_bas_bls_battery_charge_level charge_level;
	bool initialized;
} ble_battery_state = {
	.level = 0,
	.charging = false,
	.present = BT_BAS_BLS_BATTERY_PRESENT,
	.wired_power = BT_BAS_BLS_WIRED_POWER_NOT_CONNECTED,
	.charge_state = BT_BAS_BLS_CHARGE_STATE_UNKNOWN,
	.charge_level = BT_BAS_BLS_CHARGE_LEVEL_UNKNOWN,
	.initialized = false,
};

static void update_charge_level(uint8_t level)
{
	/* Update charge level based on battery percentage */
	if (level > BATTERY_GOOD_THRESHOLD) {
		ble_battery_state.charge_level = BT_BAS_BLS_CHARGE_LEVEL_GOOD;
	} else if (level > BATTERY_LOW_THRESHOLD) {
		ble_battery_state.charge_level = BT_BAS_BLS_CHARGE_LEVEL_LOW;
	} else {
		ble_battery_state.charge_level = BT_BAS_BLS_CHARGE_LEVEL_CRITICAL;
	}
}

static void update_charge_state(bool charging)
{
	if (charging) {
		ble_battery_state.charge_state = BT_BAS_BLS_CHARGE_STATE_CHARGING;
		ble_battery_state.wired_power = BT_BAS_BLS_WIRED_POWER_CONNECTED;
	} else {
		if (ble_battery_state.level > BATTERY_CRITICAL_THRESHOLD) {
			ble_battery_state.charge_state = BT_BAS_BLS_CHARGE_STATE_DISCHARGING_ACTIVE;
		} else {
			ble_battery_state.charge_state =
				BT_BAS_BLS_CHARGE_STATE_DISCHARGING_INACTIVE;
		}
		ble_battery_state.wired_power = BT_BAS_BLS_WIRED_POWER_NOT_CONNECTED;
	}
}

/* Callback function for charging status changes from hardware */
static void charging_status_changed(bool charging)
{
	if (!ble_battery_state.initialized || connected_count == 0U) {
		return;
	}

	ble_battery_state.charging = charging;
	update_charge_state(charging);

	ble_battery_service_update();
}

static void battery_poll_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!ble_battery_state.initialized || connected_count == 0U) {
		return;
	}

	int ret = ble_battery_service_update();
	if (ret != 0 && ret != -ENODATA) {
		LOG_WRN("Periodic BLE battery update failed: %d", ret);
	}

#if IS_ENABLED(CONFIG_SENSOR_PERIODIC_SAMPLING)
	k_work_reschedule(&battery_poll_work, K_SECONDS(CONFIG_SENSOR_ENV_INTERVAL_SEC));
#endif
}

int ble_battery_service_init(void)
{
	LOG_INF("Initializing BLE Battery Service");
	k_work_init_delayable(&battery_poll_work, battery_poll_work_handler);

	/* Wait a bit for Bluetooth stack to be fully ready */
	k_msleep((int32_t)BLE_STACK_READY_DELAY_MS);

	/* Initialize battery service with default values */
	bt_bas_bls_set_battery_present(ble_battery_state.present);
	bt_bas_bls_set_wired_external_power_source(ble_battery_state.wired_power);
	bt_bas_bls_set_wireless_external_power_source(BT_BAS_BLS_WIRELESS_POWER_NOT_CONNECTED);
	bt_bas_bls_set_battery_charge_state(ble_battery_state.charge_state);
	bt_bas_bls_set_battery_charge_level(ble_battery_state.charge_level);
	bt_bas_bls_set_battery_charge_type(BT_BAS_BLS_CHARGE_TYPE_UNKNOWN);
	bt_bas_bls_set_charging_fault_reason(BT_BAS_BLS_FAULT_REASON_NONE);

	battery_service_register_charging_callback(charging_status_changed);

	ble_battery_state.initialized = true;
	LOG_INF("BLE Battery Service initialized (updates on GATT connect)");
	return 0;
}

int ble_battery_service_on_connected(void)
{
	if (!ble_battery_state.initialized) {
		return -ENODEV;
	}

	connected_count++;
	LOG_INF("GATT client connected (count=%u): starting BAS updates",
		(unsigned int)connected_count);

	int ret = ble_battery_service_update();
	if (ret != 0 && ret != -ENODATA) {
		LOG_WRN("Initial BLE battery update on connect failed: %d", ret);
	}

#if IS_ENABLED(CONFIG_SENSOR_PERIODIC_SAMPLING)
	k_work_reschedule(&battery_poll_work, K_SECONDS(CONFIG_SENSOR_ENV_INTERVAL_SEC));
#endif
	return 0;
}

void ble_battery_service_on_disconnected(void)
{
	if (connected_count == 0U) {
		LOG_WRN("on_disconnected with no tracked connections");
		return;
	}

	connected_count--;
	LOG_INF("GATT client disconnected (count=%u)", (unsigned int)connected_count);

	if (connected_count == 0U) {
#if IS_ENABLED(CONFIG_SENSOR_PERIODIC_SAMPLING)
		k_work_cancel_delayable(&battery_poll_work);
#endif
	}
}

int ble_battery_service_update(void)
{
	if (!ble_battery_state.initialized) {
		LOG_ERR("BLE Battery service not initialized");
		return -ENODEV;
	}

	/* Get current values from hardware */
	int bat_level = battery_service_get_level();
	if (bat_level < 0) {
		LOG_WRN("Battery read failed: %d", bat_level);
		return bat_level;
	}
	bool charging = battery_service_is_charging();

	return ble_battery_service_update_manual((uint8_t)bat_level, charging);
}

int ble_battery_service_update_manual(uint8_t level, bool charging)
{
	if (!ble_battery_state.initialized) {
		LOG_ERR("BLE Battery service not initialized");
		return -ENODEV;
	}

	/* Validate battery level */
	if (level > BATTERY_LEVEL_MAX) {
		LOG_WRN("Invalid battery level %d, clamping to 100", level);
		level = BATTERY_LEVEL_MAX;
	}

	/* Check if values have changed */
	bool level_changed = (ble_battery_state.level != level);
	bool charging_changed = (ble_battery_state.charging != charging);

	if (!level_changed && !charging_changed) {
		/* No changes, skip update */
		return 0;
	}

	LOG_DBG("BLE battery update: level=%d%%, charging=%s", level, charging ? "true" : "false");

	/* Update internal state */
	ble_battery_state.level = level;
	ble_battery_state.charging = charging;

	/* Update derived states */
	update_charge_level(level);
	update_charge_state(charging);

	/* Update BAS characteristics */
	int ret = bt_bas_set_battery_level(level);
	if (ret) {
		LOG_ERR("Failed to update battery level: %d", ret);
		return ret;
	}

	bt_bas_bls_set_battery_charge_state(ble_battery_state.charge_state);
	bt_bas_bls_set_battery_charge_level(ble_battery_state.charge_level);
	bt_bas_bls_set_wired_external_power_source(ble_battery_state.wired_power);

	LOG_INF("BLE Battery updated: %d%% (%s, %s)", level, charging ? "charging" : "discharging",
		ble_battery_state.charge_level == BT_BAS_BLS_CHARGE_LEVEL_GOOD       ? "good"
		: ble_battery_state.charge_level == BT_BAS_BLS_CHARGE_LEVEL_LOW      ? "low"
		: ble_battery_state.charge_level == BT_BAS_BLS_CHARGE_LEVEL_CRITICAL ? "critical"
										     : "unknown");

	return 0;
}

uint8_t ble_battery_service_get_level(void)
{
	if (!ble_battery_state.initialized) {
		return 0;
	}

	return ble_battery_state.level;
}

bool ble_battery_service_is_charging(void)
{
	return ble_battery_state.charging;
}

int ble_battery_service_set_fault(bool fault_present)
{
	if (!ble_battery_state.initialized) {
		LOG_ERR("BLE Battery service not initialized");
		return -ENODEV;
	}

	bt_bas_bls_set_battery_fault(fault_present ? BT_BAS_BLS_BATTERY_FAULT_YES
						   : BT_BAS_BLS_BATTERY_FAULT_NO);

	LOG_INF("BLE Battery fault status set to: %s", fault_present ? "fault" : "no fault");

	return 0;
}

int ble_battery_service_set_service_required(bool required)
{
	if (!ble_battery_state.initialized) {
		LOG_ERR("BLE Battery service not initialized");
		return -ENODEV;
	}

	bt_bas_bls_set_service_required(required ? BT_BAS_BLS_SERVICE_REQUIRED_TRUE
						 : BT_BAS_BLS_SERVICE_REQUIRED_FALSE);

	LOG_INF("BLE Battery service required status set to: %s",
		required ? "required" : "not required");

	return 0;
}
