/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "mock_battery_service.h"
#include "mock_bluetooth.h"
#include "ble_battery_service.h"

LOG_MODULE_REGISTER(test_battery_service, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * Tests for ble_battery_service.c application logic:
 *   - charge level classification (good/low/critical thresholds)
 *   - charge state transitions
 *   - level clamping (>100 -> 100)
 *   - deduplication
 *   - deferred init when hardware not ready
 *
 * battery_service_* faked via FFF (mock_battery_service.h/c).
 * bt_bas_* faked via FFF (mock_bluetooth.h/c).
 *
 * Hardware integration tests: app/tests/hardware/src/test_hil_battery.c
 */

static void before_each(void *f)
{
	ARG_UNUSED(f);
	BATTERY_SERVICE_FFF_FAKES_LIST(RESET_FAKE);
	BT_BAS_FFF_FAKES_LIST(RESET_FAKE);
	/* Default: hardware not ready */
	battery_service_get_level_fake.return_val = -ENODATA;
}

ZTEST_SUITE(battery_service, NULL, NULL, before_each, NULL, NULL);

/* Named with a_ prefix so they sort first and run before any init call.
 * ble_battery_service has no deinit so the initialized flag cannot be reset. */
ZTEST(battery_service, test_a_guard_update_manual_not_initialized)
{
	int ret = ble_battery_service_update_manual(50, false);

	zassert_equal(ret, -ENODEV, "Expected -ENODEV before init, got %d", ret);
	zassert_equal(bt_bas_set_battery_level_fake.call_count, 0,
		      "BAS must not be written before init");
}

ZTEST(battery_service, test_a_guard_update_not_initialized)
{
	int ret = ble_battery_service_update();

	zassert_equal(ret, -ENODEV, "Expected -ENODEV before init, got %d", ret);
}

ZTEST(battery_service, test_a_guard_set_fault_not_initialized)
{
	int ret = ble_battery_service_set_fault(true);

	zassert_equal(ret, -ENODEV, "Expected -ENODEV before init, got %d", ret);
	zassert_equal(bt_bas_bls_set_battery_fault_fake.call_count, 0,
		      "BAS fault must not be written before init");
}

ZTEST(battery_service, test_a_guard_set_service_required_not_initialized)
{
	int ret = ble_battery_service_set_service_required(true);

	zassert_equal(ret, -ENODEV, "Expected -ENODEV before init, got %d", ret);
	zassert_equal(bt_bas_bls_set_service_required_fake.call_count, 0,
		      "BAS service-required must not be written before init");
}

ZTEST(battery_service, test_a_get_level_not_initialized_returns_zero)
{
	zassert_equal(ble_battery_service_get_level(), 0,
		      "Cached level must be 0 before init");
}

ZTEST(battery_service, test_init_valid_battery)
{
	battery_service_get_level_fake.return_val = 80;
	battery_service_is_charging_fake.return_val = false;

	int ret = ble_battery_service_init();

	zassert_equal(ret, 0, "Init must succeed");
	zassert_equal(ble_battery_service_get_level(), 80, "Level must be 80 after init");
	zassert_false(ble_battery_service_is_charging(), "Must not be charging");
	zassert_equal(bt_bas_set_battery_level_fake.arg0_val, 80, "BAS must receive level 80");
	zassert_true(bt_bas_set_battery_level_fake.call_count > 0,
		     "bt_bas_set_battery_level must have been called");
}

ZTEST(battery_service, test_init_deferred_when_hardware_not_ready)
{
	int ret = ble_battery_service_init();

	zassert_equal(ret, 0, "Deferred init must still return 0");
	zassert_equal(bt_bas_set_battery_level_fake.call_count, 0,
		      "BAS level must not be set when hardware is not ready");
	zassert_equal(battery_service_register_charging_callback_fake.call_count, 1,
		      "Charging callback must be registered on deferred init");
	zassert_not_null(battery_service_register_charging_callback_fake.arg0_val,
			 "Registered callback must not be NULL");
}

ZTEST(battery_service, test_charge_level_thresholds)
{
	battery_service_get_level_fake.return_val = 60;
	ble_battery_service_init();

	ble_battery_service_update_manual(80, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_GOOD, "80%% must be GOOD");

	ble_battery_service_update_manual(50, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_LOW, "50%% must be LOW");

	ble_battery_service_update_manual(10, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_CRITICAL, "10%% must be CRITICAL");
}

ZTEST(battery_service, test_charge_level_threshold_boundaries)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	ble_battery_service_update_manual(76, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_GOOD, "76%% must be GOOD");

	ble_battery_service_update_manual(75, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_LOW, "75%% must be LOW");

	ble_battery_service_update_manual(26, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_LOW, "26%% must be LOW");

	ble_battery_service_update_manual(25, false);
	zassert_equal(bt_bas_bls_set_battery_charge_level_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_LEVEL_CRITICAL, "25%% must be CRITICAL");
}

