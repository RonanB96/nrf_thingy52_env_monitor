/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mock_bluetooth, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Mock Bluetooth driver for testing
 * 
 * This file provides mock implementations for Bluetooth GATT operations
 * used by the BLE services.
 */

void mock_bluetooth_init(void)
{
    LOG_DBG("Mock Bluetooth system initialized");
}

void mock_bluetooth_reset(void)
{
    LOG_DBG("Mock Bluetooth state reset");
}