/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Shared FFF fakes for the Zephyr Bluetooth GATT API.
 *
 * `bt_gatt_notify` is a static inline that delegates to `bt_gatt_notify_cb`,
 * so we fake the cb variant. Tests that call the inline wrapper will go
 * through the fake transparently.
 *
 * Patterned on zephyrproject-rtos/zephyr tests/bluetooth/audio/mocks/.
 */

#ifndef APP_TESTS_UNIT_COMMON_MOCKS_BT_GATT_H_
#define APP_TESTS_UNIT_COMMON_MOCKS_BT_GATT_H_

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/fff.h>

#define BT_GATT_FFF_FAKES_LIST(FAKE)                                                               \
	FAKE(bt_gatt_attr_read)                                                                    \
	FAKE(bt_gatt_attr_read_service)                                                            \
	FAKE(bt_gatt_attr_read_chrc)                                                               \
	FAKE(bt_gatt_attr_read_cud)                                                                \
	FAKE(bt_gatt_attr_read_ccc)                                                                \
	FAKE(bt_gatt_attr_write_ccc)                                                               \
	FAKE(bt_gatt_notify_cb)                                                                    \
	FAKE(bt_gatt_service_register)

DECLARE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read, struct bt_conn *, const struct bt_gatt_attr *,
			void *, uint16_t, uint16_t, const void *, uint16_t);

/* The following helpers are referenced by BT_GATT_PRIMARY_SERVICE / CHARACTERISTIC /
 * CUD / CCC macros via the static attribute table. They must resolve at link
 * time even though tests do not exercise them directly.
 */
DECLARE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_service, struct bt_conn *,
			const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_chrc, struct bt_conn *,
			const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_cud, struct bt_conn *,
			const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_read_ccc, struct bt_conn *,
			const struct bt_gatt_attr *, void *, uint16_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(ssize_t, bt_gatt_attr_write_ccc, struct bt_conn *,
			const struct bt_gatt_attr *, const void *, uint16_t, uint16_t, uint8_t);

DECLARE_FAKE_VALUE_FUNC(int, bt_gatt_notify_cb, struct bt_conn *, struct bt_gatt_notify_params *);

DECLARE_FAKE_VALUE_FUNC(int, bt_gatt_service_register, struct bt_gatt_service *);

#endif /* APP_TESTS_UNIT_COMMON_MOCKS_BT_GATT_H_ */
