/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Single FFF globals translation unit. Link this into any test binary that
 * uses one of the shared mock libraries; do not also `DEFINE_FFF_GLOBALS`
 * elsewhere in the same binary.
 */

#include <zephyr/fff.h>

DEFINE_FFF_GLOBALS;
