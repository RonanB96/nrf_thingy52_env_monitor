/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * FFF fakes for the small slice of the BT host + hwinfo API consumed by
 * app/src/ble_advertiser.c. CONFIG_BT=n means the real symbols are not
 * compiled in, so these provide them.
 */

#include <zephyr/fff.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/drivers/hwinfo.h>
#include <stddef.h>
#include <string.h>

#include "mock_ble.h"

DEFINE_FAKE_VALUE_FUNC(int, bt_enable, bt_ready_cb_t);
DEFINE_FAKE_VALUE_FUNC(int, bt_le_adv_start, const struct bt_le_adv_param *,
		       const struct bt_data *, size_t, const struct bt_data *, size_t);
DEFINE_FAKE_VALUE_FUNC(int, bt_le_adv_stop);
DEFINE_FAKE_VALUE_FUNC(int, bt_conn_cb_register, struct bt_conn_cb *);
DEFINE_FAKE_VALUE_FUNC(int, bt_id_create, bt_addr_le_t *, uint8_t *);
/* hwinfo_get_device_id is a Zephyr syscall: production code calls the
 * static-inline wrapper from <zephyr/syscalls/hwinfo.h> which dispatches
 * to z_impl_hwinfo_get_device_id. Fake the impl symbol.
 */
DEFINE_FAKE_VALUE_FUNC(ssize_t, z_impl_hwinfo_get_device_id, uint8_t *, size_t);

/* Default custom_fake for bt_enable: invoke the ready callback synchronously
 * with err=0, mirroring the behaviour ble_advertiser.c expects.
 */
int mock_ble_enable_success(bt_ready_cb_t cb)
{
	if (cb != NULL) {
		cb(0);
	}
	return 0;
}

int mock_ble_enable_failure(bt_ready_cb_t cb)
{
	(void)cb;
	return -EIO;
}

ssize_t mock_hwinfo_zero_id(uint8_t *buf, size_t length)
{
	if (buf != NULL && length > 0) {
		memset(buf, 0, length);
	}
	return (ssize_t)length;
}

void mock_ble_reset(void)
{
	RESET_FAKE(bt_enable);
	RESET_FAKE(bt_le_adv_start);
	RESET_FAKE(bt_le_adv_stop);
	RESET_FAKE(bt_conn_cb_register);
	RESET_FAKE(bt_id_create);
	RESET_FAKE(z_impl_hwinfo_get_device_id);

	bt_enable_fake.custom_fake = mock_ble_enable_success;
	z_impl_hwinfo_get_device_id_fake.custom_fake = mock_hwinfo_zero_id;
}
