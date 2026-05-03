/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef POWER_CONFIG_H__
#define POWER_CONFIG_H__

/* Mesh publishing intervals */
#define MESH_PUBLISH_INTERVAL_S (30 * 60) /* 30 minutes */

/* Low power node poll intervals */
#define LPN_POLL_INTERVAL_S (60) /* 1 minute */

/* Power management timeouts */
#define MESH_IDLE_TIMEOUT_S (120) /* 2 minutes */

#endif /* POWER_CONFIG_H__ */
