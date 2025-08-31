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
#include <hal/nrf_ficr.h>
#include <string.h>
#include "ble_advertiser.h"
#include "battery_service.h"
#include "sensor_manager.h"
#include "sensor_config.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* BLE advertising update work */
static struct k_work_delayable advertising_work;

/* Convert sensor manager data to BLE format */
static void convert_sensor_data(const struct sensor_data *sensor_data,
                               struct ble_sensor_data *ble_data)
{
	/* Convert temperature from °C to 0.01°C */
	ble_data->temperature = (uint16_t)(sensor_data->temperature * 100.0f);

	/* Convert humidity from % to 0.01% */
	ble_data->humidity = (uint16_t)(sensor_data->humidity * 100.0f);

	/* Convert pressure from hPa to 0.1 hPa */
	ble_data->pressure = (uint16_t)(sensor_data->pressure * 10.0f);

	/* Copy integer values directly */
	ble_data->eco2 = sensor_data->eco2;
	ble_data->tvoc = sensor_data->tvoc;
	ble_data->battery_level = sensor_data->battery_level;
}

static void advertising_work_handler(struct k_work *work)
{
	struct sensor_data sensor_data;
	struct ble_sensor_data ble_data;
	int ret;

	/* Update sensor readings */
	ret = sensor_manager_update();
	if (ret) {
		LOG_WRN("Sensor update failed: %d", ret);
	}

	/* Get current sensor data */
	ret = sensor_manager_get_data(&sensor_data);
	if (ret) {
		LOG_ERR("Failed to get sensor data: %d", ret);
		goto reschedule;
	}

	/* Convert to BLE format */
	convert_sensor_data(&sensor_data, &ble_data);

	/* Update advertisement */
	ret = ble_advertiser_update(&ble_data);
	if (ret) {
		LOG_ERR("Failed to update advertisement: %d", ret);
	}

reschedule:
	/* Reschedule for next update */
	k_work_reschedule(&advertising_work, K_MSEC(MESH_ADVERTISING_INTERVAL_MS));
}

/* Provisioning callbacks */
static void prov_complete(uint16_t net_idx, uint16_t addr)
{
	printk("Provisioning complete! Net IDX: 0x%04x, Address: 0x%04x\n",
		   net_idx, addr);
}

static void prov_reset(void)
{
	printk("Device has been reset and unprovisioned\n");
	/* Re-enable provisioning beacons */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

/* Pre-shared authentication key for production security (128-bit) */
static const uint8_t static_auth_key[16] = {
	0xA5, 0x3E, 0x60, 0x37, 0x8D, 0x2A, 0xEE, 0x64,
	0x3F, 0x63, 0x4F, 0xD3, 0xE5, 0x2D, 0xDE, 0x11
};

/* Provisioning structure */
static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.complete = prov_complete,
	.reset = prov_reset,

	/* Static OOB for production security - prevents unauthorized provisioning */
	.output_size = 0,
	.output_actions = 0,
	.input_size = 0,
	.input_actions = 0,
	.static_val = static_auth_key,
	.static_val_len = 16,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Initialize sensors first */
	err = sensor_manager_init();
	if (err) {
		printk("Sensor manager initialization failed (err %d)\n", err);
		return;
	}
	printk("Sensor manager initialized\n");

	/* Initialize BLE Mesh models */
	const struct bt_mesh_comp *comp = model_handler_init();
	if (!comp) {
		printk("Model handler initialization failed - BLE Mesh models not ready!\n");
		return;
	}

	err = bt_mesh_init(&prov, comp);
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* Check if already provisioned */
	if (bt_mesh_is_provisioned()) {
		printk("Mesh node already provisioned - ready for operation\n");

		/* Enable GATT Proxy if supported */
		if (IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY)) {
			bt_mesh_proxy_identity_enable();
			printk("GATT Proxy identity enabled\n");
		}
	} else {
		printk("Mesh node not provisioned - enabling provisioning\n");
		/* Enable provisioning for "turn on and forget" - device will be discoverable */
		err = bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
		if (err) {
			printk("Failed to enable provisioning (err %d)\n", err);
			return;
		}
		printk("Device is now discoverable for provisioning\n");
		printk("UUID: ");
		for (int i = 0; i < 16; i++) {
			printk("%02x", dev_uuid[i]);
		}
		printk("\n");
		printk("Use nRF Mesh app or similar to provision this device\n");
	}

	printk("Mesh initialized - Environmental monitoring active\n");

	/* Debug: Print stack usage information */
	#if CONFIG_THREAD_ANALYZER
	thread_analyzer_print();
	#endif

	/* Initialize battery service after mesh is ready */
	err = battery_service_init();
	if (err) {
		printk("Battery service initialization failed (err %d)\n", err);
		/* Continue even if battery service fails */
	} else {
		printk("Battery service initialized\n");
	}

	/* Debug: Print stack usage information after initialization */
	#if CONFIG_THREAD_ANALYZER
	printk("=== Stack Analysis After Initialization ===\n");
	thread_analyzer_print();
	#endif
}

int main(void)
{
	int err;

	printk("Initializing...\n");

	/* Generate unique UUID from hardware device ID */
	generate_device_uuid();

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	return 0;
}
