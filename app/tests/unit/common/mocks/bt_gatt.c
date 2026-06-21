/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "bt_gatt.h"

DEFINE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read, struct bt_conn *, const struct bt_gatt_attr *,
		       void *, uint16_t, uint16_t, const void *, uint16_t);

DEFINE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_service, struct bt_conn *,
		       const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DEFINE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_chrc, struct bt_conn *,
		       const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DEFINE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_cud, struct bt_conn *,
		       const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DEFINE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_ccc, struct bt_conn *,
		       const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DEFINE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_write_ccc, struct bt_conn *,
		       const struct bt_gatt_attr *, const void *, uint16_t, uint16_t, uint8_t);

DEFINE_FAKE_VALUE_FUNC(int, bt_gatt_notify_cb, struct bt_conn *, struct bt_gatt_notify_params *);

DEFINE_FAKE_VALUE_FUNC(int, bt_gatt_service_register, struct bt_gatt_service *);
