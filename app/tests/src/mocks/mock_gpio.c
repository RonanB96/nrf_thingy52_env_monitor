/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mock_gpio, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Mock GPIO driver for testing
 * 
 * This file provides mock implementations for GPIO operations
 * used by the sensor drivers and power management.
 */

void mock_gpio_init(void)
{
    LOG_DBG("Mock GPIO system initialized");
}

void mock_gpio_reset(void)
{
    LOG_DBG("Mock GPIO state reset");
}