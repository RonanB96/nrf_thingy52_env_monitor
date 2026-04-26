/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic BLE environmental sensor
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <string.h>
#include "ble_advertiser.h"
#include "ble_battery_service.h"
#include "ess_service.h"
#include "sensor_manager.h"
#include "uptime_service.h"
#include "device_naming.h"
#include "board.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
	int ret;
	struct sensor_data sensor_data;
	struct ble_sensor_data ble_data;

	LOG_INF("Starting BLE Environmental Monitor");

	/* Allow GPIO hog driver to complete initialization */
	k_msleep(500);

	/* Initialize device naming service first */
	LOG_DBG("Initializing device naming service");
	ret = device_naming_init();
	if (ret) {
		LOG_ERR("Device naming service initialization failed: %d", ret);
		/* Continue with fallback naming */
	}

	/* Initialize BLE advertiser */
	LOG_DBG("Initializing BLE advertiser");
	ret = ble_advertiser_init();
	if (ret) {
		LOG_ERR("BLE advertiser initialization failed: %d", ret);
		return ret;
	}
	LOG_INF("BLE advertiser initialized");

	/* Initialize settings */
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		LOG_DBG("Initializing settings subsystem");
		ret = settings_subsys_init();
		if (ret) {
			LOG_ERR("Settings init failed: %d", ret);
			return ret;
		}
		LOG_DBG("Loading settings");
		settings_load();
		LOG_DBG("Settings loaded successfully");
	}

	/* Initialize sensor manager */
	LOG_DBG("Initializing sensor manager");
	ret = sensor_manager_init();
	if (ret) {
		LOG_ERR("Sensor manager initialization failed: %d", ret);
		return ret;
	}
	LOG_INF("Sensor manager initialized");

	/* Initialize BLE Battery Service after hardware is ready */
	LOG_DBG("Initializing BLE Battery Service");
	ret = ble_battery_service_init();
	if (ret) {
		LOG_ERR("BLE Battery Service initialization failed: %d", ret);
		return ret;
	}
	LOG_INF("BLE Battery Service initialized");

	/* Initialize Environmental Sensing Service */
	LOG_DBG("Initializing Environmental Sensing Service");
	ret = ess_service_init();
	if (ret) {
		LOG_ERR("ESS initialization failed: %d", ret);
		return ret;
	}
	LOG_INF("Environmental Sensing Service initialized");

	/* Start periodic sensor readings */
	sensor_manager_start_periodic(CONFIG_SENSOR_ENV_INTERVAL_SEC * 1000U);
	LOG_INF("Periodic sensor updates started");

	/* Initialize Uptime Service */
	LOG_DBG("Initializing Uptime Service");
	ret = uptime_service_init();
	if (ret) {
		LOG_ERR("Uptime Service initialization failed: %d", ret);
		return ret;
	}
	LOG_INF("Uptime Service initialized");

	/* Get initial sensor data (already read during sensor_manager_init) */
	LOG_DBG("Getting initial sensor data");
	ret = sensor_manager_get_data(&sensor_data);
	if (ret) {
		LOG_ERR("Failed to get initial sensor data: %d", ret);
		/* Use default values */
		memset(&sensor_data, 0, sizeof(sensor_data));
		LOG_WRN("Using default sensor values");
	}

	/* Small delay to ensure Bluetooth stack is ready */
	LOG_DBG("Waiting for Bluetooth stack to be ready");
	k_msleep(100);

	LOG_DBG("Starting BLE advertising");
	ret = ble_advertiser_start(&ble_data);
	if (ret) {
		LOG_ERR("Failed to start BLE advertising: %d", ret);
		return ret;
	}
	LOG_INF("BLE advertising started");

	ret = ble_battery_service_update();
	if (ret) {
		LOG_WRN("Failed to update BLE Battery Service: %d", ret);
	} else {
		LOG_DBG("BLE Battery Service updated successfully");
	}

	/* Update Environmental Sensing Service with initial data */
	LOG_DBG("Updating Environmental Sensing Service with initial data");
	ret = ess_service_update(&sensor_data);
	if (ret) {
		LOG_WRN("Failed to update ESS with initial data: %d", ret);
	} else {
		LOG_DBG("Environmental Sensing Service updated with initial data");
	}

	/* Print GPIO pin states again - after GPIO hogs should be applied */
	LOG_INF("=== GPIO States AFTER hog initialization delay ===");
	board_print_pin_states();

	LOG_INF("System initialized - Environmental monitoring active");

	return 0;
}
