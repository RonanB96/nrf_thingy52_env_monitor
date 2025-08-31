/*
 * Simple device naming using hardware ID only
 * Nordic Thingy:52 Environmental Monitor
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_NAMING_SIMPLE_H
#define DEVICE_NAMING_SIMPLE_H

#include <stddef.h>

/* Device naming constants */
#define DEVICE_PREFIX "T52"
#define DEVICE_NAME_MAX_LEN 64

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize device naming service
 *
 * @return 0 on success, negative error code on failure
 */
int device_naming_init(void);

/**
 * Get unique device name based on hardware ID
 *
 * @param name_buffer Buffer to store the device name
 * @param buffer_size Size of the buffer
 * @return 0 on success, negative error code on failure
 */
int device_naming_get_name(char *name_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_NAMING_SIMPLE_H */
