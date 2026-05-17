/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * In-app dependency fakes for ble_advertiser unit test.
 */

#include <stddef.h>
#include <string.h>
#include "device_naming.h"
#include "sensor_manager.h"

int device_naming_init(void)
{
	return 0;
}

int device_naming_get_name(char *buffer, size_t buffer_size)
{
	if (buffer == NULL || buffer_size == 0) {
		return -1;
	}
	const char *name = "thingy52-TEST0001";
	size_t n = strlen(name);
	if (n >= buffer_size) {
		n = buffer_size - 1;
	}
	memcpy(buffer, name, n);
	buffer[n] = '\0';
	return 0;
}

int sensor_manager_on_connected(void)
{
	return 0;
}

void sensor_manager_on_disconnected(void)
{
}
