/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "uptime_service.h"

LOG_MODULE_REGISTER(uptime_service, CONFIG_LOG_DEFAULT_LEVEL);

/* Custom UUID declarations */
#define BT_UUID_UPTIME_SERVICE BT_UUID_DECLARE_16(BT_UUID_UPTIME_SERVICE_VAL)
#define BT_UUID_UPTIME_CHAR    BT_UUID_DECLARE_16(BT_UUID_UPTIME_CHAR_VAL)

/* Service state */
static bool uptime_service_initialized = false;
static uint64_t current_uptime_seconds = 0;

/* GATT read callback for uptime characteristic */
static ssize_t read_uptime(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	/* Update uptime before reading */
	current_uptime_seconds = k_uptime_get() / 1000;

	/* Convert to little-endian for BLE transmission */
	uint64_t uptime_le = sys_cpu_to_le64(current_uptime_seconds);

	LOG_DBG("Uptime read: %llu seconds", current_uptime_seconds);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &uptime_le, sizeof(uptime_le));
}

/* Uptime Service GATT definition */
BT_GATT_SERVICE_DEFINE(uptime_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_UPTIME_SERVICE),

		       /* Uptime Characteristic */
		       BT_GATT_CHARACTERISTIC(BT_UUID_UPTIME_CHAR, BT_GATT_CHRC_READ,
					      BT_GATT_PERM_READ, read_uptime, NULL, NULL),
		       BT_GATT_CUD("Device Uptime (seconds)", BT_GATT_PERM_READ), );

int uptime_service_init(void)
{
	if (uptime_service_initialized) {
		return 0;
	}

	LOG_INF("Initializing Uptime Service");

	/* Service is automatically registered via BT_GATT_SERVICE_DEFINE */
	uptime_service_initialized = true;

	/* Initialize uptime value */
	current_uptime_seconds = k_uptime_get() / 1000;

	LOG_INF("Uptime Service initialized (Current uptime: %llu seconds)",
		current_uptime_seconds);

	return 0;
}

int uptime_service_update(void)
{
	if (!uptime_service_initialized) {
		LOG_ERR("Uptime service not initialized");
		return -ENODEV;
	}

	current_uptime_seconds = k_uptime_get() / 1000;

	LOG_DBG("Uptime updated: %llu seconds", current_uptime_seconds);

	return 0;
}

uint64_t uptime_service_get_uptime_seconds(void)
{
	/* For higher resolution, you could use:
	 * - k_uptime_get() for milliseconds (uint64_t ms)
	 * - k_uptime_get_32() for 32-bit milliseconds (overflows ~49 days)
	 * - k_cycle_get_64() for CPU cycles (highest resolution)
	 * - Hardware RTC for calendar time
	 *
	 * For long-running uptime in seconds, current approach is optimal
	 */
	return k_uptime_get() / 1000;
}
