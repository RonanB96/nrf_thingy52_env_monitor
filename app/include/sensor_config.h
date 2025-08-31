/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Centralized sensor configuration
 *
 * This file provides configuration for sensor sampling intervals used
 * by the sensor manager for periodic readings.
 */

/* Primary sensor sampling interval (in seconds) */
#define SENSOR_SAMPLING_INTERVAL_SEC 30

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_CONFIG_H */
