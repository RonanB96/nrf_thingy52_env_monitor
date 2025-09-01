/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mock_sensor, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Mock sensor driver for testing
 * 
 * This file provides mock implementations for Zephyr sensor API
 * used by the environmental sensors.
 */

void mock_sensor_init(void)
{
    LOG_DBG("Mock sensor system initialized");
}

void mock_sensor_reset(void)
{
    LOG_DBG("Mock sensor state reset");
}