/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Unit test for app/src/ble_advertiser.c.
 *
 * Drives the public API against FFF fakes for bt_enable / bt_le_adv_*
 * and hwinfo_get_device_id. The default bt_enable fake invokes the
 * production module's ready callback synchronously so ble_advertiser_start
 * can take the bt_ready_sem without timing out.
 */

#include <zephyr/ztest.h>
#include <errno.h>

#include "ble_advertiser.h"
#include "mock_ble.h"

static struct ble_sensor_data fake_sensor_data = {
	.temperature = 2050,
	.humidity = 4500,
	.pressure = 10130,
	.eco2 = 420,
	.tvoc = 5,
	.battery_level = 87,
	.battery_charging = 0,
};

static void test_setup(void *fixture)
{
	ARG_UNUSED(fixture);
	mock_ble_reset();
}

ZTEST(ble_advertiser, test_init_success)
{
	int ret = ble_advertiser_init();
	zassert_ok(ret, "ble_advertiser_init: %d", ret);
	zassert_equal(bt_enable_fake.call_count, 1, "bt_enable not called");
	zassert_equal(bt_conn_cb_register_fake.call_count, 1,
		      "bt_conn_cb_register not called");
}

ZTEST(ble_advertiser, test_init_bt_enable_failure)
{
	bt_enable_fake.custom_fake = mock_ble_enable_failure;

	int ret = ble_advertiser_init();
	zassert_equal(ret, -EIO, "expected -EIO from bt_enable failure, got %d", ret);
	zassert_equal(bt_conn_cb_register_fake.call_count, 0,
		      "conn_cb_register should not run after bt_enable failure");
}

ZTEST(ble_advertiser, test_start_null_data)
{
	int ret = ble_advertiser_start(NULL);
	zassert_equal(ret, -EINVAL, "expected -EINVAL, got %d", ret);
	zassert_equal(bt_le_adv_start_fake.call_count, 0,
		      "adv_start should not be called for NULL data");
}

ZTEST(ble_advertiser, test_start_success_after_init)
{
	zassert_ok(ble_advertiser_init(), "init must succeed first");

	int ret = ble_advertiser_start(&fake_sensor_data);
	zassert_ok(ret, "ble_advertiser_start: %d", ret);
	zassert_true(bt_le_adv_start_fake.call_count >= 1,
		     "adv_start should have been called");
}

ZTEST(ble_advertiser, test_start_retries_on_eagain)
{
	zassert_ok(ble_advertiser_init(), "init must succeed first");

	/* First two attempts return EAGAIN, third succeeds. The production
	 * code retries up to 5 times with backoff.
	 */
	int return_seq[] = { -EAGAIN, -EAGAIN, 0 };
	SET_RETURN_SEQ(bt_le_adv_start, return_seq, ARRAY_SIZE(return_seq));

	int ret = ble_advertiser_start(&fake_sensor_data);
	zassert_ok(ret, "expected eventual success after EAGAIN retries, got %d", ret);
	zassert_equal(bt_le_adv_start_fake.call_count, 3,
		      "expected 3 adv_start attempts, got %u",
		      (unsigned)bt_le_adv_start_fake.call_count);
}

ZTEST(ble_advertiser, test_start_aborts_on_einval)
{
	zassert_ok(ble_advertiser_init(), "init must succeed first");

	bt_le_adv_start_fake.return_val = -EINVAL;

	int ret = ble_advertiser_start(&fake_sensor_data);
	zassert_equal(ret, -EINVAL, "expected EINVAL bubble-up, got %d", ret);
	zassert_equal(bt_le_adv_start_fake.call_count, 1,
		      "EINVAL should not retry, got %u attempts",
		      (unsigned)bt_le_adv_start_fake.call_count);
}

ZTEST(ble_advertiser, test_stop_idempotent)
{
	zassert_ok(ble_advertiser_init(), "init must succeed first");

	/* Two stops in a row must succeed; the production module short-circuits
	 * the second one when advertising_enabled is already false.
	 */
	zassert_ok(ble_advertiser_stop(), "first stop");
	uint32_t after_first = bt_le_adv_stop_fake.call_count;
	zassert_ok(ble_advertiser_stop(), "second stop");
	zassert_equal(bt_le_adv_stop_fake.call_count, after_first,
		      "second stop should be a no-op");
}

ZTEST(ble_advertiser, test_full_cycle_start_stop)
{
	zassert_ok(ble_advertiser_init(), "init");
	zassert_ok(ble_advertiser_start(&fake_sensor_data), "start");
	zassert_ok(ble_advertiser_stop(), "stop");
	zassert_true(bt_le_adv_stop_fake.call_count >= 1,
		     "stop should reach the BT layer once advertising was active");
}

ZTEST_SUITE(ble_advertiser, NULL, NULL, test_setup, NULL, NULL);
