/*
 * Simple device naming using hardware ID only
 * Nordic Thingy:52 Environmental Monitor
 *
 * Generates unique device names like "T52-A1B2C3D4" (prefix defined by DEVICE_PREFIX)
 * Home Assistant handles room assignment
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/hwinfo.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "device_naming.h"

LOG_MODULE_REGISTER(device_naming, CONFIG_LOG_DEFAULT_LEVEL);

static char device_name_buffer[DEVICE_NAME_MAX_LEN];
static bool naming_initialized = false;

static const size_t DEVICE_NAME_MIN_BUF =
	16U; /* Minimum buffer to fit "T52-XXXXXXXX\0" (prefix + dash + 8 hex digits + NUL) */
static const int DEVICE_ID_BYTES_USED = 4; /* Last 4 bytes of HW ID used for unique suffix */
#define BITS_PER_BYTE_SHIFT CHAR_BIT       /* Bit shift per byte */

/* hwinfo returns up to 8 bytes of device ID on nRF52 */
#define HWINFO_DEVICE_ID_LEN 8U

/**
 * Get unique device name based on hardware ID
 *
 * @param name_buffer Buffer to store the device name
 * @param buffer_size Size of the buffer
 * @return 0 on success, negative error code on failure
 */
int device_naming_get_name(char *name_buffer, size_t buffer_size)
{
	uint8_t device_id[HWINFO_DEVICE_ID_LEN];
	ssize_t id_len;
	uint32_t unique_id = 0;

	if (!name_buffer || buffer_size < DEVICE_NAME_MIN_BUF) {
		return -EINVAL;
	}

	/* Return cached name if already initialized */
	if (naming_initialized) {
		strncpy(name_buffer, device_name_buffer, buffer_size - 1);
		name_buffer[buffer_size - 1] = '\0';
		return 0;
	}

	/* Get hardware device ID */
	id_len = hwinfo_get_device_id(device_id, sizeof(device_id));
	if (id_len <= 0) {
		LOG_ERR("Failed to get hardware ID, using fallback");
		(void)snprintf(name_buffer, buffer_size, "%s-UNKNOWN", DEVICE_PREFIX);
		return -ENODEV;
	}

	/* Create unique ID from last 4 bytes */
	for (int i = 0; i < DEVICE_ID_BYTES_USED && i < id_len; i++) {
		unique_id |= ((uint32_t)device_id[id_len - 1 - i] << (i * BITS_PER_BYTE_SHIFT));
	}

	/* Format as "Thingy52-XXXXXXXX" */
	(void)snprintf(name_buffer, buffer_size, "%s-%08X", DEVICE_PREFIX, unique_id);

	/* Cache the result */
	strncpy(device_name_buffer, name_buffer, sizeof(device_name_buffer) - 1);
	device_name_buffer[sizeof(device_name_buffer) - 1] = '\0';
	naming_initialized = true;

	LOG_INF("Device name: %s", name_buffer);
	return 0;
}

/**
 * Initialize device naming service
 *
 * @return 0 on success, negative error code on failure
 */
int device_naming_init(void)
{
	char temp_name[DEVICE_NAME_MAX_LEN];
	int ret;

	if (naming_initialized) {
		return 0;
	}

	ret = device_naming_get_name(temp_name, sizeof(temp_name));
	if (ret < 0) {
		LOG_ERR("Failed to initialize device naming: %d", ret);
		return ret;
	}

	LOG_INF("Device naming initialized: %s", temp_name);
	return 0;
}
