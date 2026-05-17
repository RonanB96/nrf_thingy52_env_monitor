/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include "bt_gatt.h"
#include "uptime_service.h"

LOG_MODULE_REGISTER(test_uptime_service, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * Tests for uptime_service.c application logic:
 *   - init idempotency
 *   - update guard before init
 *   - getter unit conversion (ms -> s)
 *   - read callback wire format (little-endian uint64 seconds)
 *
 * `bt_gatt_attr_read` is faked via the shared mocks library so we can
 * intercept what the read callback hands to the Bluetooth host.
 *
 * The unit-under-test calls BT_GATT_SERVICE_DEFINE which expands to a
 * static `bt_gatt_attr` array we walk through to invoke the real
 * `read_uptime` callback the same way the GATT layer would.
 */

/* Forward declarations of the iterable section produced by
 * BT_GATT_SERVICE_DEFINE(uptime_svc, ...) inside the production file.
 * The attribute array is named `attr_uptime_svc`.
 */
extern const struct bt_gatt_attr attr_uptime_svc[];

/* Index of the value attribute (characteristic VALUE follows the CHRC
 * declaration attribute). Layout from uptime_service.c:
 *   [0] PRIMARY_SERVICE
 *   [1] CHARACTERISTIC declaration
 *   [2] characteristic VALUE — read callback installed here
 *   [3] CUD descriptor
 */
#define UPTIME_VALUE_ATTR_IDX 2

static ssize_t capture_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			    uint16_t buf_len, uint16_t offset, const void *value,
			    uint16_t value_len)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);

	if (buf != NULL && value != NULL) {
		uint16_t copy_len = MIN(buf_len, value_len);
		memcpy(buf, value, copy_len);
	}
	return (ssize_t)value_len;
}

static void before_each(void *f)
{
	ARG_UNUSED(f);
	BT_GATT_FFF_FAKES_LIST(RESET_FAKE);
	bt_gatt_attr_read_fake.custom_fake = capture_read;
}

ZTEST_SUITE(uptime_service, NULL, NULL, before_each, NULL, NULL);

ZTEST(uptime_service, test_a_update_returns_enodev_before_init)
{
	int ret = uptime_service_update();

	zassert_equal(ret, -ENODEV, "Expected -ENODEV before init, got %d", ret);
}

ZTEST(uptime_service, test_init_succeeds)
{
	int ret = uptime_service_init();

	zassert_equal(ret, 0, "init must succeed, got %d", ret);
}

ZTEST(uptime_service, test_init_is_idempotent)
{
	zassert_equal(uptime_service_init(), 0, "first init must succeed");
	zassert_equal(uptime_service_init(), 0, "second init must succeed");
}

ZTEST(uptime_service, test_update_after_init_succeeds)
{
	zassert_equal(uptime_service_init(), 0, "init prerequisite");

	int ret = uptime_service_update();

	zassert_equal(ret, 0, "update must succeed after init, got %d", ret);
}

/*
 * The function under test is `return k_uptime_get() / 1000ULL`. The only piece
 * of MY code is the divisor that converts ms to s. Take two readings ~1100 ms
 * apart and assert the delta is in {1, 2} seconds — anything else (e.g. ~1100,
 * meaning the divisor was lost) proves the unit conversion broke. We do NOT
 * cross-check against k_uptime_get directly; that would just be testing Zephyr.
 */
ZTEST(uptime_service, test_get_uptime_seconds_returns_seconds_not_ms)
{
	zassert_equal(uptime_service_init(), 0, "init prerequisite");

	uint64_t first = uptime_service_get_uptime_seconds();
	k_sleep(K_MSEC(1100));
	uint64_t second = uptime_service_get_uptime_seconds();

	uint64_t delta = second - first;
	zassert_true(delta >= 1ULL && delta <= 2ULL,
		     "expected 1–2 s after 1100 ms sleep, got %llu (divisor wrong?)",
		     (unsigned long long)delta);
}

/* Read callback contract: the production code allocates a local LE-encoded
 * uint64 and passes it through bt_gatt_attr_read; assert size and encoding.
 */
ZTEST(uptime_service, test_read_callback_emits_le_uint64_seconds)
{
	zassert_equal(uptime_service_init(), 0, "init prerequisite");

	uint8_t buf[sizeof(uint64_t)] = {0};
	const struct bt_gatt_attr *value_attr = &attr_uptime_svc[UPTIME_VALUE_ATTR_IDX];

	zassert_not_null(value_attr->read, "value attribute must have read callback");

	ssize_t n = value_attr->read(NULL, value_attr, buf, sizeof(buf), 0);

	zassert_equal(n, (ssize_t)sizeof(uint64_t),
		      "read should return sizeof(uint64_t), got %zd", n);
	zassert_equal(bt_gatt_attr_read_fake.call_count, 1,
		      "bt_gatt_attr_read must be called exactly once, got %u",
		      bt_gatt_attr_read_fake.call_count);
	zassert_equal(bt_gatt_attr_read_fake.arg6_val, sizeof(uint64_t),
		      "value_len arg must be 8, got %u",
		      bt_gatt_attr_read_fake.arg6_val);

	/* The wire-format contract for MY code: the value handed to bt_gatt_attr_read
	 * is the LE-encoded uptime in seconds. Decode the captured buffer and assert
	 * it equals what uptime_service_get_uptime_seconds() returns at this instant
	 * (called immediately after, so any drift is bounded by a single tick).
	 */
	uint64_t le;
	memcpy(&le, buf, sizeof(le));
	uint64_t decoded = sys_le64_to_cpu(le);
	uint64_t now = uptime_service_get_uptime_seconds();

	zassert_true(decoded == now || decoded + 1ULL == now,
		     "wire-encoded seconds %llu does not match getter %llu",
		     (unsigned long long)decoded, (unsigned long long)now);
}