ZTEST(battery_service, test_update_manual_clamps_level_to_100)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	ble_battery_service_update_manual(150, false);

	zassert_equal(ble_battery_service_get_level(), 100, "Internal state must clamp to 100");
	zassert_equal(bt_bas_set_battery_level_fake.arg0_val, 100,
		      "BAS must receive clamped level 100, not 150");
}

ZTEST(battery_service, test_update_manual_deduplication)
{
	battery_service_get_level_fake.return_val = 60;
	ble_battery_service_init();

	int count_after_init = bt_bas_set_battery_level_fake.call_count;

	ble_battery_service_update_manual(70, false);
	zassert_equal(bt_bas_set_battery_level_fake.call_count, count_after_init + 1,
		      "First distinct update must write BAS once");

	int ret = ble_battery_service_update_manual(70, false);

	zassert_equal(ret, 0, "Duplicate update must return 0");
	zassert_equal(bt_bas_set_battery_level_fake.call_count, count_after_init + 1,
		      "Duplicate update must not call bt_bas_set_battery_level again");
}

ZTEST(battery_service, test_charging_state_transitions)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	ble_battery_service_update_manual(50, true);
	zassert_equal(bt_bas_bls_set_battery_charge_state_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_STATE_CHARGING, "Charging must set state to CHARGING");
	zassert_equal(bt_bas_bls_set_wired_external_power_source_fake.arg0_val,
		      BT_BAS_BLS_WIRED_POWER_CONNECTED, "Charging must set wired power CONNECTED");

	ble_battery_service_update_manual(50, false);
	zassert_equal(bt_bas_bls_set_battery_charge_state_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_STATE_DISCHARGING_ACTIVE,
		      "Discharging above critical must be DISCHARGING_ACTIVE");
	zassert_equal(bt_bas_bls_set_wired_external_power_source_fake.arg0_val,
		      BT_BAS_BLS_WIRED_POWER_NOT_CONNECTED,
		      "Not charging must set wired power NOT_CONNECTED");
}

ZTEST(battery_service, test_discharging_inactive_at_critical_level)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	ble_battery_service_update_manual(10, false);
	zassert_equal(bt_bas_bls_set_battery_charge_state_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_STATE_DISCHARGING_INACTIVE,
		      "Discharging at critical level must be DISCHARGING_INACTIVE");
}

/* ---- live update path: ble_battery_service_update() ---- */

ZTEST(battery_service, test_update_reads_hardware_and_writes_bas)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	int count_before = bt_bas_set_battery_level_fake.call_count;

	/* Simulate a fresh hardware reading. */
	battery_service_get_level_fake.return_val = 42;
	battery_service_is_charging_fake.return_val = true;

	int ret = ble_battery_service_update();

	zassert_equal(ret, 0, "Live update must succeed, got %d", ret);
	zassert_equal(bt_bas_set_battery_level_fake.call_count, count_before + 1,
		      "Live update must call bt_bas_set_battery_level once");
	zassert_equal(bt_bas_set_battery_level_fake.arg0_val, 42,
		      "BAS level must reflect the freshly read hardware value");
	zassert_equal(ble_battery_service_get_level(), 42, "Cached level must match hardware");
}

ZTEST(battery_service, test_update_propagates_hardware_read_error)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	int count_before = bt_bas_set_battery_level_fake.call_count;

	battery_service_get_level_fake.return_val = -EIO;

	int ret = ble_battery_service_update();

	zassert_equal(ret, -EIO, "Update must propagate hardware read error, got %d", ret);
	zassert_equal(bt_bas_set_battery_level_fake.call_count, count_before,
		      "BAS must not be written when hardware read fails");
}

/* ---- BAS write failure during update_manual is propagated ---- */

ZTEST(battery_service, test_update_manual_propagates_bas_write_error)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	bt_bas_set_battery_level_fake.return_val = -EIO;

	/* Use a level distinct from the cached value to avoid the dedup short-circuit. */
	int ret = ble_battery_service_update_manual(33, false);

	zassert_equal(ret, -EIO, "BAS write failure must be propagated, got %d", ret);

	/* Restore success for any later tests. */
	bt_bas_set_battery_level_fake.return_val = 0;
}

