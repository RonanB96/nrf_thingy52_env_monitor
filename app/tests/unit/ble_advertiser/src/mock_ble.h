/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_BLE_H_
#define MOCK_BLE_H_

#include <zephyr/fff.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/drivers/hwinfo.h>

DECLARE_FAKE_VALUE_FUNC(int, bt_enable, bt_ready_cb_t);
DECLARE_FAKE_VALUE_FUNC(int, bt_le_adv_start, const struct bt_le_adv_param *,
			const struct bt_data *, size_t, const struct bt_data *, size_t);
DECLARE_FAKE_VALUE_FUNC(int, bt_le_adv_stop);
DECLARE_FAKE_VALUE_FUNC(int, bt_conn_cb_register, struct bt_conn_cb *);
DECLARE_FAKE_VALUE_FUNC(int, bt_id_create, bt_addr_le_t *, uint8_t *);
DECLARE_FAKE_VALUE_FUNC(ssize_t, z_impl_hwinfo_get_device_id, uint8_t *, size_t);

int mock_ble_enable_success(bt_ready_cb_t cb);
int mock_ble_enable_failure(bt_ready_cb_t cb);
ssize_t mock_hwinfo_zero_id(uint8_t *buf, size_t length);
void mock_ble_reset(void);

#endif /* MOCK_BLE_H_ */
