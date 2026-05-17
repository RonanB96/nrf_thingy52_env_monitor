/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Model handler
 */

#ifndef MODEL_HANDLER_H__
#define MODEL_HANDLER_H__

#include <zephyr/bluetooth/mesh.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

const struct bt_mesh_comp *model_handler_init(void);

/* Shared work queue for sensor operations */
extern struct k_work_q sensor_work_q;

#ifdef __cplusplus
}
#endif

#endif /* MODEL_HANDLER_H__ */