/* ---- BAS retry loop on init when bt_bas_set_battery_level fails ---- */

ZTEST(battery_service, test_init_bas_set_failure_retries_then_continues)
{
	battery_service_get_level_fake.return_val = 60;
	bt_bas_set_battery_level_fake.return_val = -EIO; /* always fail to exhaust retries */

	int ret = ble_battery_service_init();

	zassert_equal(ret, 0, "Init must succeed in degraded mode even on BAS failure, got %d",
		      ret);
	zassert_equal(bt_bas_set_battery_level_fake.call_count, 3,
		      "Init must retry bt_bas_set_battery_level up to 3 times, got %u",
		      bt_bas_set_battery_level_fake.call_count);

	bt_bas_set_battery_level_fake.return_val = 0;
}

/* ---- charging-status callback path ---- */

ZTEST(battery_service, test_charging_callback_updates_state_and_bas)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	battery_charging_cb_t cb = battery_service_register_charging_callback_fake.arg0_val;

	zassert_not_null(cb, "Charging callback must be registered during init");

	int state_count_before = bt_bas_bls_set_battery_charge_state_fake.call_count;

	/*
	 * Invoke the callback that ble_battery_service registered with the HAL.
	 * The callback re-reads from battery_service via ble_battery_service_update(),
	 * so the hardware fakes must agree with the callback's intent. Use a level
	 * distinct from the cached one so update_manual's dedup does not skip the
	 * BAS write.
	 */
	battery_service_get_level_fake.return_val = 51;
	battery_service_is_charging_fake.return_val = true;
	cb(true);

	zassert_true(ble_battery_service_is_charging(),
		     "Cached charging flag must reflect the callback");
	zassert_true(bt_bas_bls_set_battery_charge_state_fake.call_count > state_count_before,
		     "Callback must propagate a state update to BAS");
	zassert_equal(bt_bas_bls_set_battery_charge_state_fake.arg0_val,
		      BT_BAS_BLS_CHARGE_STATE_CHARGING,
		      "Charging callback must set state to CHARGING");

	battery_service_get_level_fake.return_val = 52;
	battery_service_is_charging_fake.return_val = false;
	cb(false);

	zassert_false(ble_battery_service_is_charging(),
		      "Cached charging flag must clear when callback fires false");
}

/* ---- set_fault and set_service_required happy paths ---- */

ZTEST(battery_service, test_set_fault_writes_bas)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	int ret = ble_battery_service_set_fault(true);

	zassert_equal(ret, 0, "set_fault(true) must succeed");
	zassert_true(bt_bas_bls_set_battery_fault_fake.call_count > 0,
		     "set_fault must write BAS fault");
	zassert_equal(bt_bas_bls_set_battery_fault_fake.arg0_val, BT_BAS_BLS_BATTERY_FAULT_YES,
		      "set_fault(true) must map to BATTERY_FAULT_YES");

	ret = ble_battery_service_set_fault(false);
	zassert_equal(ret, 0, "set_fault(false) must succeed");
	zassert_equal(bt_bas_bls_set_battery_fault_fake.arg0_val, BT_BAS_BLS_BATTERY_FAULT_NO,
		      "set_fault(false) must map to BATTERY_FAULT_NO");
}

ZTEST(battery_service, test_set_service_required_writes_bas)
{
	battery_service_get_level_fake.return_val = 50;
	ble_battery_service_init();

	int ret = ble_battery_service_set_service_required(true);

	zassert_equal(ret, 0, "set_service_required(true) must succeed");
	zassert_true(bt_bas_bls_set_service_required_fake.call_count > 0,
		     "set_service_required must write BAS");
	zassert_equal(bt_bas_bls_set_service_required_fake.arg0_val,
		      BT_BAS_BLS_SERVICE_REQUIRED_TRUE,
		      "set_service_required(true) must map to SERVICE_REQUIRED_TRUE");

	ret = ble_battery_service_set_service_required(false);
	zassert_equal(ret, 0, "set_service_required(false) must succeed");
	zassert_equal(bt_bas_bls_set_service_required_fake.arg0_val,
		      BT_BAS_BLS_SERVICE_REQUIRED_FALSE,
		      "set_service_required(false) must map to SERVICE_REQUIRED_FALSE");
}
